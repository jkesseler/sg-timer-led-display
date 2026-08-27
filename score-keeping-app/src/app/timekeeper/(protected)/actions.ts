'use server';

import { getPayload } from 'payload';
import { revalidatePath } from 'next/cache';
import config from '@/payload.config';
import { loadSquadView } from '@/lib/match/loadSquadView';
import { resolveSquadDeviceId, type MembershipView } from '@/lib/match/matchState';

export interface ActionResult {
  ok: boolean;
  error?: string;
}

/** Resolves whether a fresh activation for this membership should target the next pending round or a deferred reshoot. */
function resolveActivationTarget(view: MembershipView): { roundResult: number } | { reshootFor: number } | null {
  const pendingRound = view.roundResults.find(r => r.status === 'pending');
  if (pendingRound) {
    return { roundResult: pendingRound.id };
  }

  const hasUnresolvedRs = view.roundResults.some(r => r.status === 'rs') && !view.membership.reshootTimeMs;
  if (hasUnresolvedRs) {
    return { reshootFor: view.membership.id };
  }

  return null;
}

async function activate(squadId: number, membershipId: number): Promise<ActionResult> {
  const payload = await getPayload({ config });
  const view = await loadSquadView(squadId);

  const deviceId = resolveSquadDeviceId(view.squad);
  if (deviceId == null) {
    return { ok: false, error: 'This squad has no timer device assigned (set one on its match).' };
  }
  if (view.isSessionActive) {
    return { ok: false, error: 'A turn is already in progress on this device.' };
  }

  const membershipView = view.memberships.find(m => m.membership.id === membershipId);
  if (!membershipView) {
    return { ok: false, error: 'Unknown shooter for this squad.' };
  }
  if (membershipView.membership.status !== 'present') {
    return { ok: false, error: 'This shooter is not marked present.' };
  }

  const target = resolveActivationTarget(membershipView);
  if (!target) {
    return { ok: false, error: 'This shooter has nothing outstanding to shoot.' };
  }

  try {
    await payload.create({
      collection: 'match-sessions',
      data: { device: deviceId, status: 'pending', ...target }
    });
  } catch (error) {
    return { ok: false, error: error instanceof Error ? error.message : 'Could not start the turn.' };
  }

  revalidatePath('/timekeeper');

  return { ok: true };
}

export async function activateMembershipAction(squadId: number, membershipId: number): Promise<ActionResult> {
  return activate(squadId, membershipId);
}

/**
 * Undoes an activation — an accidental click/scan, or a genuine
 * mid-round problem (matching the plan's "Abandon Turn" case: no
 * automatic timeout, an explicit action instead). Leaves the target
 * round-result untouched (still pending) so the shooter can be
 * re-activated; any shots already fired for this MQTT session simply
 * won't be attributed to anyone once it stops, since the server
 * subscriber no longer finds a live match-session to bind them to.
 */
export async function cancelActivationAction(squadId: number): Promise<ActionResult> {
  const payload = await getPayload({ config });
  const view = await loadSquadView(squadId);

  const liveSession = view.liveSessions[0];
  if (!liveSession) {
    return { ok: false, error: 'No active turn to cancel.' };
  }

  await payload.update({
    collection: 'match-sessions',
    id: liveSession.id,
    data: { status: 'abandoned', stoppedAtMs: Date.now() }
  });

  revalidatePath('/timekeeper');

  return { ok: true };
}

export async function activateByKnsaAction(squadId: number, rawCode: string): Promise<ActionResult> {
  const code = rawCode.trim();
  if (!code) {
    return { ok: false, error: 'Enter or scan a KNSA number.' };
  }

  const payload = await getPayload({ config });
  const shooters = await payload.find({
    collection: 'shooters',
    where: { knsaNumber: { equals: code } },
    limit: 1
  });
  const shooter = shooters.docs[0];
  if (!shooter) {
    return { ok: false, error: `No shooter found for code "${code}".` };
  }

  const view = await loadSquadView(squadId);
  const membershipView = view.memberships.find((m) => {
    const shooterId = typeof m.membership.shooter === 'object' ? m.membership.shooter.id : m.membership.shooter;

    return shooterId === shooter.id;
  });
  if (!membershipView) {
    return { ok: false, error: `${shooter.firstName} ${shooter.lastName} is not in this squad.` };
  }

  return activate(squadId, membershipView.membership.id);
}

