import { configureStore, combineReducers } from '@reduxjs/toolkit';
import { useDispatch, useSelector } from 'react-redux';
import type { TypedUseSelectorHook } from 'react-redux';
import { settingsSlice } from './settingsSlice';
import { mqttSlice } from './mqttSlice';
import { mqttMiddleware } from './mqttMiddleware';
import { beepMiddleware } from './beepMiddleware';

// ---------------------------------------------------------------------------
// Root reducer (exported so RootState can be derived without circular ref)
// ---------------------------------------------------------------------------

const rootReducer = combineReducers({
  [settingsSlice.name]: settingsSlice.reducer,
  [mqttSlice.name]: mqttSlice.reducer,
});

/** Inferred from the rootReducer — no circular dependency on `store`. */
export type RootState = ReturnType<typeof rootReducer>;

// ---------------------------------------------------------------------------
// Store
// ---------------------------------------------------------------------------

export const store = configureStore({
  reducer: rootReducer,
  middleware: (getDefaultMiddleware) =>
    getDefaultMiddleware({ thunk: true })
      .concat(mqttMiddleware as ReturnType<typeof getDefaultMiddleware>[number])
      .concat(beepMiddleware.middleware),
  devTools: true,
});

// ---------------------------------------------------------------------------
// Typed hooks
// ---------------------------------------------------------------------------

export type AppDispatch = typeof store.dispatch;

export const useAppDispatch = () => useDispatch<AppDispatch>();
export const useAppSelector: TypedUseSelectorHook<RootState> = useSelector;
