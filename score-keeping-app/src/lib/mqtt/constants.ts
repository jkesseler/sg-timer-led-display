import type { DeviceTopic, MqttTopics as IMqttTopics } from './types';

export const MqttTopics: IMqttTopics = {
  // Retained: late-joining subscribers receive the last value immediately.
  PRESENCE: 'timer/+/presence',
  CONNECTION_STATE: 'timer/+/connection/state',
  DEVICE_INFO: 'timer/+/device/info',
  // Not retained: events only.
  SESSION_STARTED: 'timer/+/session/started',
  SESSION_STOPPED: 'timer/+/session/stopped',
  SESSION_SUSPENDED: 'timer/+/session/suspended',
  SESSION_RESUMED: 'timer/+/session/resumed',
  SHOT_DETECTED: 'timer/+/shot/detected',
  COUNTDOWN_COMPLETE: 'timer/+/countdown/complete'
};

/** Published by the timekeeper for the firmware, not subscribed to by any app view. */
export const SESSION_UP_NEXT_EVENT = 'session/up-next';

export function buildDeviceTopic(deviceId: string, event: string): string {
  return `timer/${deviceId}/${event}`;
}

/** 'timer/ABCDEF/connection/state' → { deviceId: 'ABCDEF', event: 'connection/state' }; null for non-timer topics. */
export function parseDeviceTopic(topic: string): DeviceTopic | null {
  const parts = topic.split('/');
  if (parts.length < 3 || parts[0] !== 'timer') {
    return null;
  }

  return { deviceId: parts[1], event: parts.slice(2).join('/') };
}
