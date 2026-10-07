import { createListenerMiddleware } from '@reduxjs/toolkit';
import { deriveSquadUpNext } from '@/lib/match/derive';
import { SESSION_UP_NEXT_EVENT, buildDeviceTopic } from '@/lib/mqtt/constants';
import type { SessionUpNextMessage } from '@/lib/mqtt/types';
import { addUnassignedResult, turnStarted, turnStopped } from './matchSlice';
import { publishMqtt } from './mqttMiddleware';
import { selectActiveDeviceId, sessionStarted, sessionStopped } from './mqttSlice';
import type { RootState } from './store';
import type { MatchRootState } from './types';

/** Binds timer sessions to the armed turn. A result with nothing armed is kept as unassigned rather than dropped. */
export const matchListeners = createListenerMiddleware();

matchListeners.startListening({
  actionCreator: sessionStarted,
  effect: (_action, api) => {
    const state = api.getState() as MatchRootState;
    const match = state.match.current;
    if (match?.activeTurn) {
      api.dispatch(turnStarted());
    }
  }
});

matchListeners.startListening({
  actionCreator: sessionStopped,
  effect: (action, api) => {
    const state = api.getState() as MatchRootState;
    const match = state.match.current;
    if (!match) {
      return;
    }

    const lastShotTimeMs = action.payload.lastShotTimeMs;
    const timeMs = lastShotTimeMs !== undefined && lastShotTimeMs > 0 ? lastShotTimeMs : null;

    if (match.activeTurn) {
      const activeCardId = match.activeTurn.cardId;
      const squadId = match.cards.find(card => card.id === activeCardId)?.squadId;
      api.dispatch(turnStopped({ lastShotTimeMs: timeMs }));
      if (squadId) {
        publishUpNext(api.getState() as RootState, squadId, action.payload.sessionId);
      }

      return;
    }
    if (timeMs !== null) {
      api.dispatch(addUnassignedResult({ timeMs }));
    }
  }
});

/** Feeds the timer's LED panel; skipped when nobody is left to call up. */
function publishUpNext(state: RootState, squadId: string, sessionId: number) {
  const match = state.match.current;
  const deviceId = selectActiveDeviceId(state);
  if (!match || !deviceId) {
    return;
  }

  const { next, onDeck } = deriveSquadUpNext(match, squadId, null);
  if (!next) {
    return;
  }

  const message: SessionUpNextMessage = { sessionId, next, ...(onDeck && { onDeck }) };
  publishMqtt(buildDeviceTopic(deviceId, SESSION_UP_NEXT_EVENT), message);
}
