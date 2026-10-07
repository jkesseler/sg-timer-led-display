import type { Discipline } from '@/lib/domain/disciplines';
import type { MqttServerConfig } from '@/lib/mqtt/types';

export const ROUNDS_PER_CARD = 5;

export type RoundStatus = 'pending' | 'timed' | 'rs' | 'dnf' | 'skipped';

export type SquadStatus = 'scheduled' | 'active' | 'completed';

export type Presence = 'present' | 'absent';

export type TurnPhase = 'armed' | 'running';

export interface ShooterOption {
  id: string;
  name: string;
  knsaNumber: string | null;
}

export interface BootstrapMatch {
  id: string;
  label: string;
  freshState: MatchState;
  serverState: MatchState | null;
  shooters: ShooterOption[];
}

/** Response of GET /timekeeper/bootstrap. */
export interface TimekeeperBootstrap {
  userEmail: string;
  mqttConfig: MqttServerConfig;
  match: BootstrapMatch | null;
}

export interface SetupSquad {
  id: string;
  label: string;
  startTime: string;
  endTime: string;
}

export interface SetupMember {
  id: string;
  squadId: string;
  shooterId: string;
  startingPosition: number;
  discipline: Discipline;
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

export interface RelatedDocument {
  id: string;
}

export interface ScoredCard {
  status: 'scored';
  scoreMs: number;
}

export interface UnscoredCard {
  status: 'pending' | 'uncountable' | 'dq';
}

export type CardScore = ScoredCard | UnscoredCard;

export interface OutstandingItem {
  card: Card;
  kind: 'rs' | 'skipped';
  round: number;
}

export type CardWarning = 'multiple-rs' | 'signed-with-open-rounds';

export interface RosterInfo {
  current: string | null;
  next: string | null;
  onDeck: string | null;
}

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
