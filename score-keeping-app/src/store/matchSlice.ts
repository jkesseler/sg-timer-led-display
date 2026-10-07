import { createSlice, isAnyOf } from '@reduxjs/toolkit';
import { buildPendingRounds } from '@/lib/match/buildMatchState';
import { deriveCurrentRound, getSquadCards } from '@/lib/match/derive';
import type { Card, MatchState, Round } from '@/lib/match/types';
import type {
  AddCardPayload,
  AssignResultPayload,
  CardRef,
  DisqualifyPayload,
  MatchSliceState,
  MaybeMetaAction,
  MetaAction,
  NewCard,
  ReorderQueuePayload,
  ReshootTimePayload,
  ResultRef,
  RoundRef,
  RoundStatusPayload,
  RoundTimePayload,
  ShooterRef,
  SquadStatusPayload,
  SyncStatus,
  TurnStoppedPayload,
  UnassignedResultPayload
} from './types';
import type { PayloadAction } from '@reduxjs/toolkit';

const initialState: MatchSliceState = { current: null, syncStatus: 'synced' };

// Every action carrying this meta is a user-visible change: it bumps the
// revision and is written to the audit log by the sync middleware.
function withMeta<P>(payload: P) {
  return { payload, meta: { id: crypto.randomUUID(), at: new Date().toISOString() } };
}

function findCard(match: MatchState, cardId: string): Card | undefined {
  return match.cards.find(card => card.id === cardId);
}

function findRound(match: MatchState, { cardId, round }: RoundRef): Round | undefined {
  return findCard(match, cardId)?.rounds.find(candidate => candidate.n === round);
}

/** A timer result lands on the reshoot slot of an RS round, and on the round itself otherwise. */
function applyResult(round: Round, timeMs: number) {
  if (round.status === 'rs') {
    round.reshootTimeMs = timeMs;

    return;
  }

  round.status = 'timed';
  round.timeMs = timeMs;
}

/** Rounds the rest of the squad has already moved past become catch-up rounds for a late or returning shooter. */
function skipPassedRounds(match: MatchState, card: Card) {
  const otherCards = getSquadCards(match, card.squadId).filter(candidate => candidate.id !== card.id);
  const currentRound = deriveCurrentRound(otherCards);

  for (const round of card.rounds) {
    const isPassed = currentRound === null || round.n < currentRound;
    if (round.status === 'pending' && isPassed) {
      round.status = 'skipped';
    }
  }
}

