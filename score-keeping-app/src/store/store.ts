import { configureStore, combineReducers } from '@reduxjs/toolkit';
import { useDispatch, useSelector } from 'react-redux';
import { beepMiddleware } from './beepMiddleware';
import { matchListeners } from './matchListeners';
import { matchSlice } from './matchSlice';
import { mqttMiddleware } from './mqttMiddleware';
import { mqttSlice } from './mqttSlice';
import { settingsSlice } from './settingsSlice';
import { createSyncMiddleware } from './syncMiddleware';
import type { SyncTransport } from './syncMiddleware';
import type { TypedUseSelectorHook } from 'react-redux';

const rootReducer = combineReducers({
  [settingsSlice.name]: settingsSlice.reducer,
  [mqttSlice.name]: mqttSlice.reducer,
  [matchSlice.name]: matchSlice.reducer
});

export type RootState = ReturnType<typeof rootReducer>;

interface StoreOptions {
  /** Only the timekeeper persists match changes; /display never writes. */
  sync?: SyncTransport;
}

/**
 * One store per mounted provider — a module-level store would be shared
 * between requests while Next renders client components on the server.
 */
export function makeStore({ sync }: StoreOptions = {}) {
  return configureStore({
    reducer: rootReducer,
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

export const useAppDispatch = () => useDispatch<AppDispatch>();
export const useAppSelector: TypedUseSelectorHook<RootState> = useSelector;
