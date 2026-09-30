import { beforeEach, describe, expect, it } from 'vitest';
import { ASN_TRACKER, SPECIAL_PIE_M1A2_PLUS, SPECIAL_PIE_M1A2F } from '../src/protocols/timerDevices.js';
import type { NormalizedShot, TimerEvent, TimerProtocol } from '../src/protocols/timerEvent.js';

const START_SESSION_1 = Uint8Array.of(0xF8, 0xF9, 0x34, 0x01, 0x00, 0x00, 0xF9, 0xF8);
const STOP_SESSION_1 = Uint8Array.of(0xF8, 0xF9, 0x18, 0x01, 0x00, 0x00, 0xF9, 0xF8);

function buildShotFrame(seconds: number, centiseconds: number, wireShotNumber: number) {
  return Uint8Array.of(0xF8, 0xF9, 0x36, 0x00, seconds, centiseconds, wireShotNumber, 0x00, 0xF9, 0xF8);
}

function getShot(events: TimerEvent[]): NormalizedShot {
  expect(events).toHaveLength(1);
  const [event] = events;

  if (event.type !== 'shotDetected') {
    throw new Error(`Expected shotDetected, got ${event.type}`);
  }

  return event.shot;
}

describe('Special Pie M1A2+ frame protocol', () => {
  let protocol: TimerProtocol;

  beforeEach(() => {
    protocol = SPECIAL_PIE_M1A2_PLUS.createProtocol(SPECIAL_PIE_M1A2_PLUS.getDeviceModel());
  });

  it('parses the session ID on session start, with no start delay', () => {
    const events = protocol.processTimerData(Uint8Array.of(0xF8, 0xF9, 0x34, 0x05, 0x00, 0x00, 0xF9, 0xF8));

    expect(events[0]).toEqual({ type: 'sessionStarted', sessionId: 5, startDelaySeconds: 0 });
  });

  it('emits countdown complete immediately after session start', () => {
    const events = protocol.processTimerData(START_SESSION_1);

    expect(events.map(event => event.type)).toEqual(['sessionStarted', 'countdownComplete']);
    expect(events[1]).toEqual({ type: 'countdownComplete', sessionId: 1 });
  });

  it('converts seconds + centiseconds to milliseconds', () => {
    const shot = getShot(protocol.processTimerData(buildShotFrame(0x03, 0x2D, 0x01)));

    expect(shot.shotNumber).toBe(1);
    expect(shot.absoluteTimeMs).toBe(3450);
    expect(shot.isFirstShot).toBe(true);
    expect(shot.deviceModel).toBe('Special Pie Timer');
  });

  it('calculates the split time', () => {
    expect(getShot(protocol.processTimerData(buildShotFrame(0x02, 0x1E, 0x01))).absoluteTimeMs).toBe(2300);
    const shot = getShot(protocol.processTimerData(buildShotFrame(0x03, 0x05, 0x02)));

    expect(shot.shotNumber).toBe(2);
    expect(shot.absoluteTimeMs).toBe(3050);
    expect(shot.splitTimeMs).toBe(750);
    expect(shot.isFirstShot).toBe(false);
  });

  it('borrows from seconds when centiseconds drop below the previous shot', () => {
    expect(getShot(protocol.processTimerData(buildShotFrame(0x02, 0x50, 0x01))).absoluteTimeMs).toBe(2800);
    const shot = getShot(protocol.processTimerData(buildShotFrame(0x03, 0x0F, 0x01)));

    expect(shot.absoluteTimeMs).toBe(3150);
    expect(shot.splitTimeMs).toBe(350);
  });

  it('reports the started session and last shot number on session stop', () => {
    protocol.processTimerData(Uint8Array.of(0xF8, 0xF9, 0x34, 0x04, 0x00, 0x00, 0xF9, 0xF8));
    protocol.processTimerData(buildShotFrame(0x01, 0x00, 0x01));
    protocol.processTimerData(buildShotFrame(0x02, 0x00, 0x02));
    const events = protocol.processTimerData(Uint8Array.of(0xF8, 0xF9, 0x18, 0x09, 0x00, 0x00, 0xF9, 0xF8));

    expect(events).toEqual([{ type: 'sessionStopped', sessionId: 4, totalShots: 2 }]);
  });

  it('rejects invalid start markers', () => {
    expect(protocol.processTimerData(Uint8Array.of(0xAA, 0xBB, 0x36, 0x00, 0x03, 0x2D, 0x00, 0x00, 0xF9, 0xF8))).toEqual([]);
  });

  it('rejects invalid end markers', () => {
    expect(protocol.processTimerData(Uint8Array.of(0xF8, 0xF9, 0x36, 0x00, 0x03, 0x2D, 0x00, 0x00, 0xAA, 0xBB))).toEqual([]);
  });

  it('rejects a frame that is too short', () => {
    expect(protocol.processTimerData(Uint8Array.of(0xF8, 0xF9, 0x36, 0xF9, 0xF8))).toEqual([]);
  });

  it('rejects an empty notification', () => {
    expect(protocol.processTimerData(new Uint8Array(0))).toEqual([]);
  });

  it('drops a shot frame shorter than 10 bytes', () => {
    expect(protocol.processTimerData(Uint8Array.of(0xF8, 0xF9, 0x36, 0x00, 0x03, 0x2D, 0xF9, 0xF8))).toEqual([]);
  });

  it('ignores unknown message types', () => {
    expect(protocol.processTimerData(Uint8Array.of(0xF8, 0xF9, 0x99, 0x01, 0x00, 0x00, 0xF9, 0xF8))).toEqual([]);
  });
});

