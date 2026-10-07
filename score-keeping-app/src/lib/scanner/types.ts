export interface ScanCaptureOptions {
  /** Above this gap between keystrokes, in ms, a new burst starts. Scanners type far faster; humans don't. */
  maxKeystrokeGapMs?: number;
  minCodeLength?: number;
}

export interface ScanEvent {
  code: string;
  capturedAtMs: number;
  burstDurationMs: number;
}
