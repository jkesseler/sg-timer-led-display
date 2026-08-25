'use server'

import { getPayload } from 'payload'
import { revalidatePath } from 'next/cache'
import config from '@/payload.config'
import { loadSquadView } from '@/lib/match/loadSquadView'
import { resolveSquadDeviceId, type MembershipView } from '@/lib/match/matchState'

export interface ActionResult {
  ok: boolean
  error?: string
}

/** Resolves whether a fresh activation for this membership should target the next pending round or a deferred reshoot. */
function resolveActivationTarget(view: MembershipView): { roundResult: number } | { reshootFor: number } | null {
  const pendingRound = view.roundResults.find((r) => r.status === 'pending')
  if (pendingRound) {
    return { roundResult: pendingRound.id }
  }

  const hasUnresolvedRs = view.roundResults.some((r) => r.status === 'rs') && !view.membership.reshootTimeMs
  if (hasUnresolvedRs) {
    return { reshootFor: view.membership.id }
  }

  return null
}

async function activate(squadId: number, membershipId: number): Promise<ActionResult> {
  const payload = await getPayload({ config })
  const view = await loadSquadView(squadId)

  const deviceId = resolveSquadDeviceId(view.squad)
  if (deviceId == null) {
    return { ok: false, error: 'This squad has no timer device assigned (set one on its match).' }
  }
  if (view.isSessionActive) {
    return { ok: false, error: 'A turn is already in progress on this device.' }
  }

  const membershipView = view.memberships.find((m) => m.membership.id === membershipId)
  if (!membershipView) {
    return { ok: false, error: 'Unknown shooter for this squad.' }
  }
  if (membershipView.membership.status !== 'present') {
    return { ok: false, error: 'This shooter is not marked present.' }
  }

  const target = resolveActivationTarget(membershipView)
  if (!target) {
    return { ok: false, error: 'This shooter has nothing outstanding to shoot.' }
  }

  try {
    await payload.create({
      collection: 'match-sessions',
      data: { device: deviceId, status: 'pending', ...target },
    })
  } catch (error) {
    return { ok: false, error: error instanceof Error ? error.message : 'Could not start the turn.' }
  }

  revalidatePath('/timekeeper')
  return { ok: true }
}

export async function activateMembershipAction(squadId: number, membershipId: number): Promise<ActionResult> {
  return activate(squadId, membershipId)
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
  const payload = await getPayload({ config })
  const view = await loadSquadView(squadId)

  const liveSession = view.liveSessions[0]
  if (!liveSession) {
    return { ok: false, error: 'No active turn to cancel.' }
  }

  await payload.update({
    collection: 'match-sessions',
    id: liveSession.id,
    data: { status: 'abandoned', stoppedAtMs: Date.now() },
  })

  revalidatePath('/timekeeper')
  return { ok: true }
}

export async function activateByKnsaAction(squadId: number, rawCode: string): Promise<ActionResult> {
  const code = rawCode.trim()
  if (!code) {
    return { ok: false, error: 'Enter or scan a KNSA number.' }
  }

  const payload = await getPayload({ config })
  const shooters = await payload.find({
    collection: 'shooters',
    where: { knsaNumber: { equals: code } },
    limit: 1,
  })
  const shooter = shooters.docs[0]
  if (!shooter) {
    return { ok: false, error: `No shooter found for code "${code}".` }
  }

  const view = await loadSquadView(squadId)
  const membershipView = view.memberships.find((m) => {
    const shooterId = typeof m.membership.shooter === 'object' ? m.membership.shooter.id : m.membership.shooter
    return shooterId === shooter.id
  })
  if (!membershipView) {
    return { ok: false, error: `${shooter.firstName} ${shooter.lastName} is not in this squad.` }
  }

  return activate(squadId, membershipView.membership.id)
}

async function assertNotSessionActive(squadId: number): Promise<ActionResult | null> {
  const view = await loadSquadView(squadId)
  if (view.isSessionActive) {
    return { ok: false, error: 'Cannot change the queue while a turn is in progress.' }
  }
  return null
}

/** Drag-and-drop reorder: orderedMembershipIds is the full new front-to-back order of present shooters. */
export async function reorderQueueAction(squadId: number, orderedMembershipIds: number[]): Promise<ActionResult> {
  const guardError = await assertNotSessionActive(squadId)
  if (guardError) return guardError

  const payload = await getPayload({ config })
  await Promise.all(
    orderedMembershipIds.map((membershipId, index) =>
      payload.update({
        collection: 'squad-memberships',
        id: membershipId,
        data: { queuePosition: index + 1 },
      }),
    ),
  )

  revalidatePath('/timekeeper')
  return { ok: true }
}

export async function markAbsentAction(squadId: number, membershipId: number): Promise<ActionResult> {
  const guardError = await assertNotSessionActive(squadId)
  if (guardError) return guardError

  const payload = await getPayload({ config })
  const view = await loadSquadView(squadId)
  const membershipView = view.memberships.find((m) => m.membership.id === membershipId)

  await payload.update({ collection: 'squad-memberships', id: membershipId, data: { status: 'absent' } })

  for (const result of membershipView?.roundResults ?? []) {
    if (result.status === 'pending') {
      await payload.update({ collection: 'round-results', id: result.id, data: { status: 'skipped' } })
    }
  }

  revalidatePath('/timekeeper')
  return { ok: true }
}

export async function markPresentAction(squadId: number, membershipId: number): Promise<ActionResult> {
  const guardError = await assertNotSessionActive(squadId)
  if (guardError) return guardError

  const payload = await getPayload({ config })
  const view = await loadSquadView(squadId)
  const maxPosition = Math.max(0, ...view.memberships.map((m) => m.membership.queuePosition))

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
    data: { status: 'present', queuePosition: maxPosition + 1 },
  })

  revalidatePath('/timekeeper')
  return { ok: true }
}

/** Offers one specific skipped (catch-up) round during the outstanding-items phase: revives it to pending, then activates it. */
export async function offerCatchUpAction(squadId: number, roundResultId: number): Promise<ActionResult> {
  const guardError = await assertNotSessionActive(squadId)
  if (guardError) return guardError

  const payload = await getPayload({ config })
  const roundResult = await payload.findByID({ collection: 'round-results', id: roundResultId })
  if (roundResult.status !== 'skipped') {
    return { ok: false, error: 'This round is not an outstanding catch-up item.' }
  }

  const membershipId = typeof roundResult.membership === 'object' ? roundResult.membership.id : roundResult.membership
  await payload.update({ collection: 'round-results', id: roundResultId, data: { status: 'pending' } })

  return activate(squadId, membershipId)
}

export async function flagMalfunctionAction(squadId: number, roundResultId: number): Promise<ActionResult> {
  const guardError = await assertNotSessionActive(squadId)
  if (guardError) return guardError

  const payload = await getPayload({ config })
  try {
    // The RS-uniqueness rule (at most one per membership) is enforced by
    // round-results' own beforeChange hook.
    await payload.update({ collection: 'round-results', id: roundResultId, data: { status: 'rs' } })
  } catch (error) {
    return { ok: false, error: error instanceof Error ? error.message : 'Could not flag this round.' }
  }

  revalidatePath('/timekeeper')
  return { ok: true }
}

export async function markSignedOffAction(membershipId: number): Promise<ActionResult> {
  const payload = await getPayload({ config })
  await payload.update({
    collection: 'squad-memberships',
    id: membershipId,
    data: { signedOffAt: new Date().toISOString() },
  })

  revalidatePath('/timekeeper')
  return { ok: true }
}