describe('ASN Tracker frame protocol', () => {
  let protocol: TimerProtocol;

  beforeEach(() => {
    protocol = ASN_TRACKER.createProtocol(ASN_TRACKER.getDeviceModel());
  });

  it('converts time and keeps the 1-based wire shot number', () => {
    const shot = getShot(protocol.processTimerData(buildShotFrame(0x05, 0x32, 0x02)));

    expect(shot.shotNumber).toBe(2);
    expect(shot.absoluteTimeMs).toBe(5500);
    expect(shot.isFirstShot).toBe(true);
  });

  it('parses session start', () => {
    const events = protocol.processTimerData(Uint8Array.of(0xF8, 0xF9, 0x34, 0x0A, 0x00, 0x00, 0xF9, 0xF8));

    expect(events[0]).toEqual({ type: 'sessionStarted', sessionId: 0x0A, startDelaySeconds: 0 });
  });

  it('parses session stop', () => {
    protocol.processTimerData(START_SESSION_1);
    const events = protocol.processTimerData(STOP_SESSION_1);

    expect(events).toEqual([{ type: 'sessionStopped', sessionId: 1, totalShots: 0 }]);
  });

  it('stamps the ASN Tracker device model', () => {
    expect(getShot(protocol.processTimerData(buildShotFrame(0x01, 0x00, 0x00))).deviceModel).toBe('ASN Tracker');
  });

  it('accumulates splits over several shots', () => {
    const shot1 = getShot(protocol.processTimerData(buildShotFrame(0x01, 0x32, 0x00)));
    const shot2 = getShot(protocol.processTimerData(buildShotFrame(0x02, 0x0A, 0x01)));
    const shot3 = getShot(protocol.processTimerData(buildShotFrame(0x02, 0x4B, 0x02)));

    expect([shot1.absoluteTimeMs, shot1.splitTimeMs]).toEqual([1500, 0]);
    expect([shot2.absoluteTimeMs, shot2.splitTimeMs]).toEqual([2100, 600]);
    expect([shot3.absoluteTimeMs, shot3.splitTimeMs]).toEqual([2750, 650]);
  });
});

