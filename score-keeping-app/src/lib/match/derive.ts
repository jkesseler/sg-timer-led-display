import type { Card, CardScore, CardWarning, MatchState, OutstandingItem, RosterInfo } from './types';

export const NO_SCORE = '--:--';

/** SS.CC, truncated rather than rounded, so a slower time can never display as faster. */
export function formatRoundTimeMs(timeMs: number): string {
  const centiseconds = Math.floor(timeMs / 10);

  return formatCentiseconds(centiseconds);
}

/** Averages round to the nearest hundredth, unlike round times. */
export function formatScore(score: CardScore): string {
  if (score.status === 'scored') {
    return formatCentiseconds(Math.round(score.scoreMs / 10));
  }
  if (score.status === 'dq') {
    return 'DQ';
  }
  if (score.status === 'uncountable') {
    return NO_SCORE;
  }

  return '—';
}

function formatCentiseconds(centiseconds: number): string {
  const seconds = Math.floor(centiseconds / 100);
  const remainderCentiseconds = centiseconds % 100;

  return `${String(seconds).padStart(2, '0')}.${String(remainderCentiseconds).padStart(2, '0')}`;
}

export function getSquadCards(state: MatchState, squadId: string): Card[] {
  return state.cards
    .filter(card => card.squadId === squadId)
    .sort((a, b) => a.queuePosition - b.queuePosition);
}

/** The lowest pending round among present cards; null once only reshoots and catch-ups remain. */
export function deriveCurrentRound(cards: Card[]): number | null {
  let lowest: number | null = null;

  for (const card of cards) {
    if (card.presence !== 'present') {
      continue;
    }
    for (const round of card.rounds) {
      if (round.status === 'pending' && (lowest === null || round.n < lowest)) {
        lowest = round.n;
      }
    }
  }

  return lowest;
}

/** From the live queue, never the starting order. Null once the round is done; deriveOutstanding takes over. */
export function deriveUpcomingShooters(cards: Card[], currentRound: number | null, activeCardId: string | null) {
  if (currentRound === null) {
    return { next: null, onDeck: null };
  }

  const waiting = cards
    .filter(card => card.presence === 'present' && card.id !== activeCardId)
    .filter(card => card.rounds.some(round => round.n === currentRound && round.status === 'pending'))
    .sort((a, b) => a.queuePosition - b.queuePosition);

  return { next: waiting[0] ?? null, onDeck: waiting[1] ?? null };
}

/** Open reshoots and skipped (catch-up) rounds by round number; the timekeeper may take them in any order. */
export function deriveOutstanding(cards: Card[]): OutstandingItem[] {
  const items: OutstandingItem[] = [];

  for (const card of cards) {
    if (card.presence !== 'present') {
      continue;
    }
    for (const round of card.rounds) {
      if (round.status === 'rs' && round.reshootTimeMs === null) {
        items.push({ card, kind: 'rs', round: round.n });
      } else if (round.status === 'skipped') {
        items.push({ card, kind: 'skipped', round: round.n });
      }
    }
  }

  return items.sort((a, b) => a.round - b.round);
}

export function isReadyForSignOff(card: Card): boolean {
  const isAllShot = card.rounds.every(round => round.status !== 'pending');
  const hasUnresolvedRs = card.rounds.some(round => round.status === 'rs' && round.reshootTimeMs === null);

  return isAllShot && !hasUnresolvedRs;
}

/** Rule breaches are shown, never enforced — Range Office may override any of them. */
export function getCardWarnings(card: Card): CardWarning[] {
  const warnings: CardWarning[] = [];

  if (card.rounds.filter(round => round.status === 'rs').length > 1) {
    warnings.push('multiple-rs');
  }
  if (card.signedOffAt !== null && !isReadyForSignOff(card)) {
    warnings.push('signed-with-open-rounds');
  }

  return warnings;
}

/** The selected squad's card wins, so a shooter in two squads is armed where the timekeeper is working. */
export function findCardByKnsa(state: MatchState, knsa: string, squadId: string | null): Card | null {
  const code = knsa.trim();
  if (!code) {
    return null;
  }

  const matches = state.cards.filter(card => card.knsaNumber === code);

  return matches.find(card => card.squadId === squadId) ?? matches[0] ?? null;
}

/** The squad in play: the active turn's squad, else the first squad marked active. */
export function findCurrentSquadId(state: MatchState): string | null {
  if (state.activeTurn) {
    const activeCardId = state.activeTurn.cardId;
    const activeCard = state.cards.find(card => card.id === activeCardId);
    if (activeCard) {
      return activeCard.squadId;
    }
  }

  return state.squads.find(squad => squad.status === 'active')?.id ?? null;
}

export function deriveRoster(state: MatchState): RosterInfo {
  const squadId = findCurrentSquadId(state);
  if (!squadId) {
    return { current: null, next: null, onDeck: null };
  }

  const cards = getSquadCards(state, squadId);
  const activeCardId = state.activeTurn?.cardId ?? null;
  const current = cards.find(card => card.id === activeCardId)?.shooterName ?? null;
  const currentRound = deriveCurrentRound(cards);

  if (currentRound !== null) {
    const { next, onDeck } = deriveUpcomingShooters(cards, currentRound, activeCardId);

    return { current, next: next?.shooterName ?? null, onDeck: onDeck?.shooterName ?? null };
  }

  const outstanding = deriveOutstanding(cards).filter(item => item.card.id !== activeCardId);
  const next = outstanding[0] ? `${outstanding[0].card.shooterName} (reshoot)` : null;
  const onDeck = outstanding[1] ? `${outstanding[1].card.shooterName} (reshoot)` : null;

  return { current, next, onDeck };
}

/** The round a shooter shoots when armed: the next pending round, then an open reshoot, then a catch-up round. */
export function findNextRoundToShoot(card: Card): number | null {
  const pending = card.rounds.find(round => round.status === 'pending');
  if (pending) {
    return pending.n;
  }

  const openReshoot = card.rounds.find(round => round.status === 'rs' && round.reshootTimeMs === null);
  if (openReshoot) {
    return openReshoot.n;
  }

  return card.rounds.find(round => round.status === 'skipped')?.n ?? null;
}

const TIME_INPUT_PATTERN = /^\d{1,3}([.,]\d{1,3})?$/;

/** Parses a typed time in seconds ("12.34" or "12,34") to ms; null when it is not a time. */
export function parseTimeInput(text: string): number | null {
  const trimmed = text.trim();
  if (!TIME_INPUT_PATTERN.test(trimmed)) {
    return null;
  }

  return Math.round(Number(trimmed.replace(',', '.')) * 1000);
}
