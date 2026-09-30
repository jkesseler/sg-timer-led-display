'use client';

import { useState } from 'react';
import { Provider } from 'react-redux';
import { makeStore } from '@/store/store';
import type { ReactNode } from 'react';
import type { MqttServerConfig } from '@/lib/mqtt/config';
import type { SyncTransport } from '@/store/syncMiddleware';

interface ReduxProviderProps {
  children: ReactNode;
  sync?: SyncTransport;
  mqttConfig?: MqttServerConfig;
}

export function ReduxProvider({ children, sync, mqttConfig }: ReduxProviderProps) {
  const [store] = useState(() => makeStore({ sync, mqttConfig }));

  return <Provider store={store}>{children}</Provider>;
}
