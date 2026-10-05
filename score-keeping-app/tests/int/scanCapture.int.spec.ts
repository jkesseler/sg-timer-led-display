import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { createScanCapture } from '@/lib/scanner/scanCapture';
import type { ScanEvent } from '@/lib/scanner/scanCapture';

const SCANNER_GAP_MS = 10;
const HUMAN_GAP_MS = 150;

function pressKey(key: string, gapMs: number, code = ''): KeyboardEvent {
  vi.advanceTimersByTime(gapMs);
  const event = new KeyboardEvent('keydown', { key, code, bubbles: true, cancelable: true });
  document.body.dispatchEvent(event);

  return event;
}

/** Types the digits at the given pace and returns the terminating Enter event. */
function typeCode(digits: string, gapMs: number): KeyboardEvent {
  for (const digit of digits) {
    pressKey(digit, gapMs);
  }

  return pressKey('Enter', gapMs, 'Enter');
}

describe('createScanCapture', () => {
  let scans: ScanEvent[];
  let stopCapture: () => void;

  beforeEach(() => {
    vi.useFakeTimers();
    vi.setSystemTime(new Date('2026-10-05T10:00:00Z'));
    scans = [];
    stopCapture = createScanCapture(scan => scans.push(scan));
  });

  afterEach(() => {
    stopCapture();
    vi.useRealTimers();
  });

  it('reports a fast digit burst ended by Enter and prevents the Enter', () => {
    const enter = typeCode('211764', SCANNER_GAP_MS);

    expect(scans.map(scan => scan.code)).toEqual(['211764']);
    expect(enter.defaultPrevented).toBe(true);
  });

  it('ignores typing at human speed and leaves its Enter alone', () => {
    const enter = typeCode('211764', HUMAN_GAP_MS);

    expect(scans).toEqual([]);
    expect(enter.defaultPrevented).toBe(false);
  });

  it('keeps the burst when the scanner interleaves modifier keys', () => {
    pressKey('NumLock', SCANNER_GAP_MS);
    for (const digit of '199669') {
      pressKey('Shift', 1);
      pressKey(digit, SCANNER_GAP_MS);
    }
    pressKey('Enter', SCANNER_GAP_MS, 'Enter');

    expect(scans.map(scan => scan.code)).toEqual(['199669']);
  });

  it('accepts the numpad Enter', () => {
    for (const digit of '45678') {
      pressKey(digit, SCANNER_GAP_MS);
    }
    pressKey('Enter', SCANNER_GAP_MS, 'NumpadEnter');

    expect(scans.map(scan => scan.code)).toEqual(['45678']);
  });

  it('ignores codes shorter than the minimum length', () => {
    typeCode('123', SCANNER_GAP_MS);

    expect(scans).toEqual([]);
  });

  it('drops a key typed by hand just before a scan', () => {
    pressKey('9', HUMAN_GAP_MS);
    pressKey('7', HUMAN_GAP_MS);
    for (const digit of '0754') {
      pressKey(digit, SCANNER_GAP_MS);
    }
    pressKey('Enter', SCANNER_GAP_MS, 'Enter');

    expect(scans.map(scan => scan.code)).toEqual(['70754']);
  });

  it('stops listening after cleanup', () => {
    stopCapture();
    typeCode('211764', SCANNER_GAP_MS);

    expect(scans).toEqual([]);
  });
});
