import { getPayload } from 'payload'
import config from '@/payload.config'
import type { MatchSession, Squad } from '@/payload-types'
import { type MembershipView, deriveCurrentRound, isDeviceSessionActive } from './matchState'

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

export async function loadSquadView(squadId: number): Promise<SquadView> {
  const payload = await getPayload({ config })

  const squad = await payload.findByID({ collection: 'squads', id: squadId, depth: 1 })

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

  const deviceId = typeof squad.device === 'object' ? squad.device?.id : squad.device
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
