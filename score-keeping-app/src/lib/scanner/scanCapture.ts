export interface ScanCaptureOptions {
  /**
   * Max gap between keystrokes within one burst, in ms. Above this, the
   * burst-timing window resets — confirmed empirically against a NETUM
   * NT-EM61: it types far faster than this, well clear of human cadence.
   */
  maxKeystrokeGapMs?: number;
  /** Minimum decoded code length to be treated as a real scan. */
  minCodeLength?: number;
}

export interface ScanEvent {
  code: string;
  capturedAtMs: number;
  burstDurationMs: number;
}

const DEFAULT_OPTIONS: Required<ScanCaptureOptions> = {
  maxKeystrokeGapMs: 50,
  minCodeLength: 4,
};

/**
 * Captures a barcode scanner's HID keystroke burst — digits terminated by
 * Enter — without depending on any particular application input being
 * focused.
 *
 * The scanner was first tried with a Tab terminator, but Tab is
 * browser-reserved for focus navigation: with nothing left to focus in the
 * page it escapes straight to browser chrome (confirmed empirically — it
 * landed on the tab switcher), which page JS cannot reliably prevent in
 * every browser. Enter's default action is a no-op outside a form, so the
 * scanner is configured to terminate with CR instead.
 *
 * Listens in the capture phase at the document level, ahead of whatever
 * element currently has focus, so the terminating Enter can always be
 * prevented (e.g. it would otherwise submit a form if one happened to have
 * focus) regardless of what's focused. Individual digit keystrokes are
 * never intercepted, so normal typing is unaffected — if some other input
 * happens to have focus mid-scan, the digits are also typed into it as a
 * side effect; that's an accepted edge case, not the common path.
 *
 * Returns a cleanup function.
 *
 * KNOWN BUG (unresolved, deferred): against the physical NETUM NT-EM61,
 * scans reliably land as text but the terminating Enter has not been
 * reliably observed reaching a handler in this app — even though a plain
 * vanilla <textarea> with a raw addEventListener (tested via MDN's
 * KeyboardEvent.key demo) does receive a proper Enter keydown/keyup pair,
 * so the hardware and browser are not at fault. Not yet root-caused;
 * revisit before wiring this into the real timekeeper screen. Manual
 * shooter selection is unaffected and stays the primary supported path
 * until this is fixed.
 */
export function createScanCapture(onScan: (event: ScanEvent) => void, options: ScanCaptureOptions = {}): () => void {
  const { maxKeystrokeGapMs, minCodeLength } = { ...DEFAULT_OPTIONS, ...options };

  let buffer = '';
  let burstStartedAtMs = 0;
  let lastKeystrokeAtMs = 0;

  function handleKeyDown(event: KeyboardEvent): void {
    const now = Date.now();

    if (event.key === 'Enter') {
      // Read the buffer as-is — the terminator's own arrival gap isn't
      // meaningful burst timing (some scanners pace it differently from the
      // payload characters), so it must never trigger the reset below.
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
