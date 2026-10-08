import type { TimerEvent, TimerProtocol } from './timerEvent.js';

const SG_TIMER_EVENT = {
  SESSION_STARTED: 0x00,
  SESSION_SUSPENDED: 0x01,
  SESSION_RESUMED: 0x02,
  SESSION_STOPPED: 0x03,
  SHOT_DETECTED: 0x04,
  SESSION_SET_BEGIN: 0x05,
} as const;

/** The SG Timer counts shots from 0; shot numbers are normalized to 1-based. */
export const SG_TIMER_SHOT_INDEX_BASE = 0;

/**
 * SG Timer BLE API 3.2: `[len] [event] [payload...]`, big-endian, times in ms.
 * Port of SGTimer::processTimerData().
 */
export class SgTimerProtocol implements TimerProtocol {
  private readonly deviceModel: string;
  private currentSessionId = 0;
  private hasFirstShot = false;
  private previousShotTimeMs = 0;

  constructor(deviceModel: string) {
    this.deviceModel = deviceModel;
  }

  processTimerData(data: Uint8Array): TimerEvent[] {
    if (data.length < 2) {
      return [];
    }

    // The length byte counts the bytes after itself; anything else is a torn packet.
    if (data[0] !== data.length - 1) {
      return [];
    }

    const view = new DataView(data.buffer, data.byteOffset, data.byteLength);

    switch (data[1]) {
      case SG_TIMER_EVENT.SESSION_STARTED: {
        if (data.length < 8) {
          return [];
        }

        this.currentSessionId = view.getUint32(2);
        this.hasFirstShot = false;
        this.previousShotTimeMs = 0;
        const startDelayDeciseconds = view.getUint16(6);

        return [{
          type: 'sessionStarted',
          sessionId: this.currentSessionId,
          startDelaySeconds: startDelayDeciseconds / 10,
        }];
      }

      // Suspended/resumed/stopped report the session ID remembered from
      // SESSION_STARTED, not the one in the packet, exactly as the firmware does.
      case SG_TIMER_EVENT.SESSION_SUSPENDED: {
        if (data.length < 8) {
          return [];
        }

        return [{ type: 'sessionSuspended', sessionId: this.currentSessionId, totalShots: view.getUint16(6) }];
      }

      case SG_TIMER_EVENT.SESSION_RESUMED: {
        if (data.length < 8) {
          return [];
        }

        return [{ type: 'sessionResumed', sessionId: this.currentSessionId, totalShots: view.getUint16(6) }];
      }

      case SG_TIMER_EVENT.SESSION_STOPPED: {
        if (data.length < 8) {
          return [];
        }

        this.hasFirstShot = false;
        this.previousShotTimeMs = 0;

        return [{ type: 'sessionStopped', sessionId: this.currentSessionId, totalShots: view.getUint16(6) }];
      }

      case SG_TIMER_EVENT.SHOT_DETECTED: {
        if (data.length < 12) {
          return [];
        }

        const wireShotNumber = view.getUint16(6);
        const absoluteTimeMs = view.getUint32(8);
        const isFirstShot = !this.hasFirstShot;
        const splitTimeMs = isFirstShot ? 0 : absoluteTimeMs - this.previousShotTimeMs;

        this.hasFirstShot = true;
        this.previousShotTimeMs = absoluteTimeMs;

        return [{
          type: 'shotDetected',
          shot: {
            sessionId: view.getUint32(2),
            shotNumber: wireShotNumber - SG_TIMER_SHOT_INDEX_BASE + 1,
            absoluteTimeMs,
            splitTimeMs,
            deviceModel: this.deviceModel,
            isFirstShot,
          },
        }];
      }

      case SG_TIMER_EVENT.SESSION_SET_BEGIN: {
        if (data.length < 6) {
          return [];
        }

        return [{ type: 'countdownComplete', sessionId: this.currentSessionId }];
      }

      default:
        return [];
    }
  }
}
