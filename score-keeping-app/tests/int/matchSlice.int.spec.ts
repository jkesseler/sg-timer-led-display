import { describe, it, expect } from 'vitest';
import { buildMatchState } from '@/lib/match/buildMatchState';
import { getCardWarnings } from '@/lib/match/derive';
import { cardScore } from '@/lib/match/score';
import {
  addCard,
  armTurn,
  assignResult,
  disqualify,
  flagDnf,
  flagRs,
  hydrate,
  markAbsent,
  markPresent,
  matchSlice,
  reinstate,
  reorderQueue,
  setRoundStatus,
  setRoundTime,
  signOff,
  turnStopped,
  unsign
} from '@/store/matchSlice';
import { buildCard, buildState, timedRounds } from './matchFixtures';
import type { UnknownAction } from '@reduxjs/toolkit';
import type { MatchState } from '@/lib/match/types';

function run(state: MatchState, ...actions: UnknownAction[]): MatchState {
  let slice = matchSlice.reducer(undefined, hydrate(state));
  for (const action of actions) {
    slice = matchSlice.reducer(slice, action);
  }

  if (!slice.current) {
    throw new Error('match state vanished');
  }

  return slice.current;
}

describe('buildMatchState', () => {
  it('creates one pending card per member in starting order', () => {
    const state = buildMatchState({
      matchId: 'm',
      deviceId: 'ABC123',
      squads: [{ id: 's', label: 'A', startTime: '08:00', endTime: '09:00' }],
      members: [
        { id: 'late', squadId: 's', shooterId: 'p2', startingPosition: 2, discipline: 'SKP' },
        { id: 'early', squadId: 's', shooterId: 'p1', startingPosition: 1, discipline: 'OKP' }
      ],
      shooters: [
        { id: 'p1', firstName: 'Jan', lastName: 'Jansen', knsaNumber: '111' },
        { id: 'p2', firstName: 'Piet', lastName: 'Pietersen', knsaNumber: null }
      ]
    });

    expect(state.cards.map(card => [card.id, card.queuePosition, card.shooterName, card.discipline])).toEqual([
      ['early', 0, 'Jan Jansen', 'OKP'],
      ['late', 1, 'Piet Pietersen', 'SKP']
    ]);
    expect(state.cards[0].rounds.every(round => round.status === 'pending')).toBe(true);
    expect(state.squads[0].status).toBe('scheduled');
  });
});

describe('turns', () => {
  it('records the timer result on the armed round and activates the squad', () => {
    const state = run(buildState([buildCard({ id: 'a' })], { squads: [{ id: 'squad-a', label: 'A', start: '', end: '', status: 'scheduled' }] }),
      armTurn({ cardId: 'a', round: 1 }),
      turnStopped({ lastShotTimeMs: 4321 }));

    expect(state.cards[0].rounds[0]).toMatchObject({ status: 'timed', timeMs: 4321 });
    expect(state.activeTurn).toBeNull();
    expect(state.squads[0].status).toBe('active');
  });

  it('discards a turn without shots and leaves the round pending', () => {
    const state = run(buildState([buildCard({ id: 'a' })]),
      armTurn({ cardId: 'a', round: 1 }),
      turnStopped({ lastShotTimeMs: null }));

    expect(state.cards[0].rounds[0].status).toBe('pending');
    expect(state.activeTurn).toBeNull();
  });

  it('puts a reshoot result on the RS round\'s reshoot slot', () => {
    const state = run(buildState([buildCard({ id: 'a' })]),
      armTurn({ cardId: 'a', round: 2 }),
      turnStopped({ lastShotTimeMs: 9000 }),
      flagRs({ cardId: 'a', round: 2 }),
      armTurn({ cardId: 'a', round: 2 }),
      turnStopped({ lastShotTimeMs: 3000 }));

    expect(state.cards[0].rounds[1]).toMatchObject({ status: 'rs', timeMs: 9000, reshootTimeMs: 3000 });
  });

  it('bumps the revision on every change', () => {
    const state = run(buildState([buildCard({ id: 'a' })]),
      armTurn({ cardId: 'a', round: 1 }),
      turnStopped({ lastShotTimeMs: 1 }));

    expect(state.revision).toBe(2);
  });
});

