import type { Match, Shooter } from '@/payload-types';
import { buildMatchState } from './buildMatchState';
import type { Payload } from 'payload';
import type { MatchSetup } from './buildMatchState';
import type { MatchState } from './types';

// Server-only: pulls in the `payload` package via the Payload instance.

function relationId(value: string | { id: string }): string {
  return typeof value === 'object' ? value.id : value;
}

/** The match the app works on: the ticked `active` match with the newest date. */
export async function findActiveMatch(payload: Payload): Promise<Match | null> {
  const result = await payload.find({
    collection: 'matches',
    where: { active: { equals: true } },
    sort: '-date',
    depth: 1,
    limit: 1
  });

  return result.docs[0] ?? null;
}

export function getMatchDeviceId(match: Match): string | null {
  return typeof match.device === 'object' ? match.device.deviceId : null;
}

export async function loadMatchSetup(payload: Payload, match: Match): Promise<MatchSetup> {
  const squads = await payload.find({
    collection: 'squads',
    where: { match: { equals: match.id } },
    sort: 'startTime',
    depth: 0,
    pagination: false
  });

  const squadIds = squads.docs.map(squad => squad.id);
  const members = await payload.find({
    collection: 'squad-members',
    where: { squad: { in: squadIds } },
    depth: 1,
    pagination: false
  });

  const shooters = new Map<string, Shooter>();
  for (const member of members.docs) {
    if (typeof member.shooter === 'object') {
      shooters.set(member.shooter.id, member.shooter);
    }
  }

  return {
    matchId: match.id,
    deviceId: getMatchDeviceId(match),
    squads: squads.docs.map(squad => ({
      id: squad.id,
      label: squad.label ?? `${squad.startTime} - ${squad.endTime}`,
      startTime: squad.startTime,
      endTime: squad.endTime,
      discipline: squad.discipline
    })),
    members: members.docs.map(member => ({
      id: member.id,
      squadId: relationId(member.squad),
      shooterId: relationId(member.shooter),
      startingPosition: member.startingPosition
    })),
    shooters: [...shooters.values()].map(shooter => ({
      id: shooter.id,
      firstName: shooter.firstName,
      lastName: shooter.lastName,
      knsaNumber: shooter.knsaNumber ?? null
    }))
  };
}

export async function findMatchState(payload: Payload, matchId: string): Promise<MatchState | null> {
  const result = await payload.find({
    collection: 'match-states',
    where: { match: { equals: matchId } },
    depth: 0,
    limit: 1
  });
  const doc = result.docs[0];

  if (!doc) {
    return null;
  }

  // Payload types json fields loosely; this field is only ever written by saveMatchState.
  return doc.state as unknown as MatchState;
}

export async function buildFreshMatchState(payload: Payload, match: Match): Promise<MatchState> {
  return buildMatchState(await loadMatchSetup(payload, match));
}
