import { afterEach, beforeEach, describe, it, expect, vi } from 'vitest';
import { armTurn, cancelTurn, hydrate } from '@/store/matchSlice';
import { sessionStarted, sessionStopped } from '@/store/mqttSlice';
import { makeStore } from '@/store/store';
import { LOCAL_AUDIT_QUEUE_KEY, LOCAL_STATE_KEY, resolveInitialMatch } from '@/store/syncMiddleware';
import { buildCard, buildState } from './matchFixtures';
import type { AuditEntry, SyncTransport } from '@/store/syncMiddleware';
import type { MatchState } from '@/lib/match/types';

function createFakeTransport() {
  const saved: MatchState[] = [];
  const audit = new Map<string, AuditEntry>();
  let failuresLeft = 0;

  const transport: SyncTransport = {
    saveMatchState: async (state) => {
      if (failuresLeft > 0) {
        failuresLeft -= 1;
        throw new Error('offline');
      }
      saved.push(state);
    },
    appendAudit: async (entries) => {
      if (failuresLeft > 0) {
        failuresLeft -= 1;
        throw new Error('offline');
      }
      for (const entry of entries) {
        audit.set(entry.actionId, entry);
      }
    }
  };

  return {
    transport,
    saved,
    audit,
    failNext: (count: number) => {
      failuresLeft = count;
    }
  };
}

beforeEach(() => {
  vi.useFakeTimers();
  localStorage.clear();
});

afterEach(() => {
  vi.useRealTimers();
});

describe('sync middleware', () => {
  it('pushes a result immediately and logs one audit row per action', async () => {
    const fake = createFakeTransport();
    const store = makeStore({ sync: fake.transport });
    store.dispatch(hydrate(buildState([buildCard({ id: 'a' })])));
    store.dispatch(armTurn({ cardId: 'a', round: 1 }));
    store.dispatch(sessionStarted({ sessionId: 1, timestamp: 0, startDelaySeconds: 0 }));
    store.dispatch(sessionStopped({ sessionId: 1, timestamp: 1, lastShotTimeMs: 4200 }));

    await vi.advanceTimersByTimeAsync(0);

    expect(fake.saved.at(-1)?.cards[0].rounds[0]).toMatchObject({ status: 'timed', timeMs: 4200 });
    expect([...fake.audit.values()].map(entry => entry.type)).toEqual(['match/armTurn', 'match/turnStarted', 'match/turnStopped']);
    expect(store.getState().match.syncStatus).toBe('synced');
  });

  it('debounces non-result changes', async () => {
    const fake = createFakeTransport();
    const store = makeStore({ sync: fake.transport });
    store.dispatch(hydrate(buildState([buildCard({ id: 'a' })])));
    await vi.advanceTimersByTimeAsync(600);
    const savesAfterHydrate = fake.saved.length;

    store.dispatch(armTurn({ cardId: 'a', round: 1 }));
    store.dispatch(cancelTurn());

    expect(fake.saved.length).toBe(savesAfterHydrate);

    await vi.advanceTimersByTimeAsync(600);

    expect(fake.saved.length).toBe(savesAfterHydrate + 1);
  });

  it('keeps every change in localStorage and retries until the server is back, without duplicates', async () => {
    const fake = createFakeTransport();
    const store = makeStore({ sync: fake.transport });
    fake.failNext(3);
    store.dispatch(hydrate(buildState([buildCard({ id: 'a' })])));
    store.dispatch(armTurn({ cardId: 'a', round: 1 }));
    store.dispatch(sessionStopped({ sessionId: 1, timestamp: 1, lastShotTimeMs: 1000 }));
    await vi.advanceTimersByTimeAsync(0);

    expect(store.getState().match.syncStatus).toBe('error');
    expect(JSON.parse(localStorage.getItem(LOCAL_STATE_KEY) ?? 'null').cards[0].rounds[0].timeMs).toBe(1000);
    expect(JSON.parse(localStorage.getItem(LOCAL_AUDIT_QUEUE_KEY) ?? '[]')).toHaveLength(2);

    await vi.advanceTimersByTimeAsync(10000);

    expect(store.getState().match.syncStatus).toBe('synced');
    expect(fake.audit.size).toBe(2);
    expect(JSON.parse(localStorage.getItem(LOCAL_AUDIT_QUEUE_KEY) ?? '[]')).toEqual([]);
    expect(fake.saved.at(-1)?.cards[0].rounds[0].timeMs).toBe(1000);
  });

  it('keeps a result with nothing armed as unassigned', async () => {
    const fake = createFakeTransport();
    const store = makeStore({ sync: fake.transport });
    store.dispatch(hydrate(buildState([buildCard({ id: 'a' })])));
    store.dispatch(sessionStopped({ sessionId: 1, timestamp: 1, lastShotTimeMs: 3100 }));

    expect(store.getState().match.current?.unassignedResults.map(result => result.timeMs)).toEqual([3100]);
  });

  it('never persists from a store without sync (the /display store)', async () => {
    const store = makeStore();
    store.dispatch(hydrate(buildState([buildCard({ id: 'a' })])));
    store.dispatch(armTurn({ cardId: 'a', round: 1 }));
    await vi.advanceTimersByTimeAsync(1000);

    expect(localStorage.getItem(LOCAL_STATE_KEY)).toBeNull();
  });
});

describe('resolveInitialMatch', () => {
  const fresh = () => buildState([], { revision: -1 });

  it('prefers the local copy of the same match', () => {
    const local = buildState([], { revision: 7 });
    const server = buildState([], { revision: 3 });

    expect(resolveInitialMatch('match-1', local, server, fresh).revision).toBe(7);
  });

  it('ignores a local copy of another match', () => {
    const local = buildState([], { matchId: 'old', revision: 7 });
    const server = buildState([], { revision: 3 });

    expect(resolveInitialMatch('match-1', local, server, fresh).revision).toBe(3);
  });

  it('builds a fresh state when there is nothing saved', () => {
    expect(resolveInitialMatch('match-1', null, null, fresh).revision).toBe(-1);
  });
});