async function assertNotSessionActive(squadId: number): Promise<ActionResult | null> {
  const view = await loadSquadView(squadId);
  if (view.isSessionActive) {
    return { ok: false, error: 'Cannot change the queue while a turn is in progress.' };
  }

  return null;
}

/** Drag-and-drop reorder: orderedMembershipIds is the full new front-to-back order of present shooters. */
export async function reorderQueueAction(squadId: number, orderedMembershipIds: number[]): Promise<ActionResult> {
  const guardError = await assertNotSessionActive(squadId);
  if (guardError) {
    return guardError;
  }

  const payload = await getPayload({ config });
  await Promise.all(
    orderedMembershipIds.map((membershipId, index) =>
      payload.update({
        collection: 'squad-memberships',
        id: membershipId,
        data: { queuePosition: index + 1 }
      })
    )
  );

  revalidatePath('/timekeeper');

  return { ok: true };
}

export async function markAbsentAction(squadId: number, membershipId: number): Promise<ActionResult> {
  const guardError = await assertNotSessionActive(squadId);
  if (guardError) {
    return guardError;
  }

  const payload = await getPayload({ config });
  const view = await loadSquadView(squadId);
  const membershipView = view.memberships.find(m => m.membership.id === membershipId);

  await payload.update({ collection: 'squad-memberships', id: membershipId, data: { status: 'absent' } });

  for (const result of membershipView?.roundResults ?? []) {
    if (result.status === 'pending') {
      await payload.update({ collection: 'round-results', id: result.id, data: { status: 'skipped' } });
    }
  }

  revalidatePath('/timekeeper');

  return { ok: true };
}

export async function markPresentAction(squadId: number, membershipId: number): Promise<ActionResult> {
  const guardError = await assertNotSessionActive(squadId);
  if (guardError) {
    return guardError;
  }

  const payload = await getPayload({ config });
  const view = await loadSquadView(squadId);
  const maxPosition = Math.max(0, ...view.memberships.map(m => m.membership.queuePosition));

  // Deliberately leaves any already-skipped rounds as skipped rather than
  // reviving them to pending — reviving them would reopen the main
  // rotation's current round for everyone (deriveCurrentRound takes the
  // lowest pending round across all present members). Rounds skipped
  // before this rejoin stay deferred to the outstanding-items/catch-up
  // phase at the end; only rounds from here forward flow through the
  // normal rotation.
  await payload.update({
    collection: 'squad-memberships',
    id: membershipId,
    data: { status: 'present', queuePosition: maxPosition + 1 }
  });

  revalidatePath('/timekeeper');

  return { ok: true };
}

/** Offers one specific skipped (catch-up) round during the outstanding-items phase: revives it to pending, then activates it. */
export async function offerCatchUpAction(squadId: number, roundResultId: number): Promise<ActionResult> {
  const guardError = await assertNotSessionActive(squadId);
  if (guardError) {
    return guardError;
  }

  const payload = await getPayload({ config });
  const roundResult = await payload.findByID({ collection: 'round-results', id: roundResultId });
  if (roundResult.status !== 'skipped') {
    return { ok: false, error: 'This round is not an outstanding catch-up item.' };
  }

  const membershipId = typeof roundResult.membership === 'object' ? roundResult.membership.id : roundResult.membership;
  await payload.update({ collection: 'round-results', id: roundResultId, data: { status: 'pending' } });

  return activate(squadId, membershipId);
}

