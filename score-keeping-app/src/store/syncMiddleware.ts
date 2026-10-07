import { storage } from '@/lib/display/utils';
import type { MatchState } from '@/lib/match/types';
import { hydrate, isMatchChange, isResultAction, syncStatusChanged } from './matchSlice';
import type { AuditEntry, MatchRootState, MaybeMetaAction, MetaAction, SyncOptions, SyncStatus, SyncStorage } from './types';
import type { Middleware } from '@reduxjs/toolkit';

export const LOCAL_STATE_KEY = 'matchState';
export const LOCAL_AUDIT_QUEUE_KEY = 'matchAuditQueue';

const DEFAULT_DEBOUNCE_MS = 500;
const DEFAULT_MAX_RETRY_DELAY_MS = 30000;
const FIRST_RETRY_DELAY_MS = 1000;

/**
 * Persists every match change to localStorage synchronously, so a reload never loses work,
 * then to the server: results immediately, the rest debounced, retrying with backoff.
 */
export function createSyncMiddleware({
  transport,
  localStore = storage,
  debounceMs = DEFAULT_DEBOUNCE_MS,
  maxRetryDelayMs = DEFAULT_MAX_RETRY_DELAY_MS
}: SyncOptions): Middleware<object, MatchRootState> {
  return (api) => {
    let debounceTimer: ReturnType<typeof setTimeout> | undefined;
    let retryTimer: ReturnType<typeof setTimeout> | undefined;
    let retryDelayMs = FIRST_RETRY_DELAY_MS;
    let isFlushing = false;
    let isFlushRequested = false;
    let isSnapshotDirty = false;

    function setStatus(status: SyncStatus) {
      if (api.getState().match.syncStatus !== status) {
        api.dispatch(syncStatusChanged(status));
      }
    }

    function scheduleFlush(delayMs: number) {
      clearTimeout(debounceTimer);
      debounceTimer = setTimeout(flush, delayMs);
    }

    function scheduleRetry() {
      clearTimeout(retryTimer);
      retryTimer = setTimeout(flush, retryDelayMs);
      retryDelayMs = Math.min(retryDelayMs * 2, maxRetryDelayMs);
    }

    async function pushPending() {
      const state = api.getState().match.current;
      if (isSnapshotDirty && state) {
        isSnapshotDirty = false;
        try {
          await transport.saveMatchState(state);
        } catch (error) {
          isSnapshotDirty = true;
          throw error;
        }
      }

      const queue = localStore.get<AuditEntry[]>(LOCAL_AUDIT_QUEUE_KEY, []);
      if (queue.length > 0) {
        await transport.appendAudit(queue);

        const sentIds = new Set(queue.map(entry => entry.actionId));
        const remaining = localStore.get<AuditEntry[]>(LOCAL_AUDIT_QUEUE_KEY, []).filter(entry => !sentIds.has(entry.actionId));
        localStore.set(LOCAL_AUDIT_QUEUE_KEY, remaining);
      }
    }

    async function flush() {
      clearTimeout(debounceTimer);
      clearTimeout(retryTimer);

      if (isFlushing) {
        isFlushRequested = true;

        return;
      }

      isFlushing = true;
      try {
        await pushPending();
        retryDelayMs = FIRST_RETRY_DELAY_MS;
      } catch (error) {
        console.error('[sync] push to server failed — retrying', error);
        setStatus('error');
        scheduleRetry();

        return;
      } finally {
        isFlushing = false;
      }

      const hasQueuedAudit = localStore.get<AuditEntry[]>(LOCAL_AUDIT_QUEUE_KEY, []).length > 0;
      if (isFlushRequested || isSnapshotDirty || hasQueuedAudit) {
        isFlushRequested = false;
        await flush();

        return;
      }

      setStatus('synced');
    }

    function enqueueAudit(entry: AuditEntry) {
      const queue = localStore.get<AuditEntry[]>(LOCAL_AUDIT_QUEUE_KEY, []);
      localStore.set(LOCAL_AUDIT_QUEUE_KEY, [...queue, entry]);
    }

    return next => (action) => {
      const result = next(action);

      const isHydrate = hydrate.match(action);
      const isChange = isMatchChange(action as MaybeMetaAction);
      if (!isHydrate && !isChange) {
        return result;
      }

      const state = api.getState().match.current;
      if (!state) {
        return result;
      }

      localStore.set(LOCAL_STATE_KEY, state);
      isSnapshotDirty = true;

      if (isChange) {
        const changeAction = action as MetaAction<AuditEntry['payload']>;
        enqueueAudit({
          actionId: changeAction.meta.id,
          matchId: state.matchId,
          at: changeAction.meta.at,
          type: changeAction.type,
          payload: changeAction.payload
        });
      }

      setStatus('dirty');

      if (isResultAction(action)) {
        flush();
      } else {
        scheduleFlush(debounceMs);
      }

      return result;
    };
  };
}

/** Browser is leading: a local copy of the active match beats the server's, which beats a fresh start. */
export function resolveInitialMatch(
  matchId: string,
  local: MatchState | null,
  server: MatchState | null,
  buildFresh: () => MatchState
): MatchState {
  if (local && local.matchId === matchId) {
    return local;
  }
  if (server) {
    return server;
  }

  return buildFresh();
}

export function getLocalMatchState(localStore: SyncStorage = storage): MatchState | null {
  return localStore.get<MatchState | null>(LOCAL_STATE_KEY, null);
}
