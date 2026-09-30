import { createListenerMiddleware } from '@reduxjs/toolkit';
import { addUnassignedResult, turnStarted, turnStopped } from './matchSlice';
import { sessionStarted, sessionStopped } from './mqttSlice';
import type { MatchSliceState } from './matchSlice';

interface ListenerRootState {
  match: MatchSliceState;
}

/** Binds timer sessions to the armed turn. A result with nothing armed is kept as unassigned rather than dropped. */
export const matchListeners = createListenerMiddleware();

matchListeners.startListening({
  actionCreator: sessionStarted,
  effect: (_action, api) => {
    const match = (api.getState() as ListenerRootState).match.current;
    if (match?.activeTurn) {
      api.dispatch(turnStarted());
    }
  }
});

matchListeners.startListening({
  actionCreator: sessionStopped,
  effect: (action, api) => {
    const match = (api.getState() as ListenerRootState).match.current;
    if (!match) {
      return;
    }

    const lastShotTimeMs = action.payload.lastShotTimeMs;
    const timeMs = lastShotTimeMs !== undefined && lastShotTimeMs > 0 ? lastShotTimeMs : null;

    if (match.activeTurn) {
      api.dispatch(turnStopped({ lastShotTimeMs: timeMs }));

      return;
    }
    if (timeMs !== null) {
      api.dispatch(addUnassignedResult({ timeMs }));
    }
  }
});
