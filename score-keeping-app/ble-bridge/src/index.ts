import { setTimeout as delay } from 'node:timers/promises';
import dotenv from 'dotenv';
import { BleConnection } from './bleConnection.js';
import { connectPublisher } from './publisher.js';
import type { TimerEvent } from './protocols/timerEvent.js';
import type { ConnectionState, TimerIdentity } from './publisher.js';

// quiet: dotenv 17 otherwise prints a banner, and stdout is reserved for event lines.
dotenv.config({ quiet: true });

const SHUTDOWN_BLE_TIMEOUT_MS = 2000;

const deviceId = process.env.BRIDGE_DEVICE_ID?.trim();

if (!deviceId) {
  console.error('[ble-bridge] BRIDGE_DEVICE_ID is required (the ID this bridge publishes under: timer/<id>/...)');
  process.exit(1);
}

const { publisher, close } = connectPublisher({
  brokerUrl: process.env.MQTT_BROKER_URL || 'mqtt://localhost:1883',
  username: process.env.MQTT_USERNAME || undefined,
  password: process.env.MQTT_PASSWORD || undefined,
  deviceId,
});

function formatSeconds(timeMs: number) {
  return (timeMs / 1000).toFixed(2);
}

function logTimerEvent(event: TimerEvent) {
  switch (event.type) {
    case 'sessionStarted':
      console.log(`session ${event.sessionId} started (start delay ${event.startDelaySeconds}s)`);
      break;
    case 'shotDetected':
      console.log(`shot #${event.shot.shotNumber}: ${formatSeconds(event.shot.absoluteTimeMs)}s (split ${formatSeconds(event.shot.splitTimeMs)}s)`);
      break;
    case 'sessionStopped':
      console.log(`session ${event.sessionId} stopped (${event.totalShots} shots)`);
      break;
    case 'sessionSuspended':
      console.log(`session ${event.sessionId} suspended`);
      break;
    case 'sessionResumed':
      console.log(`session ${event.sessionId} resumed`);
      break;
  }
}

function handleConnectionStateChange(state: ConnectionState, timer?: TimerIdentity) {
  publisher.publishConnectionState(state, timer);

  if (state === 'CONNECTED' && timer) {
    publisher.publishDeviceInfo(timer);
    console.log(`connected to ${timer.deviceName} (${timer.deviceModel})`);
  }

  if (state === 'DISCONNECTED' && timer) {
    console.log(`disconnected from ${timer.deviceName}`);
  }
}

function handleTimerEvent(event: TimerEvent) {
  logTimerEvent(event);
  publisher.publishTimerEvent(event);
}

const bleConnection = new BleConnection({
  filter: {
    nameFilter: process.env.TIMER_NAME_FILTER || undefined,
    address: process.env.TIMER_ADDRESS || undefined,
  },
  onConnectionStateChange: handleConnectionStateChange,
  onTimerEvent: handleTimerEvent,
  onError: (error) => {
    console.error(`[ble-bridge] ${error.message}`);
  },
});

async function shutdown() {
  const timer = bleConnection.getTimer();

  // noble's stop/disconnect can wait forever on a half-open link; the retained MQTT state matters more.
  const bleStopped = bleConnection.stop().catch((error: unknown) => {
    console.error(`[ble-bridge] BLE shutdown failed: ${error instanceof Error ? error.message : String(error)}`);
  });
  await Promise.race([bleStopped, delay(SHUTDOWN_BLE_TIMEOUT_MS)]);
  await close(timer);
  process.exit(0);
}

process.once('SIGINT', shutdown);
process.once('SIGTERM', shutdown);

bleConnection.run().catch((error: unknown) => {
  console.error('[ble-bridge] BLE stopped:', error instanceof Error ? error : new Error(String(error)));
  process.exit(1);
});
