import { DisplayState, ConnectionState } from './types';
import type { MqttTopics as IMqttTopics } from './types';

// Display state enumeration matching ESP32 firmware
export { DisplayState, ConnectionState };

// Wordmark shown on the STARTUP screen
export const STARTUP_TEXT = 'J.K. PewPew Timer';

// How long the STARTUP screen holds before falling back to DISCONNECTED,
// in case no connection/state message arrives sooner.
export const STARTUP_DISPLAY_MS = 3000;

// MQTT topics
// '+' is the single-level MQTT wildcard; it matches exactly one topic segment.
// Subscribing to 'timer/+/shot/detected' receives shots from ALL devices.
// The deviceId is extracted from the matching segment in useMqtt.ts.
export const MqttTopics: IMqttTopics = {
  // Retained  – late-joining displays receive the last value immediately
  PRESENCE:         'timer/+/presence',
  CONNECTION_STATE: 'timer/+/connection/state',
  DEVICE_INFO:      'timer/+/device/info',
  // Ephemeral – events only; not retained
  SESSION_STARTED:   'timer/+/session/started',
  SESSION_STOPPED:   'timer/+/session/stopped',
  SESSION_SUSPENDED: 'timer/+/session/suspended',
  SESSION_RESUMED:   'timer/+/session/resumed',
  SHOT_DETECTED:     'timer/+/shot/detected',
  COUNTDOWN_COMPLETE:'timer/+/countdown/complete',
};

/**
 * Build the concrete topic for a known device.
 * Used anywhere you need to subscribe or log a specific device's topic.
 * e.g. buildDeviceTopic('ABCDEF', 'shot/detected') → 'timer/ABCDEF/shot/detected'
 */
export function buildDeviceTopic(deviceId: string, event: string): string {
  return `timer/${deviceId}/${event}`;
}

/**
 * Extract the deviceId and event from a received MQTT topic string.
 * e.g. 'timer/ABCDEF/connection/state' → { deviceId: 'ABCDEF', event: 'connection/state' }
 * Returns null for topics that don't match the timer/<id>/... format.
 */
export function parseDeviceTopic(topic: string): { deviceId: string; event: string } | null {
  const parts = topic.split('/');
  if (parts.length < 3 || parts[0] !== 'timer') return null;
  return { deviceId: parts[1], event: parts.slice(2).join('/') };
}

// Default MQTT settings
export const DefaultMqttSettings = {
  broker: 'ws://127.0.0.1:9001', // MQTT WebSocket port
  username: 'pewpew',
  password: 'timer',
  clientId: `pwa-display-${Math.random().toString(16).substring(2, 8)}`
};
