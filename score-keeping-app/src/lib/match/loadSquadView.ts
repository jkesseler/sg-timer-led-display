import { getPayload } from 'payload'
import config from '@/payload.config'
import type { MatchSession, Squad } from '@/payload-types'
import { type MembershipView, deriveCurrentRound, isDeviceSessionActive, resolveSquadDeviceId } from './matchState'

export interface SquadView {
  squad: Squad
  memberships: MembershipView[] // sorted by queuePosition
  currentRound: number | null
  activeMembershipId: number | null
  isSessionActive: boolean
  liveSessions: MatchSession[]
}

/** Squads currently open for timekeeping — status active or reshoot-phase. */
export async function listOpenSquads(): Promise<Squad[]> {
  const payload = await getPayload({ config })
  const result = await payload.find({
    collection: 'squads',
    where: { status: { in: ['active', 'reshoot-phase'] } },
    sort: 'startTime',
    limit: 50,
  })
  return result.docs
}

/** The open squad (if any) currently rotating through the match that owns this device. */
export async function findOpenSquadForDevice(firmwareDeviceId: string): Promise<Squad | null> {
  const payload = await getPayload({ config })

  const devices = await payload.find({
    collection: 'devices',
    where: { deviceId: { equals: firmwareDeviceId } },
    limit: 1,
  })
  const device = devices.docs[0]
  if (!device) return null

  const matches = await payload.find({
    collection: 'matches',
    where: { device: { equals: device.id } },
    limit: 20,
  })
  if (matches.docs.length === 0) return null

  const squads = await payload.find({
    collection: 'squads',
    where: {
      and: [{ match: { in: matches.docs.map((m) => m.id) } }, { status: { in: ['active', 'reshoot-phase'] } }],
    },
    sort: 'startTime',
    limit: 1,
  })
  return squads.docs[0] ?? null
}

export async function loadSquadView(squadId: number): Promise<SquadView> {
  const payload = await getPayload({ config })

  // depth 2: squad -> match -> device, so the match's timer device is
  // available as a real object (see resolveSquadDeviceId).
  const squad = await payload.findByID({ collection: 'squads', id: squadId, depth: 2 })

  const membershipsResult = await payload.find({
    collection: 'squad-memberships',
    where: { squad: { equals: squadId } },
    sort: 'queuePosition',
    depth: 1,
    limit: 100,
  })

  const memberships: MembershipView[] = []
  for (const membership of membershipsResult.docs) {
    const roundResultsResult = await payload.find({
      collection: 'round-results',
      where: { membership: { equals: membership.id } },
      sort: 'roundNumber',
      limit: 10,
    })
    memberships.push({ membership, roundResults: roundResultsResult.docs })
  }

  const deviceId = resolveSquadDeviceId(squad)
  let liveSessions: MatchSession[] = []
  let activeMembershipId: number | null = null

  if (deviceId != null) {
    const sessionsResult = await payload.find({
      collection: 'match-sessions',
      where: { device: { equals: deviceId }, status: { in: ['pending', 'active'] } },
      depth: 1,
      limit: 10,
    })
    liveSessions = sessionsResult.docs

    const liveSession = liveSessions[0]
    if (liveSession) {
      const target = liveSession.roundResult ?? liveSession.reshootFor
      if (target && typeof target === 'object') {
        activeMembershipId =
          'membership' in target
            ? (typeof target.membership === 'object' ? target.membership.id : target.membership)
            : target.id
      }
    }
  }

  return {
    squad,
    memberships,
    currentRound: deriveCurrentRound(memberships),
    activeMembershipId,
    isSessionActive: deviceId != null && isDeviceSessionActive(deviceId, liveSessions),
    liveSessions,
  }
}
