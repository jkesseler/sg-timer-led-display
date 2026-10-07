'use client';

import { useState } from 'react';
import { Provider } from 'react-redux';
import { makeStore } from '@/store/store';
import type { ReduxProviderProps } from './types';

export function ReduxProvider({ children, sync, mqttConfig }: ReduxProviderProps) {
  const [store] = useState(() => makeStore({ sync, mqttConfig }));

  return <Provider store={store}>{children}</Provider>;
}
