import type { RosterInfo } from '@/lib/match/types';
import type { MqttServerConfig, MqttSettings, SessionData, ShotData } from '@/lib/mqtt/types';
import type { SyncTransport } from '@/store/types';
import type { ReactNode } from 'react';

export interface ReduxProviderProps {
  children: ReactNode;
  sync?: SyncTransport;
  mqttConfig?: MqttServerConfig;
}

export interface SettingsProps {
  onSave: (settings: MqttSettings) => void;
  onClose: () => void;
  onConnect: () => void;
  onDisconnect: () => void;
  onResetBroker: () => void;
}

export interface SplitListProps {
  shots: ShotData[];
  /** Only meaningful once a run is complete. */
  highlightExtremes?: boolean;
}

export interface Extremes {
  fastestShotNumber: number | null;
  slowestShotNumber: number | null;
}

export type BeaconTone = 'neutral' | 'searching' | 'ready';

export interface BeaconProps {
  tone: BeaconTone;
}

export interface IdleCardContent {
  tone: BeaconTone;
  eyebrow: string;
  title: string;
  detail?: string;
}

export interface CountdownHeroProps {
  countdownRemainingMs: number;
}

export interface WaitingHeroProps {
  sessionData: SessionData | null;
}

export interface ShotHeroProps {
  shotData: ShotData;
}

export interface ShooterHeaderProps {
  name: string;
}

export interface ShooterQueueProps {
  next: string;
  onDeck: string;
}

export interface SessionEndedHeroProps {
  shotData: ShotData | null;
  sessionData: SessionData | null;
  shots: ShotData[];
}

export interface TimerDisplayProps {
  roster: RosterInfo | null;
}
