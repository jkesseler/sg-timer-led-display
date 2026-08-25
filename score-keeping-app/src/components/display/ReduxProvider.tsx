'use client';

import type { ReactNode } from 'react';
import { Provider } from 'react-redux';
import { store } from '@/store/store';

export function ReduxProvider({ children }: { children: ReactNode }) {
  return <Provider store={store}>{children}</Provider>;
}
