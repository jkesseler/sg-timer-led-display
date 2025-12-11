/**
 * Type definitions for SG Timer PWA Display
 * Matches data structures from ESP32 firmware and MQTT bridge
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
  timestamp: number;
}

// MQTT Message Handler Type
export type MqttMessageHandler<T = any> = (message: T) => void;

// Display Colors
export interface DisplayColors {
  RED: string;
  GREEN: string;
  BLUE: string;
  YELLOW: string;
  WHITE: string;
  LIGHT_BLUE: string;
  GRAY: string;
}

// Display Configuration
export interface DisplayConfig {
  PANEL_WIDTH: number;
  PANEL_HEIGHT: number;
  PANEL_CHAIN: number;
  TOTAL_WIDTH: number;
  TOTAL_HEIGHT: number;
  PIXEL_SIZE: number;
  STARTUP_MESSAGE_DELAY: number;
  SCROLL_SPEED_MS: number;
  SCROLL_PAUSE_MS: number;
  STARTUP_TEXT: string;
}

// MQTT Topics
export interface MqttTopics {
  CONNECTION_STATE: string;
  SESSION_STARTED: string;
  SESSION_STOPPED: string;
  SESSION_SUSPENDED: string;
  SESSION_RESUMED: string;
  SHOT_DETECTED: string;
  COUNTDOWN_COMPLETE: string;
  DEVICE_INFO: string;
}

// Component Props
export interface LEDMatrixProps {
  displayState: DisplayState;
  shotData: ShotData | null;
  sessionData: SessionData | null;
  connectionState: string;
  deviceName: string | null;
  brightness?: number;
}

export interface SettingsProps {
  settings: MqttSettings;
  onSave: (settings: MqttSettings) => void;
  onClose: () => void;
  isConnected: boolean;
}

// Hook return types
export interface UseMqttReturn {
  isConnected: boolean;
  connectionError: string | null;
  settings: MqttSettings;
  connect: (customSettings?: MqttSettings | null) => void;
  disconnect: () => void;
  onMessage: <T = any>(topic: string, handler: MqttMessageHandler<T>) => void;
  updateSettings: (settings: MqttSettings) => void;
}

// Canvas rendering context
export type CanvasRenderingContext = CanvasRenderingContext2D;
