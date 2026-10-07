import { createSelector } from '@reduxjs/toolkit';
import {
  deriveCurrentRound,
  deriveOutstanding,
  deriveUpcomingShooters,
  findCurrentSquadId,
  findNextRoundToShoot,
  formatRoundTimeMs,
  formatScore,
  getCardWarnings,
  getSquadCards,
  isReadyForSignOff
} from '@/lib/match/derive';
import { cardScore, countableTimeMs, isShooterDisqualified } from '@/lib/match/score';
import { DisplayState } from '@/lib/mqtt/types';
import type { Card, CardScore, CardWarning, Round } from '@/lib/match/types';
import { selectDisplayState, selectIsConnected, selectKnownDevices, selectShots } from './mqttSlice';
import type { RootState } from './store';
import type {
  CardRowView,
  CellState,
  CountableRound,
  RoundCellView,
  RoundEditorView,
  ScoreSheetRound,
  ScoreSheetView
} from './types';

// Reselect 5's default memoizer caches per argument set, so the
// parameterized selectors below keep one cached result per card / round.

const EMPTY_CARDS: Card[] = [];

const selectCardIdArgument = (_state: RootState, cardId: string) => cardId;
const selectRoundArgument = (_state: RootState, _cardId: string, round: number) => round;

export const selectMatch = (state: RootState) => state.match.current;
export const selectSyncStatus = (state: RootState) => state.match.syncStatus;
export const selectTimekeeper = (state: RootState) => state.timekeeper;
export const selectLoadStatus = (state: RootState) => state.timekeeper.loadStatus;
export const selectLoadError = (state: RootState) => state.timekeeper.loadError;
export const selectUserEmail = (state: RootState) => state.timekeeper.userEmail;
export const selectMatchLabel = (state: RootState) => state.timekeeper.matchLabel;
export const selectMessage = (state: RootState) => state.timekeeper.message;
export const selectEditor = (state: RootState) => state.timekeeper.editor;
export const selectDqDialog = (state: RootState) => state.timekeeper.dqDialog;
export const selectLateShooterDraft = (state: RootState) => state.timekeeper.lateShooter;
const selectShooters = (state: RootState) => state.timekeeper.shooters;
const selectUnassignedDrafts = (state: RootState) => state.timekeeper.unassignedDrafts;
const selectPickedSquadId = (state: RootState) => state.timekeeper.selectedSquadId;

export const selectActiveTurn = (state: RootState) => state.match.current?.activeTurn ?? null;

/** The barcode scanner arms shooters only while nobody is armed or shooting. */
export const selectIsScannerListening = (state: RootState) => state.match.current !== null && selectActiveTurn(state) === null;

/** The picked tab, else the squad in play, else the first squad. */
export const selectSelectedSquadId = createSelector(
  [selectMatch, selectPickedSquadId],
  (match, pickedSquadId) => {
    if (!match) {
      return null;
    }

    return pickedSquadId ?? findCurrentSquadId(match) ?? match.squads[0]?.id ?? null;
  }
);

export const selectSquads = createSelector([selectMatch], match => match?.squads ?? []);

export const selectSelectedSquad = createSelector(
  [selectSquads, selectSelectedSquadId],
  (squads, squadId) => squads.find(squad => squad.id === squadId) ?? null
);

export const selectSquadCards = createSelector(
  [selectMatch, selectSelectedSquadId],
  (match, squadId) => (match && squadId ? getSquadCards(match, squadId) : EMPTY_CARDS)
);

export const selectPresentCards = createSelector([selectSquadCards], cards => cards.filter(card => card.presence === 'present'));
export const selectAbsentCards = createSelector([selectSquadCards], cards => cards.filter(card => card.presence === 'absent'));
export const selectPresentCardIds = createSelector([selectPresentCards], cards => cards.map(card => card.id));
export const selectAbsentCardIds = createSelector([selectAbsentCards], cards => cards.map(card => card.id));

export const selectCurrentRound = createSelector([selectSquadCards], cards => deriveCurrentRound(cards));

export const selectActiveCard = createSelector(
  [selectMatch, selectActiveTurn],
  (match, activeTurn) => match?.cards.find(card => card.id === activeTurn?.cardId) ?? null
);

export const selectSquadStatusView = createSelector(
  [selectSelectedSquad, selectCurrentRound, selectActiveTurn, selectActiveCard],
  (squad, currentRound, activeTurn, activeCard) => {
    if (!squad) {
      return null;
    }

    return {
      squadId: squad.id,
      squadStatus: squad.status,
      roundLabel: currentRound !== null ? `Round ${currentRound} of 5` : 'Reshoot / catch-up phase',
      turn: activeTurn && activeCard
        ? { verb: activeTurn.phase === 'running' ? 'Shooting' : 'Armed', shooterName: activeCard.shooterName, round: activeTurn.round }
        : null
    };
  }
);

