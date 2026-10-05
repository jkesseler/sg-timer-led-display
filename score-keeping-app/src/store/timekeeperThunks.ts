import { arrayMove } from '@dnd-kit/sortable';
import { TIMEKEEPER_BOOTSTRAP_URL } from '@/lib/match/bootstrap';
import { findCardByKnsa, findNextRoundToShoot, parseTimeInput } from '@/lib/match/derive';
import type { TimekeeperBootstrap } from '@/lib/match/bootstrap';
import type { RoundStatus } from '@/lib/match/types';
import { addCard, armTurn, assignResult, disqualify, flagDnf, flagRs, hydrate, reorderQueue, setReshootTime, setRoundStatus, setRoundTime } from './matchSlice';
import { selectDevice, startConnecting } from './mqttSlice';
import { serverConfigLoaded } from './settingsSlice';
import { getLocalMatchState, resolveInitialMatch } from './syncMiddleware';
import {
  selectAbsentCardIds,
  selectActiveTurn,
  selectDqDialog,
  selectEditor,
  selectLateShooterView,
  selectMatch,
  selectPresentCardIds,
  selectRoundReshootText,
  selectRoundTimeText,
  selectSelectedSquadId,
  selectUnassignedView
} from './timekeeperSelectors';
import { editorErrorShown, editorReshootTextChanged, editorTimeTextChanged, loaded, loadFailed, messageShown, squadSelected } from './timekeeperSlice';
import type { AppThunk } from './store';

const LOGIN_URL = '/timekeeper/login';
const TIME_INPUT_ERROR = 'Enter a time in seconds, e.g. 12.34';

/** Fetches the match and broker config, restores the leading local state, then connects to the broker. */
export const loadTimekeeper = (): AppThunk<Promise<void>> => async (dispatch) => {
  let bootstrap: TimekeeperBootstrap;
  try {
    const response = await fetch(TIMEKEEPER_BOOTSTRAP_URL, { cache: 'no-store' });
    if (response.status === 401) {
      window.location.assign(LOGIN_URL);

      return;
    }
    if (!response.ok) {
      throw new Error(`Loading the match failed (HTTP ${response.status}).`);
    }
    bootstrap = await response.json() as TimekeeperBootstrap;
  } catch (error) {
    dispatch(loadFailed(error instanceof Error ? error.message : String(error)));

    return;
  }

  const { match, mqttConfig, userEmail } = bootstrap;
  dispatch(serverConfigLoaded(mqttConfig));

  const initialState = match
    ? resolveInitialMatch(match.id, getLocalMatchState(), match.serverState, () => match.freshState)
    : null;
  if (initialState) {
    dispatch(hydrate(initialState));
  }
  dispatch(loaded({ userEmail, matchLabel: match?.label ?? null, shooters: match?.shooters ?? [] }));

  dispatch(startConnecting());
  if (initialState?.deviceId) {
    dispatch(selectDevice(initialState.deviceId));
  }
};

/**
 * A card read by the barcode scanner: arms that shooter's next round,
 * preferring their card in the selected squad. The capture is switched off
 * while a shooter is armed; the guard covers a scan that lands in the same tick.
 */
export const handleScannedCard = (knsaNumber: string): AppThunk => (dispatch, getState) => {
  const state = getState();
  const code = knsaNumber.trim();
  const match = selectMatch(state);

  if (!match || !code || selectActiveTurn(state)) {
    return;
  }

  const card = findCardByKnsa(match, code, selectSelectedSquadId(state));
  if (!card) {
    dispatch(messageShown(`No shooter with KNSA number ${code} in this match.`));

    return;
  }

  const round = findNextRoundToShoot(card);
  if (round === null) {
    dispatch(messageShown(`${card.shooterName} has nothing left to shoot.`));

    return;
  }

  dispatch(messageShown(null));
  dispatch(squadSelected(card.squadId));
  dispatch(armTurn({ cardId: card.id, round }));
};

