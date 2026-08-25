// Wordmark shown on the STARTUP screen
export const STARTUP_TEXT = 'J.K. PewPew Timer';

// How long the STARTUP screen holds before falling back to DISCONNECTED,
// in case no connection/state message arrives sooner.
export const STARTUP_DISPLAY_MS = 3000;

// Default MQTT settings
export const DefaultMqttSettings = {
  broker: 'ws://127.0.0.1:9001', // MQTT WebSocket port
  username: 'pewpew',
  password: 'timer',
  clientId: `display-${Math.random().toString(16).substring(2, 8)}`
};