export const selectRoster = createSelector(
  [selectSquadCards, selectCurrentRound, selectActiveTurn],
  (cards, currentRound, activeTurn) => {
    const { next, onDeck } = deriveUpcomingShooters(cards, currentRound, activeTurn?.cardId ?? null);

    return { nextName: next?.shooterName ?? null, onDeckName: onDeck?.shooterName ?? null };
  }
);

export const selectOutstanding = createSelector(
  [selectSquadCards, selectCurrentRound],
  (cards, currentRound) =>
    currentRound === null
      ? deriveOutstanding(cards).map(item => ({
          cardId: item.card.id,
          shooterName: item.card.shooterName,
          round: item.round,
          kindLabel: item.kind === 'rs' ? 'reshoot' : 'catch-up'
        }))
      : []
);

export const selectAbsentRows = createSelector(
  [selectAbsentCards],
  cards => cards.map(card => ({ cardId: card.id, shooterName: card.shooterName }))
);

export const selectStatusLine = createSelector(
  [selectSyncStatus, selectMatch, selectKnownDevices, selectIsConnected],
  (syncStatus, match, knownDevices, isBrokerConnected) => {
    const deviceId = match?.deviceId ?? null;
    const timer = knownDevices.find(device => device.deviceId === deviceId);

    return { syncStatus, deviceId, isTimerOnline: timer?.presence === 'online', isBrokerConnected };
  }
);

export const selectLiveTimeMs = createSelector(
  [selectActiveTurn, selectDisplayState, selectShots],
  (activeTurn, displayState, shots) => {
    const hasLiveShots = activeTurn?.phase === 'running' && displayState !== DisplayState.SESSION_ENDED && shots.length > 0;

    return hasLiveShots ? shots[shots.length - 1].absoluteTimeMs : null;
  }
);

export const selectSplitsView = createSelector(
  [selectShots, selectDisplayState],
  (shots, displayState) => ({ shots, highlightExtremes: displayState === DisplayState.SESSION_ENDED })
);

const WARNING_LABELS: Record<CardWarning, string> = {
  'multiple-rs': 'more than one RS',
  'signed-with-open-rounds': 'signed with open rounds'
};

function describeRound(round: Round, liveTimeMs: number | null): Pick<RoundCellView, 'label' | 'modifier'> {
  if (liveTimeMs !== null) {
    return { label: formatRoundTimeMs(liveTimeMs), modifier: 'live' };
  }

  switch (round.status) {
    case 'timed':
      return { label: round.timeMs !== null ? formatRoundTimeMs(round.timeMs) : '—', modifier: 'timed' };
    case 'rs':
      return { label: 'RS', modifier: 'rs' };
    case 'dnf':
      return { label: '--:--', modifier: 'dnf' };
    case 'skipped':
      return { label: '—', modifier: 'skipped' };
    case 'pending':
      return { label: '—', modifier: 'pending' };
  }
}

function describeReshoot(round: Round, liveTimeMs: number | null): Pick<RoundCellView, 'label' | 'modifier'> {
  if (liveTimeMs !== null) {
    return { label: formatRoundTimeMs(liveTimeMs), modifier: 'live' };
  }

  return round.reshootTimeMs !== null
    ? { label: formatRoundTimeMs(round.reshootTimeMs), modifier: 'timed' }
    : { label: '—', modifier: 'pending' };
}

/** Arming or shooting an RS round means shooting its reshoot, so that shows on the reshoot cell. */
function buildRoundCells(rounds: Round[], { armedRound, liveRound, liveTimeMs, editedRound }: CellState): RoundCellView[] {
  const roundCells = rounds.map((round) => {
    const isReshootRound = round.status === 'rs';
    const isLive = liveRound === round.n && !isReshootRound;

    return {
      key: `R${round.n}`,
      heading: `R${round.n}`,
      n: round.n,
      ...describeRound(round, isLive ? liveTimeMs : null),
      isArmed: armedRound === round.n && !isReshootRound,
      isEditing: editedRound === round.n
    };
  });
  const reshootCells = rounds
    .filter(round => round.status === 'rs')
    .map(round => ({
      key: `RS${round.n}`,
      heading: 'RS',
      n: round.n,
      ...describeReshoot(round, liveRound === round.n ? liveTimeMs : null),
      isArmed: armedRound === round.n,
      isEditing: editedRound === round.n
    }));

  return [...roundCells, ...reshootCells];
}

export const selectCardById = createSelector(
  [selectMatch, selectCardIdArgument],
  (match, cardId) => match?.cards.find(card => card.id === cardId) ?? null
);

