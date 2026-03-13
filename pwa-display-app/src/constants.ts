import { DisplayState, ConnectionState } from './types';
import type { DisplayColors as IDisplayColors, DisplayConfig as IDisplayConfig, MqttTopics as IMqttTopics } from './types';

// Display state enumeration matching ESP32 firmware
export { DisplayState, ConnectionState };

// RGB565 color constants matching ESP32 firmware
export const DisplayColors: IDisplayColors = {
  RED: '#FF0000',        // Error, session end
  GREEN: '#00FF00',      // Shot times, ready, success
  BLUE: '#0000FF',       // Unused
  YELLOW: '#FFFF00',     // Shot numbers, session info
  WHITE: '#FFFFFF',      // General text
  LIGHT_BLUE: '#6464FF', // Waiting state (100, 100, 255)
  GRAY: '#808080'        // Disabled/inactive
};

// Display configuration
export const DisplayConfig: IDisplayConfig = {
  PANEL_WIDTH: 64,
  PANEL_HEIGHT: 32,
  PANEL_CHAIN: 2,
  TOTAL_WIDTH: 128,  // 64 * 2
  TOTAL_HEIGHT: 32,
  PIXEL_SIZE: 4,     // CSS pixels per LED pixel (adjustable for screen size)

  // Timing
  STARTUP_MESSAGE_DELAY: 20000, // 20 seconds like firmware
  SCROLL_SPEED_MS: 25,
  SCROLL_PAUSE_MS: 1000,

  // Text
  STARTUP_TEXT: 'J.K. PewPew Timer'
};

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
