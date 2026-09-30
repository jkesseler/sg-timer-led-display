import { createSlice, PayloadAction } from '@reduxjs/toolkit';
import { storage } from '@/lib/display/utils';
import type { MqttServerConfig } from '@/lib/mqtt/config';
import type { MqttSettings } from '@/lib/mqtt/types';
import type { RootState } from './store';

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

export interface BrokerDefaults {
  broker: string;
  username: string;
  password: string;
}

export interface SettingsState {
  broker: string;
  username: string;
  password: string;
  clientId: string;
  brightness: number;
  defaults: BrokerDefaults;
}

const SETTINGS_STORAGE_KEY = 'mqttSettings';

/** Same host as the page: lets the TV's browser reach the laptop's broker without per-device setup. */
function getFallbackBroker(): string {
  if (typeof window === 'undefined') {
    return '';
  }

  return `ws://${window.location.hostname}:9001`;
}

/** Precedence: a broker saved in the Settings panel > the server's config > the page's own host. */
export function buildInitialSettings(serverConfig?: MqttServerConfig): SettingsState {
  const defaults: BrokerDefaults = {
    broker: serverConfig?.wsUrl || getFallbackBroker(),
    username: serverConfig?.username ?? '',
    password: serverConfig?.password ?? ''
  };
  const saved = storage.get<MqttSettings | null>(SETTINGS_STORAGE_KEY, null);

  return {
    broker: saved?.broker || defaults.broker,
    username: saved ? saved.username : defaults.username,
    password: saved ? saved.password : defaults.password,
    clientId: saved?.clientId || `display-${Math.random().toString(16).substring(2, 8)}`,
    brightness: storage.get<number>('brightness', 200),
    defaults
  };
}

const initialState: SettingsState = buildInitialSettings();

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
        brightness: state.brightness
      };
      storage.set(SETTINGS_STORAGE_KEY, mqttSettings);
    },

    resetBrokerSettings(state) {
      storage.remove(SETTINGS_STORAGE_KEY);
      state.broker = state.defaults.broker;
      state.username = state.defaults.username;
      state.password = state.defaults.password;
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
    }
  }
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
  clientId: state.settings.clientId
});

export const {
  updateSettings,
  resetBrokerSettings,
  setBrightness,
  increaseBrightness,
  decreaseBrightness
} = settingsSlice.actions;
