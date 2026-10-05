import type { MqttServerConfig } from '@/lib/mqtt/config';
import type { MatchState } from './types';

export interface ShooterOption {
  id: string;
  name: string;
  knsaNumber: string | null;
}

export interface BootstrapMatch {
  id: string;
  label: string;
  freshState: MatchState;
  serverState: MatchState | null;
  shooters: ShooterOption[];
}

/** Response of GET /timekeeper/bootstrap. */
export interface TimekeeperBootstrap {
  userEmail: string;
  mqttConfig: MqttServerConfig;
  match: BootstrapMatch | null;
}

export const TIMEKEEPER_BOOTSTRAP_URL = '/timekeeper/bootstrap';
