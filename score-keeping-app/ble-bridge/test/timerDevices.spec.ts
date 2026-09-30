import { describe, expect, it } from 'vitest';
import {
  ASN_TRACKER,
  findTimerDevice,
  getSgTimerModel,
  normalizeUuid,
  SG_TIMER,
  SPECIAL_PIE_M1A2_PLUS,
  SPECIAL_PIE_M1A2F,
  TIMER_DEVICES,
} from '../src/protocols/timerDevices.js';
import type { AdvertisedDevice } from '../src/protocols/timerDevices.js';

function buildDevice(overrides: Partial<AdvertisedDevice>): AdvertisedDevice {
  return { address: 'aa:bb:cc:dd:ee:ff', serviceUuids: [], ...overrides };
}

describe('normalizeUuid', () => {
  it('shortens UUIDs on the Bluetooth base UUID to the 16-bit form', () => {
    expect(normalizeUuid('0000FFF0-0000-1000-8000-00805F9B34FB')).toBe('fff0');
  });

  it('lowercases and strips dashes from vendor UUIDs', () => {
    expect(normalizeUuid('7520FFFF-14D2-4CDA-8B6B-697C554C9311')).toBe('7520ffff14d24cda8b6b697c554c9311');
  });
});

describe('device matching', () => {
  it('matches the M1A2F name pattern exactly', () => {
    expect(SPECIAL_PIE_M1A2F.matchesDevice(buildDevice({ name: 'SP M1A2 Timer 2196' }))).toBe(true);
    expect(SPECIAL_PIE_M1A2F.matchesDevice(buildDevice({ name: 'SP M1A2 Timer 21-6' }))).toBe(false);
    expect(SPECIAL_PIE_M1A2F.matchesDevice(buildDevice({ name: 'SP M1A2 Timer 21966' }))).toBe(false);
    expect(SPECIAL_PIE_M1A2F.matchesDevice(buildDevice({}))).toBe(false);
  });

  it('matches SG Timer by service UUID in noble form', () => {
    expect(SG_TIMER.matchesDevice(buildDevice({ serviceUuids: ['7520ffff14d24cda8b6b697c554c9311'] }))).toBe(true);
  });

  it('matches M1A2+ by the 16-bit FFF0 service UUID', () => {
    expect(SPECIAL_PIE_M1A2_PLUS.matchesDevice(buildDevice({ serviceUuids: ['fff0'] }))).toBe(true);
  });

  it('matches ASN Tracker by service UUID', () => {
    expect(ASN_TRACKER.matchesDevice(buildDevice({ serviceUuids: ['e5a10001f1a24b639f8cd7b781e35e2a'] }))).toBe(true);
  });

  it('prefers M1A2F by name over M1A2+ by UUID for the same advertisement', () => {
    const device = buildDevice({ name: 'SP M1A2 Timer 2003', serviceUuids: ['fff0'] });

    expect(findTimerDevice(device)).toBe(SPECIAL_PIE_M1A2F);
  });

  it('prefers SG Timer over M1A2+ and ASN Tracker', () => {
    const device = buildDevice({ serviceUuids: ['e5a10001f1a24b639f8cd7b781e35e2a', 'fff0', '7520ffff14d24cda8b6b697c554c9311'] });

    expect(findTimerDevice(device)).toBe(SG_TIMER);
  });

  it('returns nothing for an unsupported device', () => {
    expect(findTimerDevice(buildDevice({ name: 'Headphones', serviceUuids: ['180d'] }))).toBeUndefined();
  });
});

describe('device filter', () => {
  const sgTimer = buildDevice({ name: 'SG-SST4A12345', address: 'F4:12:34:56:78:9A', serviceUuids: ['7520ffff14d24cda8b6b697c554c9311'] });

  it('matches the address case- and separator-insensitively', () => {
    expect(findTimerDevice(sgTimer, { address: 'f4:12:34:56:78:9a' })).toBe(SG_TIMER);
    expect(findTimerDevice(sgTimer, { address: 'f4123456789a' })).toBe(SG_TIMER);
    expect(findTimerDevice(sgTimer, { address: '00:00:00:00:00:00' })).toBeUndefined();
  });

  it('matches a case-insensitive name substring', () => {
    expect(findTimerDevice(sgTimer, { nameFilter: 'sst4a' })).toBe(SG_TIMER);
    expect(findTimerDevice(sgTimer, { nameFilter: 'SP M1A2' })).toBeUndefined();
  });
});

describe('device models and shot index base', () => {
  it('refines the SG model from the advertised name', () => {
    expect(getSgTimerModel('SG-SST4A12345')).toBe('SG Timer Sport');
    expect(getSgTimerModel('SG-SST4B12345')).toBe('SG Timer GO');
    expect(getSgTimerModel('SG-SST4C12345')).toBe('SG Timer');
    expect(getSgTimerModel('SG-SST4')).toBe('SG Timer');
    expect(getSgTimerModel(undefined)).toBe('SG Timer');
  });

  it('keeps each device model non-empty', () => {
    for (const definition of TIMER_DEVICES) {
      expect(definition.getDeviceModel()).not.toBe('');
    }
  });

  it('matches the firmware SHOT_INDEX_BASE per device', () => {
    expect(SPECIAL_PIE_M1A2F.shotIndexBase).toBe(1);
    expect(SPECIAL_PIE_M1A2_PLUS.shotIndexBase).toBe(1);
    expect(ASN_TRACKER.shotIndexBase).toBe(1);
    expect(SG_TIMER.shotIndexBase).toBe(0);
  });
});
