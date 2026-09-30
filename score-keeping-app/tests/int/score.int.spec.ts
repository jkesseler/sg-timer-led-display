import { describe, it, expect } from 'vitest';
import { formatRoundTimeMs, formatScore } from '@/lib/match/derive';
import { cardScore, finalScoreMs, matchScoresForShooter } from '@/lib/match/score';
import { buildCard, buildState, timedRounds } from './matchFixtures';

describe('finalScoreMs', () => {
  it('averages the three fastest rounds: 3,2,2,1,1 -> 1.3333', () => {
    const card = { ...buildCard({ id: '1' }), rounds: timedRounds(3000, 2000, 2000, 1000, 1000) };

    const score = finalScoreMs(card);

    expect(score.status).toBe('scored');
    expect(score.status === 'scored' && score.scoreMs).toBeCloseTo(4000 / 3, 6);
  });

  it('uses the reshoot time in place of an RS round', () => {
    const card = buildCard({
      id: '1',
      rounds: [
        { status: 'timed', timeMs: 5000 },
        { status: 'rs', timeMs: 9000, reshootTimeMs: 1000 },
        { status: 'timed', timeMs: 5000 },
        { status: 'timed', timeMs: 2000 },
        { status: 'timed', timeMs: 3000 }
      ]
    });

    expect(finalScoreMs(card)).toEqual({ status: 'scored', scoreMs: 2000 });
  });

  it('is pending while an RS round has no reshoot yet', () => {
    const card = buildCard({
      id: '1',
      rounds: [
        { status: 'timed', timeMs: 1000 },
        { status: 'rs', timeMs: 900 },
        { status: 'timed', timeMs: 1000 },
        { status: 'timed', timeMs: 1000 },
        { status: 'timed', timeMs: 1000 }
      ]
    });

    expect(finalScoreMs(card)).toEqual({ status: 'pending' });
  });

  it('never counts a DNF round', () => {
    const card = buildCard({
      id: '1',
      rounds: [
        { status: 'dnf', timeMs: 100 },
        { status: 'timed', timeMs: 3000 },
        { status: 'timed', timeMs: 3000 },
        { status: 'timed', timeMs: 3000 },
        { status: 'timed', timeMs: 6000 }
      ]
    });

    expect(finalScoreMs(card)).toEqual({ status: 'scored', scoreMs: 3000 });
  });

  it('is uncountable with fewer than 3 countable rounds once all are resolved', () => {
    const card = buildCard({
      id: '1',
      rounds: [
        { status: 'dnf' },
        { status: 'dnf' },
        { status: 'dnf' },
        { status: 'timed', timeMs: 1000 },
        { status: 'timed', timeMs: 1000 }
      ]
    });

    expect(finalScoreMs(card)).toEqual({ status: 'uncountable' });
    expect(formatScore(finalScoreMs(card))).toBe('--:--');
  });

  it('is pending while rounds are still to be shot or caught up', () => {
    const card = buildCard({
      id: '1',
      rounds: [
        { status: 'timed', timeMs: 1000 },
        { status: 'timed', timeMs: 1000 },
        { status: 'timed', timeMs: 1000 },
        { status: 'skipped' },
        { status: 'pending' }
      ]
    });

    expect(finalScoreMs(card)).toEqual({ status: 'pending' });
  });

  it('handles ties among the fastest rounds', () => {
    const card = { ...buildCard({ id: '1' }), rounds: timedRounds(2000, 2000, 2000, 2000, 2000) };

    expect(finalScoreMs(card)).toEqual({ status: 'scored', scoreMs: 2000 });
  });
});

describe('cardScore and DQ', () => {
  it('a DQ on one card voids all of the shooter\'s cards in the match', () => {
    const dqCard = {
      ...buildCard({ id: 'a', shooterId: 'jan', squadId: 'squad-a', dq: { reason: 'unsafe', at: '2026-09-30T10:00:00Z' } }),
      rounds: timedRounds(1000, 1000, 1000, 1000, 1000)
    };
    const otherCard = { ...buildCard({ id: 'b', shooterId: 'jan', squadId: 'squad-b' }), rounds: timedRounds(1000, 1000, 1000, 1000, 1000) };
    const bystander = { ...buildCard({ id: 'c', shooterId: 'piet' }), rounds: timedRounds(1000, 1000, 1000, 1000, 1000) };
    const state = buildState([dqCard, otherCard, bystander]);

    expect(matchScoresForShooter(state, 'jan').map(entry => entry.score)).toEqual([{ status: 'dq' }, { status: 'dq' }]);
    expect(cardScore(state, bystander)).toEqual({ status: 'scored', scoreMs: 1000 });
    expect(formatScore(cardScore(state, otherCard))).toBe('DQ');
  });

  it('a card without any DQ for its shooter scores normally', () => {
    const card = { ...buildCard({ id: 'a', shooterId: 'jan' }), rounds: timedRounds(1000, 1000, 1000, 1000, 1000) };
    const state = buildState([card]);

    expect(cardScore(state, card)).toEqual({ status: 'scored', scoreMs: 1000 });
  });
});

describe('formatScore', () => {
  it('rounds the average to 2 decimals', () => {
    expect(formatScore({ status: 'scored', scoreMs: 4000 / 3 })).toBe('01.33');
    expect(formatScore({ status: 'scored', scoreMs: 1335 })).toBe('01.34');
    expect(formatScore({ status: 'scored', scoreMs: 12994.9 })).toBe('12.99');
    expect(formatScore({ status: 'scored', scoreMs: 12995 })).toBe('13.00');
  });

  it('shows a dash while the score is pending', () => {
    expect(formatScore({ status: 'pending' })).toBe('—');
  });

  it('rounds, unlike round times, which truncate', () => {
    expect(formatRoundTimeMs(1999)).toBe('01.99');
    expect(formatScore({ status: 'scored', scoreMs: 1999 })).toBe('02.00');
  });
});
