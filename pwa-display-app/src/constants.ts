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
export const MqttTopics: IMqttTopics = {
  CONNECTION_STATE: 'timer/connection/state',
  SESSION_STARTED: 'timer/session/started',
  SESSION_STOPPED: 'timer/session/stopped',
  SESSION_SUSPENDED: 'timer/session/suspended',
  SESSION_RESUMED: 'timer/session/resumed',
  SHOT_DETECTED: 'timer/shot/detected',
  COUNTDOWN_COMPLETE: 'timer/countdown/complete',
  DEVICE_INFO: 'timer/device/info'
};

// Default MQTT settings
export const DefaultMqttSettings = {
  broker: 'ws://127.0.0.1:9001', // MQTT TCP port
  username: '',
  password: '',
  clientId: `pwa-display-${Math.random().toString(16).substring(2, 8)}`
};
