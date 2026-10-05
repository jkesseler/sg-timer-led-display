import { createSlice } from '@reduxjs/toolkit';
import { DISCIPLINES } from '@/lib/domain/disciplines';
import type { ShooterOption } from '@/lib/match/bootstrap';
import type { Discipline } from '@/lib/domain/disciplines';
import { addCard, assignResult, discardUnassignedResult, disqualify, setReshootTime, setRoundTime, signOff } from './matchSlice';
import type { PayloadAction } from '@reduxjs/toolkit';

export type TimekeeperLoadStatus = 'loading' | 'ready' | 'no-match' | 'error';

/**
 * Text typed into the round editor. `null` means untouched: the input then
 * shows the stored round value, so it always follows the round being edited.
 */
export interface RoundEditorState {
  cardId: string;
  round: number;
  timeText: string | null;
  reshootText: string | null;
  error: string | null;
}

export interface DqDialogState {
  cardId: string;
  reason: string;
}

/** Choices for one unassigned result; absent fields fall back to defaults in the selectors. */
export interface UnassignedDraft {
  cardId?: string;
  round?: number;
}

export interface LateShooterDraft {
  shooterId: string;
  discipline: Discipline;
}

/**
 * A score sheet waiting to be printed. `requestNumber` grows with every
 * request, so reprinting the same card is a new request too.
 */
export interface PrintRequest {
  cardId: string;
  requestNumber: number;
}

export interface TimekeeperState {
  loadStatus: TimekeeperLoadStatus;
  loadError: string | null;
  userEmail: string | null;
  matchLabel: string | null;
  shooters: ShooterOption[];
  /** The tab the timekeeper picked; null follows the squad in play. */
  selectedSquadId: string | null;
  message: string | null;
  editor: RoundEditorState | null;
  dqDialog: DqDialogState | null;
  lateShooter: LateShooterDraft;
  unassignedDrafts: Record<string, UnassignedDraft>;
  printRequest: PrintRequest | null;
  printRequestCount: number;
}

const initialState: TimekeeperState = {
  loadStatus: 'loading',
  loadError: null,
  userEmail: null,
  matchLabel: null,
  shooters: [],
  selectedSquadId: null,
  message: null,
  editor: null,
  dqDialog: null,
  lateShooter: { shooterId: '', discipline: DISCIPLINES[0] },
  unassignedDrafts: {},
  printRequest: null,
  printRequestCount: 0
};

function requestPrint(state: TimekeeperState, cardId: string) {
  state.printRequestCount += 1;
  state.printRequest = { cardId, requestNumber: state.printRequestCount };
}

interface LoadedPayload {
  userEmail: string;
  matchLabel: string | null;
  shooters: ShooterOption[];
}

export const timekeeperSlice = createSlice({
  name: 'timekeeper',
  initialState,
  reducers: {
    loaded(state, action: PayloadAction<LoadedPayload>) {
      state.userEmail = action.payload.userEmail;
      state.matchLabel = action.payload.matchLabel;
      state.shooters = action.payload.shooters;
      state.loadStatus = action.payload.matchLabel === null ? 'no-match' : 'ready';
      state.loadError = null;
    },
    loadFailed(state, action: PayloadAction<string>) {
      state.loadStatus = 'error';
      state.loadError = action.payload;
    },

    squadSelected(state, action: PayloadAction<string>) {
      state.selectedSquadId = action.payload;
    },

    messageShown(state, action: PayloadAction<string | null>) {
      state.message = action.payload;
    },

    /** Clicking the round that is already open closes it. */
    roundEditorToggled(state, action: PayloadAction<{ cardId: string; round: number }>) {
      const { cardId, round } = action.payload;
      const isOpen = state.editor?.cardId === cardId && state.editor.round === round;
      state.editor = isOpen ? null : { cardId, round, timeText: null, reshootText: null, error: null };
    },
    roundEditorClosed(state) {
      state.editor = null;
    },
    editorTimeTextChanged(state, action: PayloadAction<string>) {
      if (state.editor) {
        state.editor.timeText = action.payload;
      }
    },
    editorReshootTextChanged(state, action: PayloadAction<string>) {
      if (state.editor) {
        state.editor.reshootText = action.payload;
      }
    },
    editorErrorShown(state, action: PayloadAction<string | null>) {
      if (state.editor) {
        state.editor.error = action.payload;
      }
    },

    dqDialogOpened(state, action: PayloadAction<string>) {
      state.dqDialog = { cardId: action.payload, reason: '' };
    },
    dqReasonChanged(state, action: PayloadAction<string>) {
      if (state.dqDialog) {
        state.dqDialog.reason = action.payload;
      }
    },
    dqDialogClosed(state) {
      state.dqDialog = null;
    },

    lateShooterChanged(state, action: PayloadAction<Partial<LateShooterDraft>>) {
      Object.assign(state.lateShooter, action.payload);
    },

    unassignedDraftChanged(state, action: PayloadAction<{ resultId: string } & UnassignedDraft>) {
      const { resultId, ...draft } = action.payload;
      state.unassignedDrafts[resultId] = { ...state.unassignedDrafts[resultId], ...draft };
    },

    /** Reprint of a signed card's score sheet. */
    scoreSheetPrintRequested(state, action: PayloadAction<string>) {
      requestPrint(state, action.payload);
    },
    /** The browser has printed (or the print was cancelled). */
    scoreSheetPrinted(state, action: PayloadAction<number>) {
      if (state.printRequest?.requestNumber === action.payload) {
        state.printRequest = null;
      }
    },

    /** Called on page unmount; a fresh visit starts with no leftover UI state. */
    timekeeperReset: () => initialState
  },
  extraReducers: (builder) => {
    builder
      // Saved values become the stored round value, so the inputs fall back to it.
      .addCase(setRoundTime, (state, action) => {
        if (state.editor?.cardId === action.payload.cardId && state.editor.round === action.payload.round) {
          state.editor.timeText = null;
          state.editor.error = null;
        }
      })
      .addCase(setReshootTime, (state, action) => {
        if (state.editor?.cardId === action.payload.cardId && state.editor.round === action.payload.round) {
          state.editor.reshootText = null;
          state.editor.error = null;
        }
      })
      // Signing off prints the shooter's score sheet.
      .addCase(signOff, (state, action) => {
        requestPrint(state, action.payload.cardId);
      })
      .addCase(disqualify, (state) => {
        state.dqDialog = null;
      })
      .addCase(addCard, (state) => {
        state.lateShooter.shooterId = '';
      })
      .addCase(assignResult, (state, action) => {
        delete state.unassignedDrafts[action.payload.resultId];
      })
      .addCase(discardUnassignedResult, (state, action) => {
        delete state.unassignedDrafts[action.payload.resultId];
      });
  }
});

export const {
  loaded,
  loadFailed,
  squadSelected,
  messageShown,
  roundEditorToggled,
  roundEditorClosed,
  editorTimeTextChanged,
  editorReshootTextChanged,
  editorErrorShown,
  dqDialogOpened,
  dqReasonChanged,
  dqDialogClosed,
  lateShooterChanged,
  unassignedDraftChanged,
  scoreSheetPrintRequested,
  scoreSheetPrinted,
  timekeeperReset
} = timekeeperSlice.actions;
