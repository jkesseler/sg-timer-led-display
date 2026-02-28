import { createSlice, PayloadAction } from '@reduxjs/toolkit';
import { storage } from '../utils';
import { DefaultMqttSettings } from '../constants';
import type { MqttSettings } from '../types';
import type { RootState } from './store';

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

export interface SettingsState {
  broker: string;
  username: string;
  password: string;
  clientId: string;
  brightness: number;
}

function loadInitialSettings(): SettingsState {
  const persisted = storage.get<MqttSettings>('mqttSettings', DefaultMqttSettings);
  return {
    broker: persisted.broker ?? DefaultMqttSettings.broker,
    username: persisted.username ?? DefaultMqttSettings.username,
    password: persisted.password ?? DefaultMqttSettings.password,
    clientId: persisted.clientId ?? DefaultMqttSettings.clientId,
    brightness: storage.get<number>('brightness', 200),
  };
}

const initialState: SettingsState = loadInitialSettings();

// ---------------------------------------------------------------------------
// Slice
// ---------------------------------------------------------------------------

export const settingsSlice = createSlice({
  name: 'settings',
  initialState,
  reducers: {
    updateSettings(state, action: PayloadAction<Partial<SettingsState>>) {
      Object.assign(state, action.payload);

      // Persist to localStorage
      const mqttSettings: MqttSettings = {
        broker: state.broker,
        username: state.username,
        password: state.password,
        clientId: state.clientId,
        brightness: state.brightness,
      };
      storage.set('mqttSettings', mqttSettings);
    },

    setBrightness(state, action: PayloadAction<number>) {
      state.brightness = Math.max(10, Math.min(255, action.payload));
      storage.set('brightness', state.brightness);
    },

    increaseBrightness(state) {
      state.brightness = Math.min(255, state.brightness + 25);
      storage.set('brightness', state.brightness);
    },

    decreaseBrightness(state) {
      state.brightness = Math.max(10, state.brightness - 25);
      storage.set('brightness', state.brightness);
    },
  },
});

// ---------------------------------------------------------------------------
// Selectors
// ---------------------------------------------------------------------------

export const selectBrightness = (state: RootState) => state.settings.brightness;
export const selectSettings = (state: RootState) => state.settings;
export const selectMqttConnectionSettings = (state: RootState) => ({
  broker: state.settings.broker,
  username: state.settings.username,
  password: state.settings.password,
  clientId: state.settings.clientId,
});

export const {
  updateSettings,
  setBrightness,
  increaseBrightness,
  decreaseBrightness,
} = settingsSlice.actions;