describe('Range Office is always right: "illegal" edits are accepted and only warned about', () => {
  it('allows a second RS', () => {
    const state = run(buildState([buildCard({ id: 'a' })]),
      flagRs({ cardId: 'a', round: 1 }),
      flagRs({ cardId: 'a', round: 2 }));

    expect(state.cards[0].rounds.slice(0, 2).map(round => round.status)).toEqual(['rs', 'rs']);
    expect(getCardWarnings(state.cards[0])).toContain('multiple-rs');
  });

  it('allows editing a round after sign-off', () => {
    const card = { ...buildCard({ id: 'a' }), rounds: timedRounds(1000, 1000, 1000, 1000, 1000) };
    const state = run(buildState([card]),
      signOff({ cardId: 'a' }),
      setRoundTime({ cardId: 'a', round: 3, timeMs: 700 }));

    expect(state.cards[0].signedOffAt).not.toBeNull();
    expect(state.cards[0].rounds[2].timeMs).toBe(700);
  });

  it('allows signing off with open rounds, with a warning, and unsigning', () => {
    const signed = run(buildState([buildCard({ id: 'a' })]), signOff({ cardId: 'a' }));

    expect(getCardWarnings(signed.cards[0])).toContain('signed-with-open-rounds');

    const unsigned = run(signed, unsign({ cardId: 'a' }));

    expect(unsigned.cards[0].signedOffAt).toBeNull();
  });

  it('allows changing a timed value and undoing a DNF', () => {
    const card = { ...buildCard({ id: 'a' }), rounds: timedRounds(1000, 1000, 1000, 1000, 1000) };
    const state = run(buildState([card]),
      setRoundTime({ cardId: 'a', round: 1, timeMs: 1500 }),
      flagDnf({ cardId: 'a', round: 2 }),
      setRoundStatus({ cardId: 'a', round: 2, status: 'timed' }));

    expect(state.cards[0].rounds[0].timeMs).toBe(1500);
    expect(state.cards[0].rounds[1]).toMatchObject({ status: 'timed', timeMs: 1000 });
  });

  it('clearing a typed time makes the round pending again', () => {
    const state = run(buildState([buildCard({ id: 'a' })]),
      setRoundTime({ cardId: 'a', round: 1, timeMs: 2000 }),
      setRoundTime({ cardId: 'a', round: 1, timeMs: null }));

    expect(state.cards[0].rounds[0]).toMatchObject({ status: 'pending', timeMs: null });
  });

  it('allows a DQ and its reversal across all of the shooter\'s cards', () => {
    const cards = [
      { ...buildCard({ id: 'a', shooterId: 'jan' }), rounds: timedRounds(1000, 1000, 1000, 1000, 1000) },
      { ...buildCard({ id: 'b', shooterId: 'jan', squadId: 'squad-b' }), rounds: timedRounds(1000, 1000, 1000, 1000, 1000) }
    ];
    const disqualified = run(buildState(cards), disqualify({ cardId: 'a', reason: 'unsafe' }));

    expect(cardScore(disqualified, disqualified.cards[1])).toEqual({ status: 'dq' });

    const reinstated = run(disqualified, reinstate({ shooterId: 'jan' }));

    expect(cardScore(reinstated, reinstated.cards[1])).toEqual({ status: 'scored', scoreMs: 1000 });
  });
});

describe('presence, queue and late shooters', () => {
  it('a returning shooter gets the rounds the squad moved past as catch-up rounds', () => {
    const other = buildCard({ id: 'b', rounds: [{ status: 'timed', timeMs: 1 }, { status: 'timed', timeMs: 1 }, { status: 'pending' }, { status: 'pending' }, { status: 'pending' }] });
    const state = run(buildState([buildCard({ id: 'a' }), other]),
      markAbsent({ cardId: 'a' }),
      markPresent({ cardId: 'a' }));

    expect(state.cards[0].rounds.map(round => round.status)).toEqual(['skipped', 'skipped', 'pending', 'pending', 'pending']);
  });

  it('reorders the queue', () => {
    const state = run(buildState([buildCard({ id: 'a', queuePosition: 0 }), buildCard({ id: 'b', queuePosition: 1 })]),
      reorderQueue({ cardIds: ['b', 'a'] }));

    expect(state.cards.map(card => [card.id, card.queuePosition])).toEqual([['a', 1], ['b', 0]]);
  });

  it('adds a late shooter at the back of the queue', () => {
    const state = run(buildState([buildCard({ id: 'a', queuePosition: 0 })]),
      addCard({ squadId: 'squad-a', shooterId: 'late', shooterName: 'Late Comer', knsaNumber: '999', discipline: 'SKKP' }));

    const lateCard = state.cards[1];

    expect(lateCard.shooterName).toBe('Late Comer');
    expect(lateCard.discipline).toBe('SKKP');
    expect(lateCard.queuePosition).toBe(1);
    expect(lateCard.id).toMatch(/^[0-9a-f-]{36}$/);
  });
});

describe('unassigned results', () => {
  it('assigns a stray result to a chosen round', () => {
    const withResult = buildState([buildCard({ id: 'a' })], {
      unassignedResults: [{ id: 'r1', timeMs: 5555, at: '2026-09-30T10:00:00Z' }]
    });

    const state = run(withResult, assignResult({ resultId: 'r1', cardId: 'a', round: 1 }));

    expect(state.cards[0].rounds[0]).toMatchObject({ status: 'timed', timeMs: 5555 });
    expect(state.unassignedResults).toEqual([]);
  });
});
