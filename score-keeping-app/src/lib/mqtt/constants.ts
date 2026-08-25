import type { MqttTopics as IMqttTopics } from './types';

// MQTT topics
// '+' is the single-level MQTT wildcard; it matches exactly one topic segment.
// Subscribing to 'timer/+/shot/detected' receives shots from ALL devices.
export const MqttTopics: IMqttTopics = {
  // Retained  – late-joining subscribers receive the last value immediately
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
