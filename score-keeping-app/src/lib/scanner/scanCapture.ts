import type { ScanCaptureOptions, ScanEvent } from './types';

// Keys a scanner may interleave with the payload; they neither add to nor break a burst.
const IGNORED_KEYS = new Set(['Shift', 'Control', 'Alt', 'AltGraph', 'Meta', 'NumLock', 'CapsLock', 'ScrollLock']);

function isEnterKey(event: KeyboardEvent): boolean {
  return event.key === 'Enter' || event.code === 'Enter' || event.code === 'NumpadEnter';
}

const DEFAULT_OPTIONS: Required<ScanCaptureOptions> = {
  maxKeystrokeGapMs: 50,
  minCodeLength: 4
};

/**
 * Captures a barcode scanner's keystroke burst (digits, then Enter) whatever has focus.
 *
 * The scanner terminates with Enter, not Tab: with nothing left to focus, Tab
 * escapes to browser chrome, which page JS cannot reliably prevent. Listening
 * in the capture phase lets the Enter always be prevented; digits are never
 * intercepted, so they also reach a focused input as a side effect.
 *
 * Modifier and lock keys are skipped instead of breaking the burst: scanners
 * in keyboard-emulation mode send them around the digits (NETUM NT-EM61).
 */
export function createScanCapture(onScan: (event: ScanEvent) => void, options: ScanCaptureOptions = {}): () => void {
  const { maxKeystrokeGapMs, minCodeLength } = { ...DEFAULT_OPTIONS, ...options };

  let buffer = '';
  let burstStartedAtMs = 0;
  let lastKeystrokeAtMs = 0;

  function handleKeyDown(event: KeyboardEvent): void {
    if (IGNORED_KEYS.has(event.key)) {
      return;
    }

    const now = Date.now();

    if (isEnterKey(event)) {
      // Some scanners pace the terminator differently, so its gap must never reset the buffer.
      const code = buffer;
      buffer = '';
      lastKeystrokeAtMs = now;

      const isScanShaped = code.length >= minCodeLength && /^[0-9]+$/.test(code);
      if (!isScanShaped) {
        return;
      }

      event.preventDefault();
      onScan({ code, capturedAtMs: now, burstDurationMs: now - burstStartedAtMs });

      return;
    }

    const gapMs = now - lastKeystrokeAtMs;
    lastKeystrokeAtMs = now;

    if (event.key.length === 1) {
      if (gapMs > maxKeystrokeGapMs) {
        buffer = '';
        burstStartedAtMs = now;
      }
      buffer += event.key;
    } else {
      // A non-printable key (Shift, Tab, arrows, ...) breaks the burst.
      buffer = '';
    }
  }

  document.addEventListener('keydown', handleKeyDown, { capture: true });

  return () => document.removeEventListener('keydown', handleKeyDown, { capture: true });
}
