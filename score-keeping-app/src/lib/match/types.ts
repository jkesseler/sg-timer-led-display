import type { Discipline } from '@/lib/domain/disciplines';

export const ROUNDS_PER_CARD = 5;

export type RoundStatus = 'pending' | 'timed' | 'rs' | 'dnf' | 'skipped';

export type SquadStatus = 'scheduled' | 'active' | 'completed';

export type Presence = 'present' | 'absent';

export type TurnPhase = 'armed' | 'running';

// Everything below is persisted as JSON (localStorage and the match-states
// snapshot), which has no `undefined` — absent values are `null`.

export interface Round {
  n: number;
  status: RoundStatus;
  timeMs: number | null;
  reshootTimeMs: number | null;
}

export interface Disqualification {
  reason: string;
  at: string;
}

export interface Card {
  id: string;
  squadId: string;
  shooterId: string;
  shooterName: string;
  knsaNumber: string | null;
  discipline: Discipline;
  queuePosition: number;
  presence: Presence;
  rounds: Round[];
  dq: Disqualification | null;
  signedOffAt: string | null;
}

export interface MatchSquad {
  id: string;
  label: string;
  start: string;
  end: string;
  status: SquadStatus;
}

export interface ActiveTurn {
  cardId: string;
  round: number;
  phase: TurnPhase;
}

export interface UnassignedResult {
  id: string;
  timeMs: number;
  at: string;
}

export interface MatchState {
  matchId: string;
  deviceId: string | null;
  revision: number;
  squads: MatchSquad[];
  cards: Card[];
  activeTurn: ActiveTurn | null;
  unassignedResults: UnassignedResult[];
}