// Primitive per-card inputs: a change elsewhere in the match leaves them equal, so the row view stays cached.
const selectIsCardActive = (state: RootState, cardId: string) => state.match.current?.activeTurn?.cardId === cardId;
const selectArmedRoundForCard = (state: RootState, cardId: string) =>
  selectIsCardActive(state, cardId) ? state.match.current?.activeTurn?.round ?? null : null;
const selectLiveRoundForCard = (state: RootState, cardId: string) => {
  const activeTurn = state.match.current?.activeTurn;

  return activeTurn?.cardId === cardId && activeTurn.phase === 'running' ? activeTurn.round : null;
};
const selectLiveTimeMsForCard = (state: RootState, cardId: string) =>
  selectLiveRoundForCard(state, cardId) === null ? null : selectLiveTimeMs(state);
const selectEditedRoundForCard = (state: RootState, cardId: string) =>
  state.timekeeper.editor?.cardId === cardId ? state.timekeeper.editor.round : null;
const selectIsCardDisqualified = (state: RootState, cardId: string) => {
  const match = state.match.current;
  const card = selectCardById(state, cardId);

  return match !== null && card !== null && isShooterDisqualified(match, card.shooterId);
};
const selectCardScoreText = (state: RootState, cardId: string) => {
  const match = state.match.current;
  const card = selectCardById(state, cardId);

  return match && card ? formatScore(cardScore(match, card)) : '—';
};

export const selectCardRowView = createSelector(
  [
    selectCardById,
    selectIsCardActive,
    selectArmedRoundForCard,
    selectLiveRoundForCard,
    selectLiveTimeMsForCard,
    selectEditedRoundForCard,
    selectIsCardDisqualified,
    selectCardScoreText
  ],
  (card, isActive, armedRound, liveRound, liveTimeMs, editedRound, isDisqualified, scoreText): CardRowView | null => {
    if (!card) {
      return null;
    }

    return {
      cardId: card.id,
      shooterId: card.shooterId,
      shooterName: card.shooterName,
      // Absent on cards saved before discipline moved from squad to member.
      discipline: card.discipline ?? null,
      nextRound: findNextRoundToShoot(card),
      isActive,
      isDisqualified,
      isSignedOff: card.signedOffAt !== null,
      isReadyForSignOff: isReadyForSignOff(card),
      scoreText,
      warnings: getCardWarnings(card).map(warning => WARNING_LABELS[warning]),
      rounds: buildRoundCells(card.rounds, { armedRound, liveRound, liveTimeMs, editedRound })
    };
  }
);

export const selectIsEditorOpenForCard = (state: RootState, cardId: string) => selectEditedRoundForCard(state, cardId) !== null;

export const selectDqDialogForCard = createSelector(
  [selectDqDialog, selectCardById, selectCardIdArgument],
  (dqDialog, card, cardId) =>
    dqDialog?.cardId === cardId && card ? { shooterName: card.shooterName, reason: dqDialog.reason } : null
);

// Seconds with full ms precision, so saving an untouched value never changes it.
export function toSecondsText(timeMs: number | null): string {
  return timeMs === null ? '' : String(timeMs / 1000);
}

export const selectRound = createSelector(
  [selectCardById, selectRoundArgument],
  (card, round) => card?.rounds.find(candidate => candidate.n === round) ?? null
);

/** What the time input shows for this round: the draft while typing, else the stored time. */
export const selectRoundTimeText = createSelector(
  [selectRound, selectEditor, selectCardIdArgument, selectRoundArgument],
  (round, editor, cardId, roundNumber) => {
    const isEditingThisRound = editor?.cardId === cardId && editor.round === roundNumber;
    if (isEditingThisRound && editor.timeText !== null) {
      return editor.timeText;
    }

    return toSecondsText(round?.timeMs ?? null);
  }
);

/** Same for the reshoot input. */
export const selectRoundReshootText = createSelector(
  [selectRound, selectEditor, selectCardIdArgument, selectRoundArgument],
  (round, editor, cardId, roundNumber) => {
    const isEditingThisRound = editor?.cardId === cardId && editor.round === roundNumber;
    if (isEditingThisRound && editor.reshootText !== null) {
      return editor.reshootText;
    }

    return toSecondsText(round?.reshootTimeMs ?? null);
  }
);

export const selectRoundEditorView = createSelector(
  [
    selectCardById,
    selectRound,
    selectRoundTimeText,
    selectRoundReshootText,
    selectEditor,
    selectCardIdArgument,
    selectRoundArgument
  ],
  (card, round, timeText, reshootText, editor, cardId, roundNumber): RoundEditorView | null => {
    if (!card || !round) {
      return null;
    }

    return {
      cardId,
      round: roundNumber,
      shooterName: card.shooterName,
      status: round.status,
      timeText,
      reshootText,
      error: editor?.cardId === cardId && editor.round === roundNumber ? editor.error : null
    };
  }
);

