import { createHash } from 'node:crypto';
import { hostname } from 'node:os';

// MQTT topic scheme: timer/<deviceId>/<event>
// Retained events (presence, connection/state, device/info) let late-joining
// displays receive the current state immediately upon subscription — this
// mirrors MqttManager::buildTopics() in the ESP32 firmware exactly.
export const MqttEvents = {
  PRESENCE: 'presence',
  CONNECTION_STATE: 'connection/state',
  SESSION_STARTED: 'session/started',
  SESSION_STOPPED: 'session/stopped',
  SESSION_SUSPENDED: 'session/suspended',
  SESSION_RESUMED: 'session/resumed',
  SHOT_DETECTED: 'shot/detected',
  COUNTDOWN_COMPLETE: 'countdown/complete',
  DEVICE_INFO: 'device/info'
} as const;

export function buildDeviceTopic(deviceId: string, event: string): string {
  return `timer/${deviceId}/${event}`;
}

// Base-32 alphabet the firmware uses for its 6-char device ID (DeviceId.cpp) —
// excludes O/I/0/1 to avoid visual ambiguity. Reused here so simulated device
// IDs look like a real board's.
const DEVICE_ID_ALPHABET = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';

// Derive a stable 6-char device ID from a seed — the machine hostname by
// default. Unlike a random ID this is identical across runs, so re-running the
// simulator overwrites its own retained topics instead of orphaning a fresh
// timer/<id>/* tree on the broker every time. The firmware gets this for free
// from its flash-backed ID.
export function deriveDeviceId(seed: string): string {
  const digest = createHash('sha1').update(seed).digest();
  let id = '';
  for (let i = 0; i < 6; i++) {
    id += DEVICE_ID_ALPHABET[digest[i] % DEVICE_ID_ALPHABET.length];
  }
  return id;
}

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

// Display states that can be simulated directly
export enum DisplayState {
  CONNECTED = 'connected',
  WAITING_FOR_SHOTS = 'waiting_for_shots',
  SHOWING_SHOT = 'showing_shot',
  SESSION_ENDED = 'session_ended'
}

// Configuration
export interface SimulatorConfig {
  brokerUrl: string;
  /** 6-char device ID the display uses to scope topics: timer/<deviceId>/<event> */
  deviceId: string;
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
  deviceId: deriveDeviceId(hostname()),
  deviceName: 'Simulated SG Timer',
  deviceModel: DeviceModels.SIMULATED,
  startDelay: 3.0,
  shotCount: 10,
  shotInterval: 1500, // ms between shots
  splitTimeVariation: 1500, // random variation in split times
  autoReconnect: true
};
