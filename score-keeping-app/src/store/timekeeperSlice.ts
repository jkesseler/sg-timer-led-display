import { createSlice } from '@reduxjs/toolkit';
import { DISCIPLINES } from '@/lib/domain/disciplines';
import { addCard, assignResult, discardUnassignedResult, disqualify, setReshootTime, setRoundTime, signOff } from './matchSlice';
import type { LateShooterDraft, RoundRef, TimekeeperLoadedPayload, TimekeeperState, UnassignedDraftChange } from './types';
import type { PayloadAction } from '@reduxjs/toolkit';

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

export const timekeeperSlice = createSlice({
  name: 'timekeeper',
  initialState,
  reducers: {
    loaded(state, action: PayloadAction<TimekeeperLoadedPayload>) {
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

    roundEditorToggled(state, action: PayloadAction<RoundRef>) {
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

    unassignedDraftChanged(state, action: PayloadAction<UnassignedDraftChange>) {
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
