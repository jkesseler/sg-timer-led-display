import mqtt from 'mqtt';
import type { IClientPublishOptions } from 'mqtt';
import type { TimerEvent } from './protocols/timerEvent.js';

// Topic scheme and payloads mirror MqttManager in the ESP32 firmware; field
// names are part of that contract and must not be renamed.
export const MQTT_EVENTS = {
  PRESENCE: 'presence',
  CONNECTION_STATE: 'connection/state',
  DEVICE_INFO: 'device/info',
  SESSION_STARTED: 'session/started',
  SESSION_STOPPED: 'session/stopped',
  SESSION_SUSPENDED: 'session/suspended',
  SESSION_RESUMED: 'session/resumed',
  SHOT_DETECTED: 'shot/detected',
  COUNTDOWN_COMPLETE: 'countdown/complete',
} as const;

export type ConnectionState = 'DISCONNECTED' | 'SCANNING' | 'CONNECTING' | 'CONNECTED' | 'ERROR';

export interface TimerIdentity {
  deviceName: string;
  deviceModel: string;
}

/** The part of an MQTT client the publisher needs; `MqttClient` satisfies it. */
export interface PublishClient {
  publish(topic: string, message: string, options: IClientPublishOptions, callback: (error?: Error) => void): unknown;
}

type QualityOfService = 0 | 1;

export function buildDeviceTopic(deviceId: string, event: string) {
  return `timer/${deviceId}/${event}`;
}

export class TimerPublisher {
  private readonly client: PublishClient;
  private readonly deviceId: string;
  // Mirrors TimerApplication::lastShotTime: reset on session start, reported on session stop.
  private lastShotTimeMs = 0;

  constructor(client: PublishClient, deviceId: string) {
    this.client = client;
    this.deviceId = deviceId;
  }

  /** Raw `online`/`offline` string, not JSON: displays compare the payload bytes directly. */
  publishPresence(isOnline: boolean, qos: QualityOfService = 0) {
    return this.publishRaw(MQTT_EVENTS.PRESENCE, isOnline ? 'online' : 'offline', { qos, retain: true });
  }

  publishConnectionState(state: ConnectionState, timer?: TimerIdentity, qos: QualityOfService = 0) {
    const message = {
      state,
      deviceName: timer?.deviceName,
      deviceModel: timer?.deviceModel,
      timestamp: Date.now(),
    };

    return this.publishJson(MQTT_EVENTS.CONNECTION_STATE, message, { qos, retain: true });
  }

  publishDeviceInfo(timer: TimerIdentity) {
    const message = {
      deviceName: timer.deviceName,
      deviceModel: timer.deviceModel,
      deviceId: this.deviceId,
      timestamp: Date.now(),
    };

    return this.publishJson(MQTT_EVENTS.DEVICE_INFO, message, { qos: 0, retain: true });
  }

  publishTimerEvent(event: TimerEvent) {
    const eventOptions: IClientPublishOptions = { qos: 0, retain: false };

    switch (event.type) {
      case 'sessionStarted':
        this.lastShotTimeMs = 0;

        return this.publishJson(MQTT_EVENTS.SESSION_STARTED, {
          sessionId: event.sessionId,
          startDelaySeconds: event.startDelaySeconds,
          timestamp: Date.now(),
        }, eventOptions);

      case 'countdownComplete':
        return this.publishJson(MQTT_EVENTS.COUNTDOWN_COMPLETE, {
          sessionId: event.sessionId,
          timestamp: Date.now(),
        }, eventOptions);

      case 'shotDetected':
        this.lastShotTimeMs = event.shot.absoluteTimeMs;

        return this.publishJson(MQTT_EVENTS.SHOT_DETECTED, {
          ...event.shot,
          timestamp: Date.now(),
        }, eventOptions);

      case 'sessionStopped':
        return this.publishJson(MQTT_EVENTS.SESSION_STOPPED, {
          sessionId: event.sessionId,
          totalShots: event.totalShots,
          // Omitted rather than 0 when no shot was fired, as the firmware does.
          lastShotTimeMs: this.lastShotTimeMs > 0 ? this.lastShotTimeMs : undefined,
          timestamp: Date.now(),
        }, eventOptions);

      case 'sessionSuspended':
        return this.publishJson(MQTT_EVENTS.SESSION_SUSPENDED, {
          sessionId: event.sessionId,
          timestamp: Date.now(),
        }, eventOptions);

      case 'sessionResumed':
        return this.publishJson(MQTT_EVENTS.SESSION_RESUMED, {
          sessionId: event.sessionId,
          timestamp: Date.now(),
        }, eventOptions);

      default: {
        const unhandledEvent: never = event;

        return unhandledEvent;
      }
    }
  }

  private publishJson(event: string, message: object, options: IClientPublishOptions) {
    return this.publishRaw(event, JSON.stringify(message), options);
  }

  private publishRaw(event: string, payload: string, options: IClientPublishOptions) {
    const topic = buildDeviceTopic(this.deviceId, event);

    return new Promise<void>((resolve) => {
      this.client.publish(topic, payload, options, (error) => {
        if (error) {
          console.error(`[publisher] failed to publish to ${topic}: ${error.message}`);
        }

        resolve();
      });
    });
  }
}

export interface MqttConfig {
  brokerUrl: string;
  username?: string;
  password?: string;
  deviceId: string;
}

/**
 * Connect to the broker with the firmware's last will: the broker publishes a
 * retained `offline` presence if the bridge drops without a clean shutdown.
 */
export function connectPublisher({ brokerUrl, username, password, deviceId }: MqttConfig) {
  const client = mqtt.connect(brokerUrl, {
    clientId: `pewpew-${deviceId}`,
    username,
    password,
    clean: true,
    reconnectPeriod: 5000,
    will: {
      topic: buildDeviceTopic(deviceId, MQTT_EVENTS.PRESENCE),
      payload: Buffer.from('offline'),
      qos: 0,
      retain: true,
    },
  });
  const publisher = new TimerPublisher(client, deviceId);

  // Re-announce after every (re)connect, since the will may have replaced the retained value.
  client.on('connect', () => {
    publisher.publishPresence(true);
  });

  client.on('error', (error) => {
    console.error(`[publisher] MQTT error: ${error.message}`);
  });

  /**
   * Publish DISCONNECTED and `offline` before closing. A clean disconnect does
   * not fire the will, so skipping this leaves the retained presence `online`.
   * QoS 1 so the broker has both before the socket closes.
   */
  async function close(timer?: TimerIdentity) {
    if (client.connected) {
      await publisher.publishConnectionState('DISCONNECTED', timer, 1);
      await publisher.publishPresence(false, 1);
    }

    await client.endAsync();
  }

  return { client, publisher, close };
}
