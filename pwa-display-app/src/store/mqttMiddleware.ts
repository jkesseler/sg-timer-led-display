import type { Middleware } from '@reduxjs/toolkit';
import mqtt, { MqttClient } from 'mqtt';
import { MqttTopics, parseDeviceTopic } from '../constants';
import { parseMqttMessage } from '../utils';
import { mqttSlice } from './mqttSlice';
import { selectMqttConnectionSettings } from './settingsSlice';
import type {
  ConnectionStateMessage,
  SessionStartedMessage,
  SessionStoppedMessage,
  ShotDetectedMessage,
  CountdownCompleteMessage,
  DeviceInfoMessage,
} from '../types';
import type { RootState } from './store';

// ---------------------------------------------------------------------------
// Internal state held outside Redux (the live WebSocket client)
// ---------------------------------------------------------------------------

let mqttClient: MqttClient | null = null;
let countdownTimerId: ReturnType<typeof setInterval> | null = null;

/**
 * Match a topic pattern (with '+' single-level wildcard) against a concrete topic.
 * '#' multi-level wildcard is supported only at the end.
 */
function matchPattern(pattern: string, topic: string): boolean {
  const pp = pattern.split('/');
  const tp = topic.split('/');
  if (pp[pp.length - 1] === '#') {
    if (tp.length < pp.length - 1) return false;
    return pp.slice(0, -1).every((seg, i) => seg === '+' || seg === tp[i]);
  }
  if (pp.length !== tp.length) return false;
  return pp.every((seg, i) => seg === '+' || seg === tp[i]);
}

// ---------------------------------------------------------------------------
// Countdown timer helpers
// ---------------------------------------------------------------------------

function startCountdownTimer(
  dispatch: (action: unknown) => void,
  getState: () => RootState,
) {
  stopCountdownTimer();
  countdownTimerId = setInterval(() => {
    const { sessionData, displayState } = getState().mqtt;
    if (!sessionData || displayState !== 'COUNTDOWN') {
      stopCountdownTimer();
      return;
    }
    const elapsed = Date.now() - sessionData.countdownStartTime;
    const remaining = Math.max(0, sessionData.startDelaySeconds * 1000 - elapsed);
    dispatch(mqttSlice.actions.countdownTick(remaining));
  }, 50); // ~20 fps for smooth countdown
}

function stopCountdownTimer() {
  if (countdownTimerId !== null) {
    clearInterval(countdownTimerId);
    countdownTimerId = null;
  }
}

// ---------------------------------------------------------------------------
// Resolve the active device ID from current state
// ---------------------------------------------------------------------------

function resolveActiveDeviceId(state: RootState): string | null {
  const { knownDevices, selectedDeviceId } = state.mqtt;
  if (selectedDeviceId) return selectedDeviceId;
  return knownDevices.find((d: { presence: string }) => d.presence === 'online')?.deviceId ?? null;
}

// ---------------------------------------------------------------------------
// Middleware
// ---------------------------------------------------------------------------

