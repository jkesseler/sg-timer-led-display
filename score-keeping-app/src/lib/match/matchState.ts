import type { MatchSession, RoundResult, Squad, SquadMembership } from '@/payload-types'

export const ROUNDS_PER_MEMBERSHIP = 5

export interface MembershipView {
  membership: SquadMembership
  /** This membership's round-results, sorted by roundNumber. */
  roundResults: RoundResult[]
}

// Pure — safe to import from client components (unlike loadSquadView.ts,
// which pulls in the server-only `payload` package).

/** The device belongs to the squad's match, not to the squad itself — every squad in a match shares the one timer. */
export function resolveSquadDeviceId(squad: Squad): number | null {
  const match = squad.match
  if (typeof match !== 'object' || match === null) return null
  const device = match.device
  return typeof device === 'object' ? (device?.id ?? null) : (device ?? null)
}

/** The firmware's 6-char MQTT device ID (timer/<deviceId>/...) for this squad's match — needs the squad fetched at depth >= 2. */
export function resolveSquadFirmwareDeviceId(squad: Squad): string | null {
  const match = squad.match
  if (typeof match !== 'object' || match === null) return null
  const device = match.device
  return typeof device === 'object' ? (device?.deviceId ?? null) : null
}

/**
 * SS.CC, zero-padded, truncated (not rounded) — the safe direction to be
 * wrong in a competitive context: rounding up could make a slower recorded
 * time display as faster than a genuinely quicker one. Full millisecond
 * precision stays in storage; only the display is lossy.
 */
export function formatRoundTimeMs(timeMs: number): string {
  const centiseconds = Math.floor(timeMs / 10)
  const seconds = Math.floor(centiseconds / 100)
  const remainderCentiseconds = centiseconds % 100
  return `${String(seconds).padStart(2, '0')}.${String(remainderCentiseconds).padStart(2, '0')}`
}

/**
 * The lowest round number (1-5) that still has a pending result among
 * present memberships. Null once none remain — the squad has moved into
 * the reshoot/catch-up phase.
 */
export function deriveCurrentRound(memberships: MembershipView[]): number | null {
  const present = memberships.filter((m) => m.membership.status === 'present')
  let lowest: number | null = null

  for (const { roundResults } of present) {
    for (const result of roundResults) {
      if (result.status === 'pending' && (lowest === null || result.roundNumber < lowest)) {
        lowest = result.roundNumber
      }
    }
  }

  return lowest
}

/**
 * Next / on-deck shooters for the current round, derived live from the
 * mutable queue — never computed once from starting order. `next` is
 * whoever shoots after the currently active shooter (excluded); `onDeck` is
 * one further out. Both are null once nothing remains for the current
 * round (end of round, end of squad, or during the reshoot phase — the
 * caller should query deriveOutstanding for that phase's own queue).
 */
export function deriveUpcomingShooters(
  memberships: MembershipView[],
  currentRound: number | null,
  activeMembershipId: number | null,
): { next: MembershipView | null; onDeck: MembershipView | null } {
  if (currentRound === null) {
    return { next: null, onDeck: null }
  }

  const waiting = memberships
    .filter((m) => m.membership.status === 'present' && m.membership.id !== activeMembershipId)
    .filter((m) => m.roundResults.some((r) => r.roundNumber === currentRound && r.status === 'pending'))
    .sort((a, b) => a.membership.queuePosition - b.membership.queuePosition)

  return { next: waiting[0] ?? null, onDeck: waiting[1] ?? null }
}

export interface OutstandingItem {
  membership: MembershipView
  kind: 'rs' | 'skipped'
  roundNumber: number
  roundResultId: number
}

/**
 * Deferred reshoots (RS-marked rounds with no reshoot time yet) and
 * catch-up rounds (skipped, e.g. a late arrival) — the queue for the phase
 * after the main 5-round rotation is done. FIFO by round number as a
 * deterministic default; the timekeeper can offer them in any order.
 */
export function deriveOutstanding(memberships: MembershipView[]): OutstandingItem[] {
  const items: OutstandingItem[] = []

  for (const view of memberships) {
    if (view.membership.status !== 'present') continue

    for (const result of view.roundResults) {
      if (result.status === 'rs' && !view.membership.reshootTimeMs) {
        items.push({ membership: view, kind: 'rs', roundNumber: result.roundNumber, roundResultId: result.id })
      } else if (result.status === 'skipped') {
        items.push({ membership: view, kind: 'skipped', roundNumber: result.roundNumber, roundResultId: result.id })
      }
    }
  }

  return items.sort((a, b) => a.roundNumber - b.roundNumber)
}

/** A shooter with an outstanding RS and no reshoot time yet cannot sign — no partial sign-off. */
export function isReadyForSignOff(view: MembershipView): boolean {
  const allResolved = view.roundResults.every((r) => r.status !== 'pending')
  const hasUnresolvedRs = view.roundResults.some((r) => r.status === 'rs') && !view.membership.reshootTimeMs
  return allResolved && !hasUnresolvedRs
}

/**
 * The scanner/manual-select/queue-mutation guard: true whenever the given
 * device has a match-session that's still pending or active. Once a
 * session is completed/abandoned, the device is immediately free again —
 * there's no separate "close the turn" server-side gate; "End Turn" in the
 * UI is a local acknowledgement of the result, not a state transition.
 */
export function isDeviceSessionActive(deviceId: number, sessions: MatchSession[]): boolean {
  return sessions.some((session) => {
    const sessionDeviceId = typeof session.device === 'object' ? session.device.id : session.device
    return sessionDeviceId === deviceId && (session.status === 'pending' || session.status === 'active')
  })
}
