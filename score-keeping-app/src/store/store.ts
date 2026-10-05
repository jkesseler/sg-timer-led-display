import { configureStore, combineReducers } from '@reduxjs/toolkit';
import { useDispatch, useSelector } from 'react-redux';
import type { MqttServerConfig } from '@/lib/mqtt/config';
import { beepMiddleware } from './beepMiddleware';
import { matchListeners } from './matchListeners';
import { matchSlice } from './matchSlice';
import { mqttMiddleware } from './mqttMiddleware';
import { mqttSlice } from './mqttSlice';
import { buildInitialSettings, settingsSlice } from './settingsSlice';
import { createSyncMiddleware } from './syncMiddleware';
import { timekeeperSlice } from './timekeeperSlice';
import type { SyncTransport } from './syncMiddleware';
import type { Action, ThunkAction } from '@reduxjs/toolkit';
import type { TypedUseSelectorHook } from 'react-redux';

const rootReducer = combineReducers({
  [settingsSlice.name]: settingsSlice.reducer,
  [mqttSlice.name]: mqttSlice.reducer,
  [matchSlice.name]: matchSlice.reducer,
  [timekeeperSlice.name]: timekeeperSlice.reducer
});

export type RootState = ReturnType<typeof rootReducer>;

interface StoreOptions {
  /** Only the timekeeper persists match changes; /display never writes. */
  sync?: SyncTransport;
  mqttConfig?: MqttServerConfig;
}

/**
 * One store per mounted provider — a module-level store would be shared
 * between requests while Next renders client components on the server.
 */
export function makeStore({ sync, mqttConfig }: StoreOptions = {}) {
  return configureStore({
    reducer: rootReducer,
    preloadedState: { settings: buildInitialSettings(mqttConfig) },
    middleware: (getDefaultMiddleware) => {
      const middleware = getDefaultMiddleware({ thunk: true })
        .concat(mqttMiddleware as ReturnType<typeof getDefaultMiddleware>[number])
        .concat(beepMiddleware.middleware);

      if (!sync) {
        return middleware;
      }

      return middleware
        .concat(matchListeners.middleware)
        .concat(createSyncMiddleware({ transport: sync }) as ReturnType<typeof getDefaultMiddleware>[number]);
    },
    devTools: true
  });
}

export type AppStore = ReturnType<typeof makeStore>;
export type AppDispatch = AppStore['dispatch'];
export type AppThunk<ReturnType = void> = ThunkAction<ReturnType, RootState, unknown, Action>;

export const useAppDispatch = () => useDispatch<AppDispatch>();
export const useAppSelector: TypedUseSelectorHook<RootState> = useSelector;
