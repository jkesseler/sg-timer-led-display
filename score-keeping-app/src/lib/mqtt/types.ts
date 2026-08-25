/**
 * Type definitions for the timer/<deviceId>/<event> MQTT contract.
 * Matches data structures from ESP32 firmware and MQTT bridge.
 * Ported from pwa-display-app/src/types.ts — this is the shared contract
 * consumed by both the server-side subscriber and the /display route.
 */

// Display States
export enum DisplayState {
  STARTUP = 'STARTUP',
  DISCONNECTED = 'DISCONNECTED',
  SCANNING = 'SCANNING',
  CONNECTING = 'CONNECTING',
  CONNECTED = 'CONNECTED',
  COUNTDOWN = 'COUNTDOWN',
  WAITING_FOR_SHOTS = 'WAITING_FOR_SHOTS',
  SHOWING_SHOT = 'SHOWING_SHOT',
  SESSION_ENDED = 'SESSION_ENDED'
}

// Connection States
export enum ConnectionState {
  DISCONNECTED = 'DISCONNECTED',
  SCANNING = 'SCANNING',
  CONNECTING = 'CONNECTING',
  CONNECTED = 'CONNECTED',
  ERROR = 'ERROR'
}

// Shot data from timer device
export interface ShotData {
  sessionId: number;
  shotNumber: number;
  absoluteTimeMs: number;
  splitTimeMs: number;
  deviceModel: string;
  isFirstShot: boolean;
  timestamp: number;
}

// Session data
export interface SessionData {
  sessionId: number;
  isActive: boolean;
  totalShots: number;
  startTimestamp: number;
  startDelaySeconds: number;
  countdownStartTime: number;
}

// MQTT Settings
export interface MqttSettings {
  broker: string;
  username: string;
  password: string;
  clientId: string;
  brightness?: number;
}

// MQTT Message Types
export interface ConnectionStateMessage {
  state: ConnectionState;
  deviceName?: string;
  deviceModel?: string;
  timestamp: number;
}

export interface SessionStartedMessage {
  sessionId: number;
  timestamp: number;
  startDelaySeconds?: number;
}

export interface SessionStoppedMessage {
  sessionId: number;
  totalShots?: number;
  lastShotTimeMs?: number;  // Final shot time in milliseconds
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

export interface CountdownCompleteMessage {
  sessionId: number;
  timestamp: number;
}

export interface DeviceInfoMessage {
  deviceName?: string;
  deviceModel?: string;
  firmwareVersion?: string;
  deviceId?: string;   // Embedded by firmware in publishDeviceInfo
  timestamp: number;
}

// MQTT Message Handler Type
// deviceId is parsed from the MQTT topic (e.g. timer/ABCDEF/connection/state → 'ABCDEF')
export type MqttMessageHandler<T = any> = (message: T, deviceId: string) => void;

// A device discovered via its presence topic
export interface KnownDevice {
  deviceId: string;          // 6-char alphanumeric from DeviceId.h
  deviceName?: string;       // BLE device name (from device/info)
  deviceModel?: string;      // BLE device model (from device/info)
  firmwareVersion?: string;
  presence: 'online' | 'offline';
  lastSeenMs: number;        // Date.now() when last message was received
}

// Presence payload published to timer/<deviceId>/presence
export interface DevicePresenceMessage {
  presence: 'online' | 'offline';
}

// MQTT Topics
// Values are MQTT subscription patterns using '+' (single-level wildcard)
// so a subscriber receives events from ANY device in one call.
export interface MqttTopics {
  /** timer/+/presence – retained, used for device discovery */
  PRESENCE: string;
  /** timer/+/connection/state – retained */
  CONNECTION_STATE: string;
  SESSION_STARTED: string;
  SESSION_STOPPED: string;
  SESSION_SUSPENDED: string;
  SESSION_RESUMED: string;
  SHOT_DETECTED: string;
  COUNTDOWN_COMPLETE: string;
  /** timer/+/device/info – retained */
  DEVICE_INFO: string;
}