export const selectLateShooterView = createSelector(
  [selectShooters, selectSquadCards, selectLateShooterDraft, selectSelectedSquadId],
  (shooters, squadCards, draft, squadId) => {
    // A shooter can be in one squad once per discipline.
    const available = shooters.filter(shooter =>
      !squadCards.some(card => card.shooterId === shooter.id && card.discipline === draft.discipline));

    return {
      squadId,
      shooters: available,
      shooterId: available.some(shooter => shooter.id === draft.shooterId) ? draft.shooterId : '',
      discipline: draft.discipline
    };
  }
);

export const selectUnassignedView = createSelector(
  [selectMatch, selectUnassignedDrafts, selectPresentCards],
  (match, drafts, presentCards) => {
    const shooterOptions = presentCards.map(card => ({ cardId: card.id, shooterName: card.shooterName }));
    const results = (match?.unassignedResults ?? []).map((result) => {
      const draft = drafts[result.id] ?? {};
      const cardId = draft.cardId && presentCards.some(card => card.id === draft.cardId) ? draft.cardId : shooterOptions[0]?.cardId ?? '';

      return {
        resultId: result.id,
        timeText: formatRoundTimeMs(result.timeMs),
        at: result.at,
        cardId,
        round: draft.round ?? 1
      };
    });

    return { results, shooterOptions };
  }
);

export const selectPrintRequest = (state: RootState) => state.timekeeper.printRequest;

function describeSheetRound(round: Round): string {
  switch (round.status) {
    case 'timed':
      return round.timeMs !== null ? formatRoundTimeMs(round.timeMs) : '—';
    case 'rs':
      return 'RS';
    case 'dnf':
      return 'DNF';
    case 'skipped':
      return 'skipped';
    case 'pending':
      return '—';
  }
}

/** The round numbers of the three fastest countable rounds; empty unless the card has a score (not pending, not DQ). */
function findCountedRounds(card: Card, score: CardScore): Set<number> {
  const countedRounds = new Set<number>();
  if (score.status !== 'scored') {
    return countedRounds;
  }

  const countable = card.rounds
    .map(round => ({ n: round.n, timeMs: countableTimeMs(round) }))
    .filter((round): round is CountableRound => round.timeMs !== null)
    .sort((a, b) => a.timeMs - b.timeMs);
  for (const round of countable.slice(0, 3)) {
    countedRounds.add(round.n);
  }

  return countedRounds;
}

/** A reshoot's time counts, not the round it replaces, so the counted mark goes on the RS row. */
function buildSheetRounds(rounds: Round[], countedRounds: Set<number>): ScoreSheetRound[] {
  const roundRows = rounds.map(round => ({
    key: `R${round.n}`,
    heading: `Round ${round.n}`,
    label: describeSheetRound(round),
    isCounted: round.status !== 'rs' && countedRounds.has(round.n)
  }));
  const reshootRows = rounds
    .filter(round => round.status === 'rs')
    .map(round => ({
      key: `RS${round.n}`,
      heading: 'RS',
      label: round.reshootTimeMs !== null ? formatRoundTimeMs(round.reshootTimeMs) : '—',
      isCounted: countedRounds.has(round.n)
    }));

  return [...roundRows, ...reshootRows];
}

const SIGNED_OFF_FORMAT = new Intl.DateTimeFormat('en-GB', { dateStyle: 'medium', timeStyle: 'short' });

export const selectScoreSheet = createSelector(
  [selectMatch, selectCardById, selectMatchLabel],
  (match, card, matchLabel): ScoreSheetView | null => {
    if (!match || !card) {
      return null;
    }

    const squad = match.squads.find(candidate => candidate.id === card.squadId);
    const dqCard = match.cards.find(candidate => candidate.shooterId === card.shooterId && candidate.dq !== null);
    const score = cardScore(match, card);
    const countedRounds = findCountedRounds(card, score);

    return {
      matchLabel: matchLabel ?? 'Match',
      squadLabel: squad?.label ?? '',
      // A squad without its own label already uses its times as the label.
      squadTimes: squad && squad.label !== `${squad.start} - ${squad.end}` ? `${squad.start} - ${squad.end}` : '',
      shooterName: card.shooterName,
      knsaNumber: card.knsaNumber,
      discipline: card.discipline ?? null,
      rounds: buildSheetRounds(card.rounds, countedRounds),
      scoreText: formatScore(score),
      dqReason: dqCard ? dqCard.dq?.reason || 'no reason given' : null,
      warnings: getCardWarnings(card).map(warning => WARNING_LABELS[warning]),
      signedOffText: card.signedOffAt ? SIGNED_OFF_FORMAT.format(new Date(card.signedOffAt)) : null
    };
  }
);