export async function flagMalfunctionAction(squadId: number, roundResultId: number): Promise<ActionResult> {
  const guardError = await assertNotSessionActive(squadId);
  if (guardError) {
    return guardError;
  }

  const payload = await getPayload({ config });
  try {
    // The RS-uniqueness rule (at most one per membership) is enforced by
    // round-results' own beforeChange hook.
    await payload.update({ collection: 'round-results', id: roundResultId, data: { status: 'rs' } });
  } catch (error) {
    return { ok: false, error: error instanceof Error ? error.message : 'Could not flag this round.' };
  }

  revalidatePath('/timekeeper');

  return { ok: true };
}

/**
 * Marks a round the shooter shot but did not complete — ran out of
 * ammunition, ran out of time. Like RS it is a permanent marker rather
 * than a time, but unlike RS it earns no reshoot: the round is simply
 * done. Any time the timer recorded stays in storage; the card shows DNF.
 */
export async function flagDnfAction(squadId: number, roundResultId: number): Promise<ActionResult> {
  const guardError = await assertNotSessionActive(squadId);
  if (guardError) {
    return guardError;
  }

  const payload = await getPayload({ config });
  await payload.update({ collection: 'round-results', id: roundResultId, data: { status: 'dnf' } });

  revalidatePath('/timekeeper');

  return { ok: true };
}

/**
 * A DQ ends the shooter's whole match. Every squad-membership they hold
 * across squads in this match is marked disqualified, and each of those
 * cards' not-yet-shot rounds becomes the terminal `dq` marker. Rounds
 * already recorded (a time or RS) are left untouched — the match director
 * decides what a disqualified card counts for.
 */
export async function disqualifyShooterAction(
  squadId: number,
  membershipId: number,
  reason: string
): Promise<ActionResult> {
  const guardError = await assertNotSessionActive(squadId);
  if (guardError) {
    return guardError;
  }

  const trimmedReason = reason.trim();
  if (!trimmedReason) {
    return { ok: false, error: 'A disqualification needs a reason.' };
  }

  const payload = await getPayload({ config });
  const membership = await payload.findByID({ collection: 'squad-memberships', id: membershipId, depth: 2 });

  const shooterId = typeof membership.shooter === 'object' ? membership.shooter.id : membership.shooter;
  const squad = typeof membership.squad === 'object' ? membership.squad : null;
  const matchId
    = squad && typeof squad.match === 'object' ? squad.match.id : (squad?.match ?? null);
  if (matchId == null) {
    return { ok: false, error: 'This squad has no match assigned.' };
  }

  const squadsInMatch = await payload.find({
    collection: 'squads',
    where: { match: { equals: matchId } },
    limit: 100
  });
  const squadIds = squadsInMatch.docs.map(s => s.id);

  const cards = await payload.find({
    collection: 'squad-memberships',
    where: { and: [{ shooter: { equals: shooterId } }, { squad: { in: squadIds } }] },
    limit: 100
  });

  const disqualifiedAt = new Date().toISOString();

  // Not a single transaction — like markAbsentAction, the updates run one
  // by one. If one fails the caller sees the error and can re-run; a DQ is
  // idempotent (statuses only ever move to disqualified / dq), so a retry
  // finishes the partially-applied cards without side effects.
  try {
    for (const card of cards.docs) {
      await payload.update({
        collection: 'squad-memberships',
        id: card.id,
        data: { status: 'disqualified', disqualifiedReason: trimmedReason, disqualifiedAt }
      });

      const pendingRounds = await payload.find({
        collection: 'round-results',
        where: { and: [{ membership: { equals: card.id } }, { status: { equals: 'pending' } }] },
        limit: 20
      });
      for (const round of pendingRounds.docs) {
        await payload.update({ collection: 'round-results', id: round.id, data: { status: 'dq' } });
      }
    }
  } catch (error) {
    return { ok: false, error: error instanceof Error ? error.message : 'Could not disqualify this shooter.' };
  }

  revalidatePath('/timekeeper');

  return { ok: true };
}

export async function markSignedOffAction(membershipId: number): Promise<ActionResult> {
  const payload = await getPayload({ config });
  await payload.update({
    collection: 'squad-memberships',
    id: membershipId,
    data: { signedOffAt: new Date().toISOString() }
  });

  revalidatePath('/timekeeper');

  return { ok: true };
}
