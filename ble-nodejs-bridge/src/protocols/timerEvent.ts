/** Mirrors the firmware's NormalizedShotData. All times are milliseconds. */
export interface NormalizedShot {
  sessionId: number;
  shotNumber: number;
  absoluteTimeMs: number;
  splitTimeMs: number;
  deviceModel: string;
  isFirstShot: boolean;
}

export interface SessionStartedEvent {
  type: 'sessionStarted';
  sessionId: number;
  startDelaySeconds: number;
}

export interface CountdownCompleteEvent {
  type: 'countdownComplete';
  sessionId: number;
}

export interface SessionStoppedEvent {
  type: 'sessionStopped';
  sessionId: number;
  totalShots: number;
}

export interface SessionSuspendedEvent {
  type: 'sessionSuspended';
  sessionId: number;
  totalShots: number;
}

export interface SessionResumedEvent {
  type: 'sessionResumed';
  sessionId: number;
  totalShots: number;
}

export interface ShotDetectedEvent {
  type: 'shotDetected';
  shot: NormalizedShot;
}

export type TimerEvent =
  | SessionStartedEvent
  | CountdownCompleteEvent
  | SessionStoppedEvent
  | SessionSuspendedEvent
  | SessionResumedEvent
  | ShotDetectedEvent;

/**
 * Stateful per-connection parser: bytes of one BLE notification in, zero or
 * more normalized events out. Create a fresh instance for every connection,
 * as the firmware discards session state when the link drops.
 */
export interface TimerProtocol {
  processTimerData(data: Uint8Array): TimerEvent[];
}
