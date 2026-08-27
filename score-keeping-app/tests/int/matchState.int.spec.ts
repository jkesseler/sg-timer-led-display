import { describe, it, expect } from 'vitest';
import {
  deriveCurrentRound,
  deriveOutstanding,
  deriveUpcomingShooters,
  isReadyForSignOff,
  type MembershipView
} from '@/lib/match/matchState';
import type { RoundResult, SquadMembership } from '@/payload-types';

interface RoundSpec {
  roundNumber: number;
  status: RoundResult['status'];
}

// The derivations only read status, roundNumber, id, queuePosition and
// reshootTimeMs — build just enough of each shape and cast at the boundary.
function buildView(
  spec: {
    id: number;
    status: SquadMembership['status'];
    queuePosition?: number;
    reshootTimeMs?: number;
    rounds: RoundSpec[];
  }
): MembershipView {
  return {
    membership: {
      id: spec.id,
      status: spec.status,
      queuePosition: spec.queuePosition ?? spec.id,
      reshootTimeMs: spec.reshootTimeMs ?? null
    } as SquadMembership,
    roundResults: spec.rounds.map(round => ({
      id: spec.id * 100 + round.roundNumber,
      roundNumber: round.roundNumber,
      status: round.status
    }) as RoundResult)
  };
}

const fiveRounds = (status: RoundResult['status']): RoundSpec[] =>
  [1, 2, 3, 4, 5].map(roundNumber => ({ roundNumber, status }));

describe('matchState — DNF and DQ do not leak into derivations', () => {
  it('deriveCurrentRound ignores DNF and DQ rounds', () => {
    const views = [
      buildView({ id: 1, status: 'present', rounds: [
        { roundNumber: 1, status: 'timed' },
        { roundNumber: 2, status: 'dnf' },
        { roundNumber: 3, status: 'pending' },
        { roundNumber: 4, status: 'pending' },
        { roundNumber: 5, status: 'pending' }
      ] })
    ];

    expect(deriveCurrentRound(views)).toBe(3);
  });

  it('deriveCurrentRound returns null once every remaining round is DNF/DQ/timed', () => {
    const views = [
      buildView({ id: 1, status: 'present', rounds: [
        { roundNumber: 1, status: 'timed' },
        { roundNumber: 2, status: 'timed' },
        { roundNumber: 3, status: 'dnf' },
        { roundNumber: 4, status: 'timed' },
        { roundNumber: 5, status: 'timed' }
      ] })
    ];

    expect(deriveCurrentRound(views)).toBeNull();
  });

  it('deriveOutstanding does not queue DNF or DQ rounds for catch-up', () => {
    const views = [
      buildView({ id: 1, status: 'present', rounds: [
        { roundNumber: 1, status: 'dnf' },
        { roundNumber: 2, status: 'rs' },
        { roundNumber: 3, status: 'skipped' },
        { roundNumber: 4, status: 'timed' },
        { roundNumber: 5, status: 'timed' }
      ] })
    ];

    const kinds = deriveOutstanding(views).map(item => item.kind);
    expect(kinds).toEqual(['rs', 'skipped']);
  });

  it('a disqualified membership contributes nothing to any queue', () => {
    const views = [
      buildView({ id: 1, status: 'disqualified', rounds: [
        { roundNumber: 1, status: 'timed' },
        { roundNumber: 2, status: 'dq' },
        { roundNumber: 3, status: 'dq' },
        { roundNumber: 4, status: 'dq' },
        { roundNumber: 5, status: 'dq' }
      ] }),
      buildView({ id: 2, status: 'present', queuePosition: 2, rounds: fiveRounds('pending') })
    ];

    expect(deriveCurrentRound(views)).toBe(1);
    expect(deriveOutstanding(views)).toEqual([]);

    const { next, onDeck } = deriveUpcomingShooters(views, 1, null);
    expect(next?.membership.id).toBe(2);
    expect(onDeck).toBeNull();
  });

  it('isReadyForSignOff treats a DNF round as resolved', () => {
    const view = buildView({ id: 1, status: 'present', rounds: [
      { roundNumber: 1, status: 'timed' },
      { roundNumber: 2, status: 'timed' },
      { roundNumber: 3, status: 'dnf' },
      { roundNumber: 4, status: 'timed' },
      { roundNumber: 5, status: 'timed' }
    ] });

    expect(isReadyForSignOff(view)).toBe(true);
  });
});