// eslint-disable-next-line @typescript-eslint/no-explicit-any
export const mqttMiddleware: Middleware = (store) => (next) => (action: any) => {
  const dispatch = store.dispatch;
  const getState = store.getState as () => RootState;

  // ---- startConnecting: open a new MQTT connection ----
  if (mqttSlice.actions.startConnecting.match(action)) {
    // Tear down any existing client first
    if (mqttClient) {
      mqttClient.end(true);
      mqttClient = null;
    }

    const settings = selectMqttConnectionSettings(getState());
    const freshClientId = `pwa-display-${Math.random().toString(16).substring(2, 10)}`;

    const options: mqtt.IClientOptions = {
      clientId: freshClientId,
      clean: true,
      reconnectPeriod: 5000,
      connectTimeout: 30000,
    };

    if (settings.username) {
      options.username = settings.username;
      options.password = settings.password;
    }

    console.log('MQTT connecting to:', settings.broker);
    mqttClient = mqtt.connect(settings.broker, options);

    mqttClient.on('connect', () => {
      console.log('MQTT connected');
      dispatch(mqttSlice.actions.connected());

      mqttClient?.subscribe('timer/#', { qos: 1 }, (err) => {
        if (err) console.error('Failed to subscribe to timer/#:', err);
        else console.log('Subscribed to timer/# (all devices)');
      });
    });

    mqttClient.on('error', (err: Error) => {
      console.error('MQTT error:', err);
      dispatch(mqttSlice.actions.connectionError(err.message));
    });

    mqttClient.on('close', () => {
      console.log('MQTT connection closed');
      dispatch(mqttSlice.actions.disconnected());
      stopCountdownTimer();
    });

    mqttClient.on('offline', () => {
      console.log('MQTT client offline');
      dispatch(mqttSlice.actions.disconnected());
    });

    mqttClient.on('reconnect', () => {
      console.log('MQTT reconnecting...');
    });

    // ---- Central message router ----
    mqttClient.on('message', (topic: string, payload: Buffer) => {
      const parsed = parseDeviceTopic(topic);
      if (!parsed) return;

      const { deviceId, event } = parsed;

      // --- Presence ---
      if (event === 'presence') {
        const presence = payload.toString() as 'online' | 'offline';
        dispatch(mqttSlice.actions.devicePresenceUpdated({ deviceId, presence }));
        return;
      }

      // --- Device info (update registry even if not active device) ---
      if (matchPattern(MqttTopics.DEVICE_INFO, topic)) {
        const msg = parseMqttMessage<DeviceInfoMessage>(topic, payload);
        if (msg) {
          dispatch(mqttSlice.actions.deviceInfoUpdated({ deviceId, info: msg }));
        }
      }

      // --- Filter by active device ---
      const activeId = resolveActiveDeviceId(getState());
      if (activeId && deviceId !== activeId) return;

      // --- Connection state ---
      if (matchPattern(MqttTopics.CONNECTION_STATE, topic)) {
        const msg = parseMqttMessage<ConnectionStateMessage>(topic, payload);
        if (msg) dispatch(mqttSlice.actions.timerConnectionStateChanged(msg));
      }

      // --- Session started ---
      if (matchPattern(MqttTopics.SESSION_STARTED, topic)) {
        const msg = parseMqttMessage<SessionStartedMessage>(topic, payload);
        if (msg) {
          dispatch(mqttSlice.actions.sessionStarted(msg));
          // Start a client-side countdown ticker if there's a delay
          if (msg.startDelaySeconds && msg.startDelaySeconds > 0) {
            startCountdownTimer(dispatch as (action: unknown) => void, getState);
          }
        }
      }

      // --- Countdown complete ---
      if (matchPattern(MqttTopics.COUNTDOWN_COMPLETE, topic)) {
        const msg = parseMqttMessage<CountdownCompleteMessage>(topic, payload);
        if (msg) {
          stopCountdownTimer();
          dispatch(mqttSlice.actions.countdownComplete(msg));
        }
      }

      // --- Shot detected ---
      if (matchPattern(MqttTopics.SHOT_DETECTED, topic)) {
        const msg = parseMqttMessage<ShotDetectedMessage>(topic, payload);
        if (msg) dispatch(mqttSlice.actions.shotDetected(msg));
      }

      // --- Session stopped ---
      if (matchPattern(MqttTopics.SESSION_STOPPED, topic)) {
        const msg = parseMqttMessage<SessionStoppedMessage>(topic, payload);
        if (msg) {
          stopCountdownTimer();
          dispatch(mqttSlice.actions.sessionStopped(msg));
        }
      }
    });
  }

  // ---- disconnected action: also tear down the client ----
  if (mqttSlice.actions.disconnected.match(action)) {
    stopCountdownTimer();
  }

  // ---- sessionStarted with countdown: the slice already set COUNTDOWN state,
  //      but we also need to schedule the fallback auto-transition in case
  //      the MQTT countdownComplete message never arrives. ----
  if (mqttSlice.actions.sessionStarted.match(action)) {
    const delay = (action as ReturnType<typeof mqttSlice.actions.sessionStarted>).payload.startDelaySeconds;
    if (delay && delay > 0) {
      // Safety timeout: transition to WAITING_FOR_SHOTS even if MQTT
      // countdown/complete message is lost. Add 500ms buffer.
      setTimeout(() => {
        const state = getState();
        if (state.mqtt.displayState === 'COUNTDOWN') {
          dispatch(mqttSlice.actions.countdownComplete({
            sessionId: state.mqtt.sessionData?.sessionId ?? 0,
            timestamp: Date.now(),
          }));
          stopCountdownTimer();
        }
      }, delay * 1000 + 500);
    }
  }

  return next(action);
};

// ---------------------------------------------------------------------------
// Exported helper so components can trigger a disconnect
// ---------------------------------------------------------------------------

export function disconnectMqttClient() {
  if (mqttClient) {
    mqttClient.end(true);
    mqttClient = null;
  }
}
