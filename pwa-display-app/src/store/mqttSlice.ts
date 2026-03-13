import { createSlice, PayloadAction } from '@reduxjs/toolkit';
import { DisplayState, ConnectionState } from '../constants';
import type {
  ShotData,
  SessionData,
  KnownDevice,
  ConnectionStateMessage,
  SessionStartedMessage,
  SessionStoppedMessage,
  ShotDetectedMessage,
  CountdownCompleteMessage,
  DeviceInfoMessage,
} from '../types';
import type { RootState } from './store';

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

export interface MqttState {
  // Connection
  isConnecting: boolean;
  isConnected: boolean;
  connectionError: string | null;

  // Display
  displayState: DisplayState;
  shotData: ShotData | null;
  sessionData: SessionData | null;
  deviceName: string | null;

  // Multi-device
  knownDevices: KnownDevice[];
  selectedDeviceId: string | null;

  // Countdown
  countdownRemainingMs: number;
}

const initialState: MqttState = {
  isConnecting: false,
  isConnected: false,
  connectionError: null,

  displayState: DisplayState.STARTUP,
  shotData: null,
  sessionData: null,
  deviceName: null,

  knownDevices: [],
  selectedDeviceId: null,

  countdownRemainingMs: 0,
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Pre-session states that can be overridden by a CONNECTED event */
const PRE_SESSION_STATES: DisplayState[] = [
  DisplayState.STARTUP,
  DisplayState.DISCONNECTED,
  DisplayState.SCANNING,
  DisplayState.CONNECTING,
  DisplayState.CONNECTED,
];

function resolveActiveDeviceId(
  devices: KnownDevice[],
  pinned: string | null,
): string | null {
  if (pinned) return pinned;
  return devices.find(d => d.presence === 'online')?.deviceId ?? null;
}

// ---------------------------------------------------------------------------
// Slice
// ---------------------------------------------------------------------------

export const mqttSlice = createSlice({
  name: 'mqtt',
  initialState,
  reducers: {
    // -----------  Connection lifecycle  -----------

    startConnecting(state) {
      state.isConnecting = true;
      state.connectionError = null;
    },

    connected(state) {
      state.isConnected = true;
      state.isConnecting = false;
      state.connectionError = null;
    },

    disconnected(state) {
      state.isConnected = false;
      state.isConnecting = false;
    },

    connectionError(state, action: PayloadAction<string>) {
      state.connectionError = action.payload;
      state.isConnected = false;
      state.isConnecting = false;
    },

    // -----------  Device discovery  -----------

    selectDevice(state, action: PayloadAction<string | null>) {
      state.selectedDeviceId = action.payload;
    },

    devicePresenceUpdated(
      state,
      action: PayloadAction<{ deviceId: string; presence: 'online' | 'offline' }>,
    ) {
      const { deviceId, presence } = action.payload;
      const idx = state.knownDevices.findIndex(d => d.deviceId === deviceId);
      if (idx === -1) {
        state.knownDevices.push({ deviceId, presence, lastSeenMs: Date.now() });
      } else {
        state.knownDevices[idx].presence = presence;
        state.knownDevices[idx].lastSeenMs = Date.now();
        presence === 'offline' && (state.knownDevices[idx].deviceName = undefined);
      }
    },

    deviceInfoUpdated(
      state,
      action: PayloadAction<{
        deviceId: string;
        info: DeviceInfoMessage;
      }>,
    ) {
      const { deviceId, info } = action.payload;
      const idx = state.knownDevices.findIndex(d => d.deviceId === deviceId);
      const patch = {
        deviceName: info.deviceName,
        deviceModel: info.deviceModel,
        firmwareVersion: info.firmwareVersion,
        lastSeenMs: Date.now(),
      };
      if (idx === -1) {
        state.knownDevices.push({
          deviceId,
          presence: 'online',
          ...patch,
        });
      } else {
        Object.assign(state.knownDevices[idx], patch);
      }

      // Also set the device name for display
      state.deviceName = info.deviceName || info.deviceModel || null;
    },

    // -----------  Timer connection state (from ESP32)  -----------

    timerConnectionStateChanged(state, action: PayloadAction<ConnectionStateMessage>) {
      const msg = action.payload;
      state.deviceName = msg.deviceName || null;

      switch (msg.state) {
        case ConnectionState.CONNECTED:
          if (PRE_SESSION_STATES.includes(state.displayState)) {
            state.displayState = DisplayState.CONNECTED;
          }
          break;
        case ConnectionState.SCANNING:
          state.displayState = DisplayState.SCANNING;
          break;
        case ConnectionState.CONNECTING:
          state.displayState = DisplayState.CONNECTING;
          break;
        case ConnectionState.DISCONNECTED:
        case ConnectionState.ERROR:
          state.displayState = DisplayState.DISCONNECTED;
          state.deviceName = null;
          state.shotData = null;
          state.sessionData = null;
          state.countdownRemainingMs = 0;
          break;
      }
    },

    // -----------  Session events  -----------

    sessionStarted(state, action: PayloadAction<SessionStartedMessage>) {
      const msg = action.payload;
      const newSession: SessionData = {
        sessionId: msg.sessionId,
        isActive: true,
        totalShots: 0,
        startTimestamp: msg.timestamp,
        startDelaySeconds: msg.startDelaySeconds || 0,
        countdownStartTime: Date.now(),
      };
      state.sessionData = newSession;

      if (msg.startDelaySeconds && msg.startDelaySeconds > 0) {
        state.displayState = DisplayState.COUNTDOWN;
        state.countdownRemainingMs = msg.startDelaySeconds * 1000;
      } else {
        state.displayState = DisplayState.WAITING_FOR_SHOTS;
        state.countdownRemainingMs = 0;
      }
    },

    /** Called once per animation-frame tick while countdown is active */
    countdownTick(state, action: PayloadAction<number>) {
      state.countdownRemainingMs = Math.max(0, action.payload);
    },

    countdownComplete(state, action: PayloadAction<CountdownCompleteMessage>) {
      void action; // payload used by beepMiddleware side-effect
      state.displayState = DisplayState.WAITING_FOR_SHOTS;
      state.countdownRemainingMs = 0;
    },

    sessionStopped(state, action: PayloadAction<SessionStoppedMessage>) {
      const msg = action.payload;
      if (state.sessionData) {
        state.sessionData.isActive = false;
        state.sessionData.totalShots = msg.totalShots ?? state.sessionData.totalShots;
      }

      // If session ended includes last shot time, show it
      if (msg.lastShotTimeMs !== undefined && msg.lastShotTimeMs > 0) {
        state.shotData = {
          sessionId: msg.sessionId,
          shotNumber: state.sessionData?.totalShots || 0,
          absoluteTimeMs: msg.lastShotTimeMs,
          splitTimeMs: 0,
          deviceModel: 'Timer',
          isFirstShot: false,
          timestamp: msg.timestamp,
        };
      }

      state.displayState = DisplayState.SESSION_ENDED;
      state.countdownRemainingMs = 0;
    },

    // -----------  Shot events  -----------

    shotDetected(state, action: PayloadAction<ShotDetectedMessage>) {
      const msg = action.payload;
      state.shotData = {
        sessionId: msg.sessionId,
        shotNumber: msg.shotNumber,
        absoluteTimeMs: msg.absoluteTimeMs,
        splitTimeMs: msg.splitTimeMs,
        deviceModel: msg.deviceModel,
        isFirstShot: msg.isFirstShot,
        timestamp: msg.timestamp,
      };
      state.displayState = DisplayState.SHOWING_SHOT;

      if (state.sessionData) {
        state.sessionData.totalShots = Math.max(
          state.sessionData.totalShots,
          msg.shotNumber,
        );
      }
    },

    // -----------  Startup transition  -----------

    startupComplete(state) {
      if (state.displayState === DisplayState.STARTUP) {
        state.displayState = DisplayState.DISCONNECTED;
      }
    },
  },
});

// ---------------------------------------------------------------------------
// Selectors
// ---------------------------------------------------------------------------

export const selectIsConnected = (state: RootState) => state.mqtt.isConnected;
export const selectIsConnecting = (state: RootState) => state.mqtt.isConnecting;
export const selectConnectionError = (state: RootState) => state.mqtt.connectionError;
export const selectDisplayState = (state: RootState) => state.mqtt.displayState;
export const selectShotData = (state: RootState) => state.mqtt.shotData;
export const selectSessionData = (state: RootState) => state.mqtt.sessionData;
export const selectDeviceName = (state: RootState) => state.mqtt.deviceName;
export const selectKnownDevices = (state: RootState) => state.mqtt.knownDevices;
export const selectSelectedDeviceId = (state: RootState) => state.mqtt.selectedDeviceId;
export const selectCountdownRemainingMs = (state: RootState) => state.mqtt.countdownRemainingMs;

export const selectActiveDeviceId = (state: RootState) =>
  resolveActiveDeviceId(state.mqtt.knownDevices, state.mqtt.selectedDeviceId);

export const selectCurrentDeviceId = (state: RootState) =>
  resolveActiveDeviceId(state.mqtt.knownDevices, state.mqtt.selectedDeviceId);

export const selectIsCurrentDeviceConnected = (state: RootState) => {
  const currentDeviceId = selectCurrentDeviceId(state);
  if (!currentDeviceId) return false;

  const currentDevice = state.mqtt.knownDevices.find(
    (device) => device.deviceId === currentDeviceId,
  );

  return currentDevice?.presence === 'online';
};

export const {
  startConnecting,
  connected,
  disconnected,
  connectionError,
  selectDevice,
  devicePresenceUpdated,
  deviceInfoUpdated,
  timerConnectionStateChanged,
  sessionStarted,
  countdownTick,
  countdownComplete,
  sessionStopped,
  shotDetected,
  startupComplete,
} = mqttSlice.actions;