export const saveEditorTime = (): AppThunk => (dispatch, getState) => {
  const editor = selectEditor(getState());
  if (!editor) {
    return;
  }

  const timeMs = parseTimeInput(selectRoundTimeText(getState(), editor.cardId, editor.round));
  if (timeMs === null) {
    dispatch(editorErrorShown(TIME_INPUT_ERROR));

    return;
  }
  dispatch(setRoundTime({ cardId: editor.cardId, round: editor.round, timeMs }));
};

export const clearEditorTime = (): AppThunk => (dispatch, getState) => {
  const editor = selectEditor(getState());
  if (editor) {
    dispatch(setRoundTime({ cardId: editor.cardId, round: editor.round, timeMs: null }));
    dispatch(editorTimeTextChanged(''));
  }
};

export const saveEditorReshoot = (): AppThunk => (dispatch, getState) => {
  const editor = selectEditor(getState());
  if (!editor) {
    return;
  }

  const reshootTimeMs = parseTimeInput(selectRoundReshootText(getState(), editor.cardId, editor.round));
  if (reshootTimeMs === null) {
    dispatch(editorErrorShown(TIME_INPUT_ERROR));

    return;
  }
  dispatch(setReshootTime({ cardId: editor.cardId, round: editor.round, reshootTimeMs }));
};

export const clearEditorReshoot = (): AppThunk => (dispatch, getState) => {
  const editor = selectEditor(getState());
  if (editor) {
    dispatch(setReshootTime({ cardId: editor.cardId, round: editor.round, reshootTimeMs: null }));
    dispatch(editorReshootTextChanged(''));
  }
};

/** RS and DNF have their own actions: they also end an armed turn on that round. */
export const setEditorRoundStatus = (status: RoundStatus): AppThunk => (dispatch, getState) => {
  const editor = selectEditor(getState());
  if (!editor) {
    return;
  }

  const ref = { cardId: editor.cardId, round: editor.round };
  if (status === 'rs') {
    dispatch(flagRs(ref));

    return;
  }
  if (status === 'dnf') {
    dispatch(flagDnf(ref));

    return;
  }
  dispatch(setRoundStatus({ ...ref, status }));
};

export const armEditorRound = (): AppThunk => (dispatch, getState) => {
  const editor = selectEditor(getState());
  if (editor) {
    dispatch(armTurn({ cardId: editor.cardId, round: editor.round }));
  }
};

export const confirmDisqualification = (): AppThunk => (dispatch, getState) => {
  const dqDialog = selectDqDialog(getState());
  if (dqDialog) {
    dispatch(disqualify({ cardId: dqDialog.cardId, reason: dqDialog.reason.trim() }));
  }
};

export const addLateShooter = (): AppThunk => (dispatch, getState) => {
  const { squadId, shooters, shooterId, discipline } = selectLateShooterView(getState());
  const shooter = shooters.find(candidate => candidate.id === shooterId);
  if (!squadId || !shooter) {
    return;
  }

  dispatch(addCard({ squadId, shooterId: shooter.id, shooterName: shooter.name, knsaNumber: shooter.knsaNumber, discipline }));
};

export const assignUnassignedResult = (resultId: string): AppThunk => (dispatch, getState) => {
  const result = selectUnassignedView(getState()).results.find(candidate => candidate.resultId === resultId);
  if (result?.cardId) {
    dispatch(assignResult({ resultId, cardId: result.cardId, round: result.round }));
  }
};

/** Drag-and-drop within the present cards; absent cards keep their place at the back. */
export const moveQueueCard = (activeCardId: string, overCardId: string): AppThunk => (dispatch, getState) => {
  const presentIds = selectPresentCardIds(getState());
  const oldIndex = presentIds.indexOf(activeCardId);
  const newIndex = presentIds.indexOf(overCardId);
  if (oldIndex === -1 || newIndex === -1 || oldIndex === newIndex) {
    return;
  }

  dispatch(reorderQueue({ cardIds: [...arrayMove(presentIds, oldIndex, newIndex), ...selectAbsentCardIds(getState())] }));
};
