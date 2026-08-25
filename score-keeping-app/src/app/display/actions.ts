'use server'

import { getPayload } from 'payload'
import config from '@/payload.config'
import { listOpenSquads, loadSquadView } from '@/lib/match/loadSquadView'
import { deriveUpcomingShooters, deriveOutstanding } from '@/lib/match/matchState'
import type { MembershipView } from '@/lib/match/matchState'

export interface RosterInfo {
  current: string | null
  next: string | null
  onDeck: string | null
}

function shooterName(view: MembershipView): string {
  const shooter = view.membership.shooter
  return typeof shooter === 'object' ? `${shooter.firstName} ${shooter.lastName}` : `Shooter #${shooter}`
}

/**
 * The Next:/On deck: callouts for whichever squad is bound to this device.
 * Polled from the /display route (a lightweight read, not a live push
 * channel — see the plan's note on this simplification). During the
 * reshoot/catch-up phase, names are suffixed "(reshoot)" per the plan.
 */
export async function getRosterForDevice(deviceId: string): Promise<RosterInfo> {
  const empty: RosterInfo = { current: null, next: null, onDeck: null }
  if (!deviceId) return empty

  const payload = await getPayload({ config })
  const devices = await payload.find({ collection: 'devices', where: { deviceId: { equals: deviceId } }, limit: 1 })
  const device = devices.docs[0]
  if (!device) return empty

  const openSquads = await listOpenSquads()
  const squad = openSquads.find((s) => (typeof s.device === 'object' ? s.device?.id : s.device) === device.id)
  if (!squad) return empty

  const view = await loadSquadView(squad.id)
  const activeMembership = view.memberships.find((m) => m.membership.id === view.activeMembershipId) ?? null
  const current = activeMembership ? shooterName(activeMembership) : null

  if (view.currentRound !== null) {
    const { next, onDeck } = deriveUpcomingShooters(view.memberships, view.currentRound, view.activeMembershipId)
    return { current, next: next ? shooterName(next) : null, onDeck: onDeck ? shooterName(onDeck) : null }
  }

  const outstanding = deriveOutstanding(view.memberships)
  const next = outstanding[0] ? `${shooterName(outstanding[0].membership)} (reshoot)` : null
  const onDeck = outstanding[1] ? `${shooterName(outstanding[1].membership)} (reshoot)` : null
  return { current, next, onDeck }
}
