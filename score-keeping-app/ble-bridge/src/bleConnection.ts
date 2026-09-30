import { setTimeout as delay } from 'node:timers/promises';
import { findTimerDevice, normalizeUuid } from './protocols/timerDevices.js';
import type { Peripheral } from '@abandonware/noble';
import type { TimerEvent } from './protocols/timerEvent.js';
import type { DeviceFilter, TimerDeviceDefinition } from './protocols/timerDevices.js';
import type { ConnectionState, TimerIdentity } from './publisher.js';

type Noble = typeof import('@abandonware/noble');

/** Firmware BLE_RECONNECT_INTERVAL: wait between a lost/failed connection and the next scan. */
export const BLE_RECONNECT_INTERVAL_MS = 5000;
/** Firmware BLE_CONNECTION_DELAY_MS: let the adapter settle after stopping the scan. */
export const BLE_CONNECTION_DELAY_MS = 2000;
// noble's connect can hang indefinitely when the peripheral goes out of range mid-handshake.
const BLE_CONNECT_TIMEOUT_MS = 15000;

export interface BleConnectionOptions {
  filter: DeviceFilter;
  onConnectionStateChange(state: ConnectionState, timer?: TimerIdentity): void;
  onTimerEvent(event: TimerEvent): void;
  onError(error: Error): void;
}

interface DiscoveredTimer {
  peripheral: Peripheral;
  definition: TimerDeviceDefinition;
}

function waitForPoweredOn(noble: Noble, onError: (error: Error) => void) {
  if (noble._state === 'poweredOn') {
    return Promise.resolve();
  }

  return new Promise<void>((resolve) => {
    const handleStateChange = (state: string) => {
      if (state === 'poweredOn') {
        noble.removeListener('stateChange', handleStateChange);
        resolve();

        return;
      }

      onError(new Error(`Bluetooth adapter is ${state}; waiting for poweredOn`));
    };

    noble.on('stateChange', handleStateChange);
  });
}

function scanForTimer(noble: Noble, filter: DeviceFilter) {
  return new Promise<DiscoveredTimer>((resolve, reject) => {
    const handleDiscover = (peripheral: Peripheral) => {
      const definition = findTimerDevice({
        name: peripheral.advertisement.localName || undefined,
        address: peripheral.address,
        serviceUuids: peripheral.advertisement.serviceUuids ?? [],
      }, filter);

      if (!definition) {
        return;
      }

      noble.removeListener('discover', handleDiscover);
      noble.stopScanningAsync().then(() => resolve({ peripheral, definition }), reject);
    };

    noble.on('discover', handleDiscover);
    // No service filter: the M1A2F advertises none. Duplicates allowed because
    // the name may only arrive in a later scan response.
    noble.startScanningAsync([], true).catch(reject);
  });
}

function connectWithTimeout(peripheral: Peripheral) {
  return new Promise<void>((resolve, reject) => {
    const timeoutId = setTimeout(() => {
      peripheral.cancelConnect();
      reject(new Error(`Connection timed out after ${BLE_CONNECT_TIMEOUT_MS} ms`));
    }, BLE_CONNECT_TIMEOUT_MS);

    peripheral.connectAsync()
      .then(resolve, reject)
      .finally(() => clearTimeout(timeoutId));
  });
}

/**
 * Scan → connect → subscribe → wait for disconnect, forever, one timer at a
 * time. Mirrors the firmware's BLE loop in TimerApplication.
 */
export class BleConnection {
  private readonly options: BleConnectionOptions;
  private noble?: Noble;
  private activePeripheral?: Peripheral;
  private connectionState?: ConnectionState;
  private isStopped = false;
  private timer?: TimerIdentity;

  constructor(options: BleConnectionOptions) {
    this.options = options;
  }

  async run() {
    // Loaded lazily so the parser tests never load the native module.
    const { default: noble } = await import('@abandonware/noble');
    this.noble = noble;

    await waitForPoweredOn(noble, this.options.onError);

    while (!this.isStopped) {
      try {
        await this.connectOnce(noble);
      } catch (error) {
        if (this.isStopped) {
          return;
        }

        this.options.onError(error instanceof Error ? error : new Error(String(error)));
        this.setConnectionState('ERROR');
      }

      this.timer = undefined;
      await delay(BLE_RECONNECT_INTERVAL_MS);
    }
  }

  getTimer() {
    return this.timer;
  }

  async stop() {
    this.isStopped = true;
    await this.noble?.stopScanningAsync();
    await this.activePeripheral?.disconnectAsync();
  }

  private setConnectionState(state: ConnectionState) {
    if (this.connectionState === state) {
      return;
    }

    this.connectionState = state;
    this.options.onConnectionStateChange(state, this.timer);
  }

  private async connectOnce(noble: Noble) {
    this.setConnectionState('SCANNING');
    const { peripheral, definition } = await scanForTimer(noble, this.options.filter);

    const deviceName = peripheral.advertisement.localName || peripheral.address;
    const deviceModel = definition.getDeviceModel(peripheral.advertisement.localName || undefined);
    this.timer = { deviceName, deviceModel };
    this.activePeripheral = peripheral;

    try {
      await delay(BLE_CONNECTION_DELAY_MS);
      this.setConnectionState('CONNECTING');
      await connectWithTimeout(peripheral);

      const disconnected = new Promise<void>((resolve) => {
        peripheral.once('disconnect', () => resolve());
      });

      // noble's discovery never settles if the link drops mid-way, so race it against the disconnect.
      const isSubscribed = await Promise.race([
        this.subscribeToTimer(peripheral, definition, deviceModel).then(() => true),
        disconnected.then(() => false),
      ]);

      if (!isSubscribed) {
        throw new Error(`${deviceName} disconnected during service discovery`);
      }

      this.setConnectionState('CONNECTED');
      await disconnected;
      this.setConnectionState('DISCONNECTED');
    } catch (error) {
      if (peripheral.state === 'connected' || peripheral.state === 'connecting') {
        // Best effort: the error being rethrown is the one worth reporting.
        await peripheral.disconnectAsync().catch(() => undefined);
      }

      throw error;
    } finally {
      this.activePeripheral = undefined;
    }
  }

  private async subscribeToTimer(peripheral: Peripheral, definition: TimerDeviceDefinition, deviceModel: string) {
    const { characteristics } = await peripheral.discoverSomeServicesAndCharacteristicsAsync(
      [normalizeUuid(definition.serviceUuid)],
      [normalizeUuid(definition.characteristicUuid)],
    );
    const characteristic = characteristics[0];

    if (!characteristic) {
      throw new Error(`Notify characteristic ${definition.characteristicUuid} not found`);
    }

    const protocol = definition.createProtocol(deviceModel);
    characteristic.on('data', (data: Buffer) => {
      for (const event of protocol.processTimerData(data)) {
        this.options.onTimerEvent(event);
      }
    });
    await characteristic.subscribeAsync();
  }
}
