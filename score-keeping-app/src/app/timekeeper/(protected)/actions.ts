'use server';

import { headers as getHeaders } from 'next/headers';
import { getPayload } from 'payload';
import config from '@/payload.config';
import type { MatchState } from '@/lib/match/types';
import type { AuditEntry } from '@/store/syncMiddleware';

async function getAuthenticatedPayload() {
  const payload = await getPayload({ config });
  const { user } = await payload.auth({ headers: await getHeaders() });
  if (!user) {
    throw new Error('Not logged in.');
  }

  return { payload, user };
}

/** Overwrites the server's backup of the match — the browser is leading, so there is no conflict check. */
export async function saveMatchState(state: MatchState): Promise<void> {
  const { payload } = await getAuthenticatedPayload();

  const existing = await payload.find({
    collection: 'match-states',
    where: { match: { equals: state.matchId } },
    depth: 0,
    limit: 1
  });
  const data = { match: state.matchId, revision: state.revision, state: { ...state } };

  if (existing.docs[0]) {
    await payload.update({ collection: 'match-states', id: existing.docs[0].id, data });

    return;
  }

  await payload.create({ collection: 'match-states', data });
}

/** Idempotent: entries already stored (a retry after a lost response) are skipped. */
export async function appendAudit(entries: AuditEntry[]): Promise<void> {
  const { payload, user } = await getAuthenticatedPayload();
  if (entries.length === 0) {
    return;
  }

  const stored = await payload.find({
    collection: 'match-audit',
    where: { actionId: { in: entries.map(entry => entry.actionId) } },
    depth: 0,
    pagination: false,
    select: { actionId: true }
  });
  const storedIds = new Set(stored.docs.map(doc => doc.actionId));

  for (const entry of entries) {
    if (storedIds.has(entry.actionId)) {
      continue;
    }

    await payload.create({
      collection: 'match-audit',
      data: {
        actionId: entry.actionId,
        match: entry.matchId,
        at: entry.at,
        type: entry.type,
        payload: entry.payload
      },
      user,
      overrideAccess: false
    });
  }
}
