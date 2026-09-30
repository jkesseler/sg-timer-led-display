'use client';

import { useState } from 'react';
import { Provider } from 'react-redux';
import { makeStore } from '@/store/store';
import type { ReactNode } from 'react';
import type { SyncTransport } from '@/store/syncMiddleware';

interface ReduxProviderProps {
  children: ReactNode;
  sync?: SyncTransport;
}

export function ReduxProvider({ children, sync }: ReduxProviderProps) {
  const [store] = useState(() => makeStore({ sync }));

  return <Provider store={store}>{children}</Provider>;
}
