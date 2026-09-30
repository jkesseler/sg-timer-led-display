import { describe, it, expect } from 'vitest';
import {
  deriveCurrentRound,
  deriveOutstanding,
  deriveRoster,
  deriveUpcomingShooters,
  findCardByKnsa,
  findNextRoundToShoot,
  formatRoundTimeMs,
  getCardWarnings,
  isReadyForSignOff,
  parseTimeInput
} from '@/lib/match/derive';
import { buildCard, buildState } from './matchFixtures';

describe('deriveCurrentRound', () => {
  it('is the lowest pending round among present cards', () => {
    const cards = [
      buildCard({ id: '1', rounds: [{ status: 'timed', timeMs: 1 }, { status: 'dnf' }, { status: 'pending' }, { status: 'pending' }, { status: 'pending' }] }),
      buildCard({ id: '2', presence: 'absent' })
    ];

    expect(deriveCurrentRound(cards)).toBe(3);
  });

  it('is null once no present card has a pending round', () => {
    const cards = [buildCard({ id: '1', rounds: Array.from({ length: 5 }, () => ({ status: 'timed' as const, timeMs: 1 })) })];

    expect(deriveCurrentRound(cards)).toBeNull();
  });
});

describe('deriveUpcomingShooters', () => {
  it('follows the mutable queue order and skips the active card', () => {
    const cards = [
      buildCard({ id: 'a', queuePosition: 2 }),
      buildCard({ id: 'b', queuePosition: 0 }),
      buildCard({ id: 'c', queuePosition: 1 })
    ];

    const { next, onDeck } = deriveUpcomingShooters(cards, 1, 'b');

    expect(next?.id).toBe('c');
    expect(onDeck?.id).toBe('a');
  });
});

describe('deriveOutstanding', () => {
  it('lists RS rounds without a reshoot and skipped rounds, by round number', () => {
    const cards = [
      buildCard({ id: '1', rounds: [{ status: 'timed', timeMs: 1 }, { status: 'timed', timeMs: 1 }, { status: 'skipped' }, { status: 'timed', timeMs: 1 }, { status: 'timed', timeMs: 1 }] }),
      buildCard({ id: '2', rounds: [{ status: 'rs' }, { status: 'rs', reshootTimeMs: 5 }, { status: 'timed', timeMs: 1 }, { status: 'timed', timeMs: 1 }, { status: 'timed', timeMs: 1 }] })
    ];

    expect(deriveOutstanding(cards).map(item => [item.card.id, item.kind, item.round])).toEqual([
      ['2', 'rs', 1],
      ['1', 'skipped', 3]
    ]);
  });
});

describe('sign-off and warnings', () => {
  it('is not ready while an RS round has no reshoot', () => {
    const card = buildCard({ id: '1', rounds: [{ status: 'rs' }, { status: 'timed', timeMs: 1 }, { status: 'timed', timeMs: 1 }, { status: 'timed', timeMs: 1 }, { status: 'timed', timeMs: 1 }] });

    expect(isReadyForSignOff(card)).toBe(false);
  });

  it('warns about a second RS and about signing with open rounds, without blocking', () => {
    const card = buildCard({
      id: '1',
      signedOffAt: '2026-09-30T10:00:00Z',
      rounds: [{ status: 'rs' }, { status: 'rs' }, { status: 'pending' }, { status: 'pending' }, { status: 'pending' }]
    });

    expect(getCardWarnings(card)).toEqual(['multiple-rs', 'signed-with-open-rounds']);
  });
});

describe('findCardByKnsa', () => {
  const inSquadA = buildCard({ id: 'a', shooterId: 'jan', knsaNumber: '123456', squadId: 'squad-a' });
  const inSquadB = buildCard({ id: 'b', shooterId: 'jan', knsaNumber: '123456', squadId: 'squad-b' });
  const state = buildState([inSquadA, inSquadB]);

  it('prefers the card in the selected squad for a shooter in two squads', () => {
    expect(findCardByKnsa(state, '123456', 'squad-b')?.id).toBe('b');
    expect(findCardByKnsa(state, ' 123456 ', 'squad-a')?.id).toBe('a');
  });

  it('falls back to any card of the shooter', () => {
    expect(findCardByKnsa(state, '123456', null)?.id).toBe('a');
  });

  it('returns null for an unknown number', () => {
    expect(findCardByKnsa(state, '999999', 'squad-a')).toBeNull();
    expect(findCardByKnsa(state, '  ', 'squad-a')).toBeNull();
  });
});

describe('deriveRoster', () => {
  it('names the active shooter, next and on deck in the current squad', () => {
    const cards = [
      buildCard({ id: 'a', queuePosition: 0 }),
      buildCard({ id: 'b', queuePosition: 1 }),
      buildCard({ id: 'c', queuePosition: 2 }),
      buildCard({ id: 'x', squadId: 'squad-b' })
    ];
    const state = buildState(cards, { activeTurn: { cardId: 'a', round: 1, phase: 'armed' } });

    expect(deriveRoster(state)).toEqual({ current: 'Shooter a', next: 'Shooter b', onDeck: 'Shooter c' });
  });

  it('is empty when no squad is in play', () => {
    const state = buildState([], { squads: [] });

    expect(deriveRoster(state)).toEqual({ current: null, next: null, onDeck: null });
  });
});

describe('formatRoundTimeMs', () => {
  it('truncates to hundredths', () => {
    expect(formatRoundTimeMs(1999)).toBe('01.99');
    expect(formatRoundTimeMs(12345)).toBe('12.34');
  });
});

describe('findNextRoundToShoot', () => {
  it('picks the next pending round first', () => {
    const card = buildCard({ id: '1', rounds: [{ status: 'timed', timeMs: 1 }, { status: 'rs' }, { status: 'pending' }, { status: 'pending' }, { status: 'skipped' }] });

    expect(findNextRoundToShoot(card)).toBe(3);
  });

  it('then an open reshoot, then a catch-up round', () => {
    const withReshoot = buildCard({ id: '1', rounds: [{ status: 'skipped' }, { status: 'rs' }, { status: 'timed', timeMs: 1 }, { status: 'timed', timeMs: 1 }, { status: 'timed', timeMs: 1 }] });
    const withCatchUp = buildCard({ id: '2', rounds: [{ status: 'skipped' }, { status: 'rs', reshootTimeMs: 1 }, { status: 'timed', timeMs: 1 }, { status: 'timed', timeMs: 1 }, { status: 'dnf' }] });

    expect(findNextRoundToShoot(withReshoot)).toBe(2);
    expect(findNextRoundToShoot(withCatchUp)).toBe(1);
  });

  it('is null when nothing is left to shoot', () => {
    const card = buildCard({ id: '1', rounds: Array.from({ length: 5 }, () => ({ status: 'dnf' as const })) });

    expect(findNextRoundToShoot(card)).toBeNull();
  });
});

describe('parseTimeInput', () => {
  it('parses seconds with a dot or comma', () => {
    expect(parseTimeInput('12.34')).toBe(12340);
    expect(parseTimeInput(' 7,5 ')).toBe(7500);
    expect(parseTimeInput('9')).toBe(9000);
  });

  it('rejects anything that is not a time', () => {
    expect(parseTimeInput('')).toBeNull();
    expect(parseTimeInput('abc')).toBeNull();
    expect(parseTimeInput('-1')).toBeNull();
  });
});