export const matchSlice = createSlice({
  name: 'match',
  initialState,
  reducers: {
    hydrate(state, action) {
      state.current = action.payload;
    },

    syncStatusChanged(state, action: PayloadAction<SyncStatus>) {
      state.syncStatus = action.payload;
    },

    armTurn: {
      reducer(state, action: MetaAction<RoundRef>) {
        const match = state.current;
        const card = match ? findCard(match, action.payload.cardId) : undefined;
        if (!match || !card) {
          return;
        }

        match.activeTurn = { cardId: card.id, round: action.payload.round, phase: 'armed' };

        const squad = match.squads.find(candidate => candidate.id === card.squadId);
        if (squad?.status === 'scheduled') {
          squad.status = 'active';
        }
      },
      prepare: withMeta<RoundRef>
    },

    cancelTurn: {
      reducer(state) {
        if (state.current) {
          state.current.activeTurn = null;
        }
      },
      prepare: () => withMeta(null)
    },

    turnStarted: {
      reducer(state) {
        const activeTurn = state.current?.activeTurn;
        if (activeTurn) {
          activeTurn.phase = 'running';
        }
      },
      prepare: () => withMeta(null)
    },

    /** No lastShotTimeMs means no shots were fired: the turn is discarded and the round stays as it was. */
    turnStopped: {
      reducer(state, action: MetaAction<TurnStoppedPayload>) {
        const match = state.current;
        const activeTurn = match?.activeTurn;
        if (!match || !activeTurn) {
          return;
        }

        const round = findRound(match, activeTurn);
        if (round && action.payload.lastShotTimeMs !== null) {
          applyResult(round, action.payload.lastShotTimeMs);
        }
        match.activeTurn = null;
      },
      prepare: withMeta<TurnStoppedPayload>
    },

    /** Typing a time onto a pending round times it; clearing a timed round's time makes it pending again. */
    setRoundTime: {
      reducer(state, action: MetaAction<RoundTimePayload>) {
        const round = state.current ? findRound(state.current, action.payload) : undefined;
        if (!round) {
          return;
        }

        round.timeMs = action.payload.timeMs;

        const isUnshot = round.status === 'pending' || round.status === 'skipped';
        if (action.payload.timeMs !== null && isUnshot) {
          round.status = 'timed';
        }
        if (action.payload.timeMs === null && round.status === 'timed') {
          round.status = 'pending';
        }
      },
      prepare: withMeta<RoundTimePayload>
    },

    setRoundStatus: {
      reducer(state, action: MetaAction<RoundStatusPayload>) {
        const round = state.current ? findRound(state.current, action.payload) : undefined;
        if (round) {
          round.status = action.payload.status;
        }
      },
      prepare: withMeta<RoundStatusPayload>
    },

    setReshootTime: {
      reducer(state, action: MetaAction<ReshootTimePayload>) {
        const round = state.current ? findRound(state.current, action.payload) : undefined;
        if (round) {
          round.reshootTimeMs = action.payload.reshootTimeMs;
        }
      },
      prepare: withMeta<ReshootTimePayload>
    },

    flagRs: {
      reducer(state, action: MetaAction<RoundRef>) {
        const round = state.current ? findRound(state.current, action.payload) : undefined;
        if (round) {
          round.status = 'rs';
        }
      },
      prepare: withMeta<RoundRef>
    },

    flagDnf: {
      reducer(state, action: MetaAction<RoundRef>) {
        const round = state.current ? findRound(state.current, action.payload) : undefined;
        if (round) {
          round.status = 'dnf';
        }
      },
      prepare: withMeta<RoundRef>
    },

    disqualify: {
      reducer(state, action: MetaAction<DisqualifyPayload>) {
        const card = state.current ? findCard(state.current, action.payload.cardId) : undefined;
        if (card) {
          card.dq = { reason: action.payload.reason, at: action.meta.at };
        }
      },
      prepare: withMeta<DisqualifyPayload>
    },

    /** Clears the DQ from every card of the shooter, since a DQ on one card voids them all. */
    reinstate: {
      reducer(state, action: MetaAction<ShooterRef>) {
        for (const card of state.current?.cards ?? []) {
          if (card.shooterId === action.payload.shooterId) {
            card.dq = null;
          }
        }
      },
      prepare: withMeta<ShooterRef>
    },

    markAbsent: {
      reducer(state, action: MetaAction<CardRef>) {
        const card = state.current ? findCard(state.current, action.payload.cardId) : undefined;
        if (card) {
          card.presence = 'absent';
        }
      },
      prepare: withMeta<CardRef>
    },

    markPresent: {
      reducer(state, action: MetaAction<CardRef>) {
        const match = state.current;
        const card = match ? findCard(match, action.payload.cardId) : undefined;
        if (!match || !card) {
          return;
        }

        card.presence = 'present';
        skipPassedRounds(match, card);
      },
      prepare: withMeta<CardRef>
    },

    reorderQueue: {
      reducer(state, action: MetaAction<ReorderQueuePayload>) {
        const match = state.current;
        if (!match) {
          return;
        }

        action.payload.cardIds.forEach((cardId, index) => {
          const card = findCard(match, cardId);
          if (card) {
            card.queuePosition = index;
          }
        });
      },
      prepare: withMeta<ReorderQueuePayload>
    },

    /** A late shooter joins at the back of the queue; rounds the squad already shot become catch-up rounds. */
    addCard: {
      reducer(state, action: MetaAction<AddCardPayload>) {
        const match = state.current;
        if (!match) {
          return;
        }

        const squadCards = getSquadCards(match, action.payload.squadId);
        const card: Card = {
          id: action.payload.cardId,
          squadId: action.payload.squadId,
          shooterId: action.payload.shooterId,
          shooterName: action.payload.shooterName,
          knsaNumber: action.payload.knsaNumber,
          discipline: action.payload.discipline,
          queuePosition: squadCards.length,
          presence: 'present',
          rounds: buildPendingRounds(),
          dq: null,
          signedOffAt: null
        };

        skipPassedRounds(match, card);
        match.cards.push(card);
      },
      prepare: (card: NewCard) => withMeta({ ...card, cardId: crypto.randomUUID() })
    },

    signOff: {
      reducer(state, action: MetaAction<CardRef>) {
        const card = state.current ? findCard(state.current, action.payload.cardId) : undefined;
        if (card) {
          card.signedOffAt = action.meta.at;
        }
      },
      prepare: withMeta<CardRef>
    },

    unsign: {
      reducer(state, action: MetaAction<CardRef>) {
        const card = state.current ? findCard(state.current, action.payload.cardId) : undefined;
        if (card) {
          card.signedOffAt = null;
        }
      },
      prepare: withMeta<CardRef>
    },

    setSquadStatus: {
      reducer(state, action: MetaAction<SquadStatusPayload>) {
        const squad = state.current?.squads.find(candidate => candidate.id === action.payload.squadId);
        if (squad) {
          squad.status = action.payload.status;
        }
      },
      prepare: withMeta<SquadStatusPayload>
    },

    addUnassignedResult: {
      reducer(state, action: MetaAction<UnassignedResultPayload>) {
        state.current?.unassignedResults.push({ id: action.meta.id, timeMs: action.payload.timeMs, at: action.meta.at });
      },
      prepare: withMeta<UnassignedResultPayload>
    },

    assignResult: {
      reducer(state, action: MetaAction<AssignResultPayload>) {
        const match = state.current;
        const result = match?.unassignedResults.find(candidate => candidate.id === action.payload.resultId);
        const round = match ? findRound(match, action.payload) : undefined;
        if (!match || !result || !round) {
          return;
        }

        applyResult(round, result.timeMs);
        match.unassignedResults = match.unassignedResults.filter(candidate => candidate.id !== result.id);
      },
      prepare: withMeta<AssignResultPayload>
    },

    discardUnassignedResult: {
      reducer(state, action: MetaAction<ResultRef>) {
        const match = state.current;
        if (match) {
          match.unassignedResults = match.unassignedResults.filter(candidate => candidate.id !== action.payload.resultId);
        }
      },
      prepare: withMeta<ResultRef>
    }
  },
  extraReducers: (builder) => {
    builder.addMatcher(isMatchChange, (state) => {
      if (state.current) {
        state.current.revision += 1;
      }
    });
  }
});

export function isMatchChange(action: MaybeMetaAction): action is MetaAction<object | null> {
  return action.type.startsWith(`${matchSlice.name}/`) && action.meta !== undefined;
}

export const {
  hydrate,
  syncStatusChanged,
  armTurn,
  cancelTurn,
  turnStarted,
  turnStopped,
  setRoundTime,
  setRoundStatus,
  setReshootTime,
  flagRs,
  flagDnf,
  disqualify,
  reinstate,
  markAbsent,
  markPresent,
  reorderQueue,
  addCard,
  signOff,
  unsign,
  setSquadStatus,
  addUnassignedResult,
  assignResult,
  discardUnassignedResult
} = matchSlice.actions;

// Results are what a debounce must never delay; everything else can batch.
export const isResultAction = isAnyOf(
  turnStopped,
  setRoundTime,
  setRoundStatus,
  setReshootTime,
  addUnassignedResult,
  assignResult
);
