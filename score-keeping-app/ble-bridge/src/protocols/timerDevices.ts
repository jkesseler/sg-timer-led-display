import { FrameProtocol } from './frameProtocol.js';
import { SG_TIMER_SHOT_INDEX_BASE, SgTimerProtocol } from './sgTimerProtocol.js';
import type { TimerProtocol } from './timerEvent.js';

/** What a BLE scan tells us about a peripheral, independent of the BLE library. */
export interface AdvertisedDevice {
  name?: string;
  address: string;
  serviceUuids: string[];
}

export interface TimerDeviceDefinition {
  serviceUuid: string;
  characteristicUuid: string;
  /** Shot number the device puts on the wire for the first shot of a session. */
  shotIndexBase: number;
  /** The device sends no notifications until the link is bonded and encrypted. */
  requiresPairing?: boolean;
  matchesDevice(device: AdvertisedDevice): boolean;
  getDeviceModel(deviceName?: string): string;
  createProtocol(deviceModel: string): TimerProtocol;
}

export interface DeviceFilter {
  nameFilter?: string;
  address?: string;
}

const BLUETOOTH_BASE_UUID_PATTERN = /^0000([0-9a-f]{4})00001000800000805f9b34fb$/;

/**
 * Lowercase, no dashes, and 16-bit form for UUIDs on the Bluetooth base UUID —
 * the form noble reports, so advertised and configured UUIDs compare equal.
 */
export function normalizeUuid(uuid: string) {
  const compact = uuid.toLowerCase().replaceAll('-', '');
  const shortForm = BLUETOOTH_BASE_UUID_PATTERN.exec(compact);

  if (shortForm) {
    return shortForm[1];
  }

  return compact;
}

function isAdvertisingService(device: AdvertisedDevice, serviceUuid: string) {
  const wantedUuid = normalizeUuid(serviceUuid);

  return device.serviceUuids.some(uuid => normalizeUuid(uuid) === wantedUuid);
}

const SPECIAL_PIE_M1A2F_NAME_PATTERN = /^SP M1A2 Timer [A-Za-z0-9]{4}$/;

// M1A2F and M1A2+ are different hardware that share the FFF0/FFF1 GATT profile;
// only the M1A2+ advertises the service, so the M1A2F is recognised by name.
export const SPECIAL_PIE_M1A2F: TimerDeviceDefinition = {
  serviceUuid: '0000FFF0-0000-1000-8000-00805F9B34FB',
  characteristicUuid: '0000FFF1-0000-1000-8000-00805F9B34FB',
  shotIndexBase: 1,
  matchesDevice: device => SPECIAL_PIE_M1A2F_NAME_PATTERN.test(device.name ?? ''),
  getDeviceModel: () => 'SP M1A2 Timer',
  createProtocol: deviceModel => new FrameProtocol({ deviceModel, shotIndexBase: SPECIAL_PIE_M1A2F.shotIndexBase }),
};

export const SG_TIMER: TimerDeviceDefinition = {
  serviceUuid: '7520FFFF-14D2-4CDA-8B6B-697C554C9311',
  characteristicUuid: '75200001-14D2-4CDA-8B6B-697C554C9311',
  shotIndexBase: SG_TIMER_SHOT_INDEX_BASE,
  // Verified on the SG Timer GO: it sends a security request (Just Works) and
  // drops the link ~30 s later if nobody pairs. The ESP32 stack pairs automatically.
  requiresPairing: true,
  matchesDevice: device => isAdvertisingService(device, SG_TIMER.serviceUuid),
  getDeviceModel: getSgTimerModel,
  createProtocol: deviceModel => new SgTimerProtocol(deviceModel),
};

export const SPECIAL_PIE_M1A2_PLUS: TimerDeviceDefinition = {
  serviceUuid: '0000FFF0-0000-1000-8000-00805F9B34FB',
  characteristicUuid: '0000FFF1-0000-1000-8000-00805F9B34FB',
  shotIndexBase: 1,
  matchesDevice: device => isAdvertisingService(device, SPECIAL_PIE_M1A2_PLUS.serviceUuid),
  getDeviceModel: () => 'Special Pie Timer',
  createProtocol: deviceModel => new FrameProtocol({ deviceModel, shotIndexBase: SPECIAL_PIE_M1A2_PLUS.shotIndexBase }),
};

// Shot index base 1 is assumed from the Special Pie timers; not yet verified on ASN hardware.
export const ASN_TRACKER: TimerDeviceDefinition = {
  serviceUuid: 'E5A10001-F1A2-4B63-9F8C-D7B781E35E2A',
  characteristicUuid: 'E5A10002-F1A2-4B63-9F8C-D7B781E35E2A',
  shotIndexBase: 1,
  matchesDevice: device => isAdvertisingService(device, ASN_TRACKER.serviceUuid),
  getDeviceModel: () => 'ASN Tracker',
  createProtocol: deviceModel => new FrameProtocol({ deviceModel, shotIndexBase: ASN_TRACKER.shotIndexBase }),
};

/** Scan priority from TimerApplication::processScanResults(); first match wins. */
export const TIMER_DEVICES = [SPECIAL_PIE_M1A2F, SG_TIMER, SPECIAL_PIE_M1A2_PLUS, ASN_TRACKER];

/** SG names follow `SG-SST4<variant>...`, where the variant letter identifies the model. */
export function getSgTimerModel(deviceName?: string) {
  if (!deviceName?.startsWith('SG-SST4') || deviceName.length <= 7) {
    return 'SG Timer';
  }

  const variant = deviceName[7];

  if (variant === 'A') {
    return 'SG Timer Sport';
  }

  if (variant === 'B') {
    return 'SG Timer GO';
  }

  return 'SG Timer';
}

function normalizeAddress(address: string) {
  return address.toLowerCase().replaceAll(/[:-]/g, '');
}

/** Returns the definition for a supported timer that passes the optional filter, if any. */
export function findTimerDevice(device: AdvertisedDevice, filter: DeviceFilter = {}) {
  if (filter.address && normalizeAddress(device.address) !== normalizeAddress(filter.address)) {
    return undefined;
  }

  if (filter.nameFilter) {
    const isNameMatch = (device.name ?? '').toLowerCase().includes(filter.nameFilter.toLowerCase());

    if (!isNameMatch) {
      return undefined;
    }
  }

  return TIMER_DEVICES.find(definition => definition.matchesDevice(device));
}
