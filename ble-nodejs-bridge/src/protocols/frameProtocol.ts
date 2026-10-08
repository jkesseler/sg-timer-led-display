import type { TimerEvent, TimerProtocol } from './timerEvent.js';

const FRAME_MESSAGE_TYPE = {
  SESSION_STOP: 0x18,
  SESSION_START: 0x34,
  SHOT_DETECTED: 0x36,
} as const;

interface FrameProtocolOptions {
  deviceModel: string;
  /** Shot number the device puts on the wire for the first shot of a session (0 or 1). */
  shotIndexBase: number;
}

/**
 * F8/F9 frame protocol shared by the Special Pie M1A2 / M1A2+ and ASN Tracker:
 * `[F8 F9] [type] [data...] [F9 F8]`, shot times as seconds + centiseconds.
 * Port of FrameProtocolTimerDevice::processTimerData().
 */
export class FrameProtocol implements TimerProtocol {
  private readonly deviceModel: string;
  private readonly shotIndexBase: number;
  private currentSessionId = 0;
  private totalShots = 0;
  private hasPreviousShot = false;
  private previousTimeMs = 0;

  constructor({ deviceModel, shotIndexBase }: FrameProtocolOptions) {
    this.deviceModel = deviceModel;
    this.shotIndexBase = shotIndexBase;
  }

  processTimerData(data: Uint8Array): TimerEvent[] {
    const hasFrameMarkers = data.length >= 6
      && data[0] === 0xF8
      && data[1] === 0xF9
      && data[data.length - 2] === 0xF9
      && data[data.length - 1] === 0xF8;

    if (!hasFrameMarkers) {
      return [];
    }

    switch (data[2]) {
      case FRAME_MESSAGE_TYPE.SESSION_START:
        return this.handleSessionStart(data[3]);

      case FRAME_MESSAGE_TYPE.SESSION_STOP:
        return this.handleSessionStop();

      // F8 F9 36 00 [seconds] [centiseconds] [shot#] [unknown] F9 F8
      case FRAME_MESSAGE_TYPE.SHOT_DETECTED:
        if (data.length < 10) {
          return [];
        }

        return this.handleShotDetected(data[4], data[5], data[6]);

      default:
        return [];
    }
  }

  private handleSessionStart(sessionId: number): TimerEvent[] {
    this.currentSessionId = sessionId;
    this.totalShots = 0;
    this.hasPreviousShot = false;
    this.previousTimeMs = 0;

    // The frame protocol has no start delay, so the countdown completes immediately.
    return [
      { type: 'sessionStarted', sessionId, startDelaySeconds: 0 },
      { type: 'countdownComplete', sessionId },
    ];
  }

  private handleSessionStop(): TimerEvent[] {
    this.hasPreviousShot = false;

    return [{ type: 'sessionStopped', sessionId: this.currentSessionId, totalShots: this.totalShots }];
  }

  private handleShotDetected(seconds: number, centiseconds: number, wireShotNumber: number): TimerEvent[] {
    const absoluteTimeMs = seconds * 1000 + centiseconds * 10;
    const isFirstShot = !this.hasPreviousShot;
    const splitTimeMs = isFirstShot ? 0 : absoluteTimeMs - this.previousTimeMs;
    const shotNumber = wireShotNumber - this.shotIndexBase + 1;

    this.hasPreviousShot = true;
    this.previousTimeMs = absoluteTimeMs;
    this.totalShots = shotNumber;

    return [{
      type: 'shotDetected',
      shot: {
        sessionId: this.currentSessionId,
        shotNumber,
        absoluteTimeMs,
        splitTimeMs,
        deviceModel: this.deviceModel,
        isFirstShot,
      },
    }];
  }
}
