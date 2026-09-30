import type { Discipline } from '@/lib/domain/disciplines';
import { ROUNDS_PER_CARD } from './types';
import type { Card, MatchState, Round } from './types';

export interface SetupSquad {
  id: string;
  label: string;
  startTime: string;
  endTime: string;
  discipline: Discipline;
}

export interface SetupMember {
  id: string;
  squadId: string;
  shooterId: string;
  startingPosition: number;
}

export interface SetupShooter {
  id: string;
  firstName: string;
  lastName: string;
  knsaNumber: string | null;
}

export interface MatchSetup {
  matchId: string;
  deviceId: string | null;
  squads: SetupSquad[];
  members: SetupMember[];
  shooters: SetupShooter[];
}

export function buildPendingRounds(): Round[] {
  return Array.from({ length: ROUNDS_PER_CARD }, (_, index) => ({
    n: index + 1,
    status: 'pending',
    timeMs: null,
    reshootTimeMs: null
  }));
}

/** The initial state for a match that has no saved state yet: every card present, all rounds pending, queue in starting order. */
export function buildMatchState(setup: MatchSetup): MatchState {
  const cards: Card[] = [];

  for (const squad of setup.squads) {
    const members = setup.members
      .filter(member => member.squadId === squad.id)
      .sort((a, b) => a.startingPosition - b.startingPosition);

    members.forEach((member, index) => {
      const shooter = setup.shooters.find(candidate => candidate.id === member.shooterId);

      cards.push({
        id: member.id,
        squadId: squad.id,
        shooterId: member.shooterId,
        shooterName: shooter ? `${shooter.firstName} ${shooter.lastName}` : 'Unknown shooter',
        knsaNumber: shooter?.knsaNumber ?? null,
        queuePosition: index,
        presence: 'present',
        rounds: buildPendingRounds(),
        dq: null,
        signedOffAt: null
      });
    });
  }

  return {
    matchId: setup.matchId,
    deviceId: setup.deviceId,
    revision: 0,
    squads: setup.squads.map(squad => ({
      id: squad.id,
      label: squad.label,
      start: squad.startTime,
      end: squad.endTime,
      discipline: squad.discipline,
      status: 'scheduled'
    })),
    cards,
    activeTurn: null,
    unassignedResults: []
  };
}
