// MQTT Topics
export const MqttTopics = {
  CONNECTION_STATE: 'timer/connection/state',
  SESSION_STARTED: 'timer/session/started',
  SESSION_STOPPED: 'timer/session/stopped',
  SESSION_SUSPENDED: 'timer/session/suspended',
  SESSION_RESUMED: 'timer/session/resumed',
  SHOT_DETECTED: 'timer/shot/detected',
  COUNTDOWN_COMPLETE: 'timer/countdown/complete',
  DEVICE_INFO: 'timer/device/info'
} as const;

// Connection states
export enum ConnectionState {
  DISCONNECTED = 'DISCONNECTED',
  SCANNING = 'SCANNING',
  CONNECTING = 'CONNECTING',
  CONNECTED = 'CONNECTED',
  ERROR = 'ERROR'
}

// Device models
export const DeviceModels = {
  SG_TIMER_SPORT: 'SG Timer Sport',
  SG_TIMER_GO: 'SG Timer GO',
  SPECIAL_PIE_M1A2: 'Special Pie M1A2+',
  SIMULATED: 'Simulated Timer'
} as const;

// Message types
export interface ConnectionStateMessage {
  state: ConnectionState;
  deviceName?: string;
  timestamp: number;
}

export interface SessionStartedMessage {
  sessionId: number;
  startDelaySeconds: number;
  timestamp: number;
}

export interface ShotDetectedMessage {
  sessionId: number;
  shotNumber: number;
  absoluteTimeMs: number;
  splitTimeMs: number;
  deviceModel: string;
  isFirstShot: boolean;
  timestamp: number;
}

export interface SessionStoppedMessage {
  sessionId: number;
  totalShots: number;
  timestamp: number;
}

export interface CountdownCompleteMessage {
  sessionId: number;
  timestamp: number;
}

export interface DeviceInfoMessage {
  deviceModel: string;
  deviceName: string;
  timestamp: number;
}

// Configuration
export interface SimulatorConfig {
  brokerUrl: string;
  deviceName: string;
  deviceModel: string;
  startDelay: number;
  shotCount: number;
  shotInterval: number;
  splitTimeVariation: number;
  autoReconnect: boolean;
}

export const DEFAULT_CONFIG: SimulatorConfig = {
  brokerUrl: 'ws://localhost:9001',
  deviceName: 'Simulated SG Timer',
  deviceModel: DeviceModels.SIMULATED,
  startDelay: 3.0,
  shotCount: 10,
  shotInterval: 1500, // ms between shots
  splitTimeVariation: 1500, // random variation in split times
  autoReconnect: true
};
