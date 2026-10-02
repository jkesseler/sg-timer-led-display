import type { Card, MatchState, Round, RoundStatus } from '@/lib/match/types';

interface RoundSpec {
  status: RoundStatus;
  timeMs?: number;
  reshootTimeMs?: number;
}

interface CardSpec {
  id: string;
  squadId?: string;
  shooterId?: string;
  knsaNumber?: string;
  discipline?: Card['discipline'];
  queuePosition?: number;
  presence?: Card['presence'];
  rounds?: RoundSpec[];
  dq?: Card['dq'];
  signedOffAt?: string;
}

export function buildRounds(specs: RoundSpec[]): Round[] {
  return specs.map((spec, index) => ({
    n: index + 1,
    status: spec.status,
    timeMs: spec.timeMs ?? null,
    reshootTimeMs: spec.reshootTimeMs ?? null
  }));
}

export function timedRounds(...timesMs: number[]): Round[] {
  return buildRounds(timesMs.map(timeMs => ({ status: 'timed', timeMs })));
}

export const pendingRounds = (): Round[] => buildRounds(Array.from({ length: 5 }, () => ({ status: 'pending' })));

export function buildCard(spec: CardSpec): Card {
  return {
    id: spec.id,
    squadId: spec.squadId ?? 'squad-a',
    shooterId: spec.shooterId ?? `shooter-${spec.id}`,
    shooterName: `Shooter ${spec.id}`,
    knsaNumber: spec.knsaNumber ?? null,
    discipline: spec.discipline ?? 'OKP',
    queuePosition: spec.queuePosition ?? 0,
    presence: spec.presence ?? 'present',
    rounds: spec.rounds ? buildRounds(spec.rounds) : pendingRounds(),
    dq: spec.dq ?? null,
    signedOffAt: spec.signedOffAt ?? null
  };
}

export function buildState(cards: Card[], overrides: Partial<MatchState> = {}): MatchState {
  return {
    matchId: 'match-1',
    deviceId: 'ABC123',
    revision: 0,
    squads: [
      { id: 'squad-a', label: 'A', start: '08:00', end: '09:00', status: 'active' },
      { id: 'squad-b', label: 'B', start: '09:00', end: '10:00', status: 'scheduled' }
    ],
    cards,
    activeTurn: null,
    unassignedResults: [],
    ...overrides
  };
}