describe('Special Pie M1A2F frame protocol', () => {
  let protocol: TimerProtocol;

  beforeEach(() => {
    protocol = SPECIAL_PIE_M1A2F.createProtocol(SPECIAL_PIE_M1A2F.getDeviceModel());
  });

  it('parses the first shot', () => {
    const shot = getShot(protocol.processTimerData(buildShotFrame(0x04, 0x19, 0x01)));

    expect(shot.shotNumber).toBe(1);
    expect(shot.absoluteTimeMs).toBe(4250);
    expect(shot.isFirstShot).toBe(true);
    expect(shot.deviceModel).toBe('SP M1A2 Timer');
  });

  it('calculates the split time', () => {
    expect(getShot(protocol.processTimerData(buildShotFrame(0x02, 0x32, 0x01))).absoluteTimeMs).toBe(2500);
    const shot = getShot(protocol.processTimerData(buildShotFrame(0x03, 0x14, 0x02)));

    expect(shot.absoluteTimeMs).toBe(3200);
    expect(shot.splitTimeMs).toBe(700);
    expect(shot.isFirstShot).toBe(false);
  });

  it('parses session start', () => {
    const events = protocol.processTimerData(Uint8Array.of(0xF8, 0xF9, 0x34, 0x07, 0x00, 0x00, 0xF9, 0xF8));

    expect(events[0]).toEqual({ type: 'sessionStarted', sessionId: 7, startDelaySeconds: 0 });
  });

  it('parses session stop', () => {
    protocol.processTimerData(START_SESSION_1);

    expect(protocol.processTimerData(STOP_SESSION_1).map(event => event.type)).toEqual(['sessionStopped']);
  });

  it('rejects invalid frame markers', () => {
    expect(protocol.processTimerData(Uint8Array.of(0x00, 0x00, 0x36, 0x00, 0x03, 0x2D, 0x00, 0x00, 0x00, 0x00))).toEqual([]);
  });

  it('resets first-shot tracking on a new session', () => {
    protocol.processTimerData(START_SESSION_1);
    expect(getShot(protocol.processTimerData(buildShotFrame(0x02, 0x32, 0x01))).isFirstShot).toBe(true);
    protocol.processTimerData(STOP_SESSION_1);
    protocol.processTimerData(Uint8Array.of(0xF8, 0xF9, 0x34, 0x02, 0x00, 0x00, 0xF9, 0xF8));

    const shot = getShot(protocol.processTimerData(buildShotFrame(0x01, 0x00, 0x01)));

    expect(shot.isFirstShot).toBe(true);
    expect(shot.splitTimeMs).toBe(0);
    expect(shot.sessionId).toBe(2);
  });

  // Raw notifications captured from a physical "SP M1A2 Timer 2003" (fff1).
  // Byte 7 is 0x0B, a non-zero trailing byte the parser must ignore.
  it('replays a real captured shot sequence', () => {
    const frames = [
      { bytes: [0x0C, 0x4A, 0x02], shotNumber: 2, absoluteTimeMs: 12740, splitTimeMs: 0, isFirstShot: true },
      { bytes: [0x0C, 0x4B, 0x03], shotNumber: 3, absoluteTimeMs: 12750, splitTimeMs: 10, isFirstShot: false },
      { bytes: [0x0E, 0x22, 0x04], shotNumber: 4, absoluteTimeMs: 14340, splitTimeMs: 1590, isFirstShot: false },
      { bytes: [0x0E, 0x26, 0x05], shotNumber: 5, absoluteTimeMs: 14380, splitTimeMs: 40, isFirstShot: false },
      { bytes: [0x0F, 0x10, 0x06], shotNumber: 6, absoluteTimeMs: 15160, splitTimeMs: 780, isFirstShot: false },
      { bytes: [0x10, 0x24, 0x07], shotNumber: 7, absoluteTimeMs: 16360, splitTimeMs: 1200, isFirstShot: false },
      { bytes: [0x10, 0x28, 0x08], shotNumber: 8, absoluteTimeMs: 16400, splitTimeMs: 40, isFirstShot: false },
      { bytes: [0x10, 0x33, 0x09], shotNumber: 9, absoluteTimeMs: 16510, splitTimeMs: 110, isFirstShot: false },
      { bytes: [0x11, 0x53, 0x0A], shotNumber: 10, absoluteTimeMs: 17830, splitTimeMs: 1320, isFirstShot: false },
      { bytes: [0x11, 0x55, 0x0B], shotNumber: 11, absoluteTimeMs: 17850, splitTimeMs: 20, isFirstShot: false },
      { bytes: [0x14, 0x0B, 0x0C], shotNumber: 12, absoluteTimeMs: 20110, splitTimeMs: 2260, isFirstShot: false },
      { bytes: [0x14, 0x0C, 0x0D], shotNumber: 13, absoluteTimeMs: 20120, splitTimeMs: 10, isFirstShot: false },
    ];

    for (const frame of frames) {
      const notification = Uint8Array.of(0xF8, 0xF9, 0x36, 0x00, ...frame.bytes, 0x0B, 0xF9, 0xF8);
      const shot = getShot(protocol.processTimerData(notification));

      expect({
        shotNumber: shot.shotNumber,
        absoluteTimeMs: shot.absoluteTimeMs,
        splitTimeMs: shot.splitTimeMs,
        isFirstShot: shot.isFirstShot,
      }).toEqual({
        shotNumber: frame.shotNumber,
        absoluteTimeMs: frame.absoluteTimeMs,
        splitTimeMs: frame.splitTimeMs,
        isFirstShot: frame.isFirstShot,
      });
    }
  });
});
