import type { Card, MatchState, Round } from './types';

const COUNTED_ROUNDS = 3;

interface ScoredCard {
  status: 'scored';
  scoreMs: number;
}

interface UnscoredCard {
  status: 'pending' | 'uncountable' | 'dq';
}

export type CardScore = ScoredCard | UnscoredCard;

/** The time a round contributes to the score, or null when it does not count (pending, DNF, skipped, RS without reshoot). */
export function countableTimeMs(round: Round): number | null {
  if (round.status === 'timed') {
    return round.timeMs;
  }
  if (round.status === 'rs') {
    return round.reshootTimeMs;
  }

  return null;
}

function isRoundResolved(round: Round): boolean {
  if (round.status === 'rs') {
    return round.reshootTimeMs !== null;
  }

  return round.status === 'timed' || round.status === 'dnf';
}

/**
 * Mean of the three fastest countable rounds, unrounded, in ms. Ignores DQ —
 * use cardScore for the match-wide DQ rule. Only final once every round is
 * resolved: an outstanding round could still be one of the fastest three.
 */
export function finalScoreMs(card: Card): CardScore {
  if (!card.rounds.every(isRoundResolved)) {
    return { status: 'pending' };
  }

  const countableTimes = card.rounds
    .map(countableTimeMs)
    .filter((timeMs): timeMs is number => timeMs !== null)
    .sort((a, b) => a - b);

  if (countableTimes.length < COUNTED_ROUNDS) {
    return { status: 'uncountable' };
  }

  const fastest = countableTimes.slice(0, COUNTED_ROUNDS);
  const totalMs = fastest.reduce((sum, timeMs) => sum + timeMs, 0);

  return { status: 'scored', scoreMs: totalMs / COUNTED_ROUNDS };
}

/** A DQ on any of the shooter's cards voids every card of theirs in the match. */
export function isShooterDisqualified(state: MatchState, shooterId: string): boolean {
  return state.cards.some(card => card.shooterId === shooterId && card.dq !== null);
}

export function cardScore(state: MatchState, card: Card): CardScore {
  if (isShooterDisqualified(state, card.shooterId)) {
    return { status: 'dq' };
  }

  return finalScoreMs(card);
}

export function matchScoresForShooter(state: MatchState, shooterId: string) {
  return state.cards
    .filter(card => card.shooterId === shooterId)
    .map(card => ({ cardId: card.id, score: cardScore(state, card) }));
}
