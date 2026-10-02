import { beforeEach, describe, expect, it } from 'vitest';
import { SgTimerProtocol } from '../src/protocols/sgTimerProtocol.js';
import type { NormalizedShot, TimerEvent } from '../src/protocols/timerEvent.js';

function getShot(events: TimerEvent[]): NormalizedShot {
  expect(events).toHaveLength(1);
  const [event] = events;

  if (event.type !== 'shotDetected') {
    throw new Error(`Expected shotDetected, got ${event.type}`);
  }

  return event.shot;
}

describe('SgTimerProtocol', () => {
  let protocol: SgTimerProtocol;

  beforeEach(() => {
    protocol = new SgTimerProtocol('SG Timer');
  });

  describe('SESSION_STARTED', () => {
    it('parses session ID and start delay', () => {
      const events = protocol.processTimerData(Uint8Array.of(0x07, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x1E));

      expect(events).toEqual([{ type: 'sessionStarted', sessionId: 1, startDelaySeconds: 3 }]);
    });

    it('parses a zero start delay', () => {
      const events = protocol.processTimerData(Uint8Array.of(0x07, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00));

      expect(events).toEqual([{ type: 'sessionStarted', sessionId: 2, startDelaySeconds: 0 }]);
    });

    it('parses a session ID with the high bit set as unsigned', () => {
      const events = protocol.processTimerData(Uint8Array.of(0x07, 0x00, 0xFF, 0x00, 0x00, 0x01, 0x00, 0x00));

      expect(events).toEqual([{ type: 'sessionStarted', sessionId: 0xFF000001, startDelaySeconds: 0 }]);
    });
  });

  describe('SHOT_DETECTED', () => {
    it('normalizes the first shot to 1-based with no split', () => {
      const shot = getShot(protocol.processTimerData(Uint8Array.of(
        0x0B, 0x04,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00,
        0x00, 0x00, 0x05, 0xDC,
      )));

      expect(shot).toEqual({
        sessionId: 1,
        shotNumber: 1,
        absoluteTimeMs: 1500,
        splitTimeMs: 0,
        deviceModel: 'SG Timer',
        isFirstShot: true,
      });
    });

    it('calculates the split time from the previous shot', () => {
      protocol.processTimerData(Uint8Array.of(0x0B, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x05, 0xDC));
      const shot = getShot(protocol.processTimerData(Uint8Array.of(
        0x0B, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x08, 0x98,
      )));

      expect(shot.shotNumber).toBe(2);
      expect(shot.absoluteTimeMs).toBe(2200);
      expect(shot.splitTimeMs).toBe(700);
      expect(shot.isFirstShot).toBe(false);
    });

    it('parses multi-byte times', () => {
      const shot = getShot(protocol.processTimerData(Uint8Array.of(
        0x0B, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x05, 0x00, 0x00, 0xFD, 0xE8,
      )));

      expect(shot.shotNumber).toBe(6);
      expect(shot.absoluteTimeMs).toBe(65000);
    });

    it('stamps the configured device model', () => {
      const sportProtocol = new SgTimerProtocol('SG Timer Sport');
      const shot = getShot(sportProtocol.processTimerData(Uint8Array.of(
        0x0B, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x03, 0xE8,
      )));

      expect(shot.deviceModel).toBe('SG Timer Sport');
    });

    it('treats the first shot after a stop as a new first shot', () => {
      protocol.processTimerData(Uint8Array.of(0x0B, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x05, 0xDC));
      protocol.processTimerData(Uint8Array.of(0x07, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01));
      const shot = getShot(protocol.processTimerData(Uint8Array.of(
        0x0B, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x03, 0xE8,
      )));

      expect(shot.isFirstShot).toBe(true);
      expect(shot.splitTimeMs).toBe(0);
    });
  });

  describe('SESSION_STOPPED', () => {
    it('reports total shots from the packet', () => {
      const events = protocol.processTimerData(Uint8Array.of(0x07, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x05));

      expect(events).toEqual([{ type: 'sessionStopped', sessionId: 0, totalShots: 5 }]);
    });

    it('reports the session ID remembered from SESSION_STARTED', () => {
      protocol.processTimerData(Uint8Array.of(0x07, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00));
      const events = protocol.processTimerData(Uint8Array.of(0x07, 0x03, 0x00, 0x00, 0x00, 0x09, 0x00, 0x02));

      expect(events).toEqual([{ type: 'sessionStopped', sessionId: 9, totalShots: 2 }]);
    });
  });

  describe('SESSION_SUSPENDED / SESSION_RESUMED', () => {
    it('emits suspended and resumed with total shots', () => {
      protocol.processTimerData(Uint8Array.of(0x07, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00));

      expect(protocol.processTimerData(Uint8Array.of(0x07, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x04)))
        .toEqual([{ type: 'sessionSuspended', sessionId: 3, totalShots: 4 }]);
      expect(protocol.processTimerData(Uint8Array.of(0x07, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x04)))
        .toEqual([{ type: 'sessionResumed', sessionId: 3, totalShots: 4 }]);
    });
  });

  describe('SESSION_SET_BEGIN', () => {
    it('emits countdown complete', () => {
      protocol.processTimerData(Uint8Array.of(0x07, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x1E));
      const events = protocol.processTimerData(Uint8Array.of(0x05, 0x05, 0x00, 0x00, 0x00, 0x01));

      expect(events).toEqual([{ type: 'countdownComplete', sessionId: 1 }]);
    });
  });

  describe('validation', () => {
    it('rejects an empty packet', () => {
      expect(protocol.processTimerData(new Uint8Array(0))).toEqual([]);
    });

    it('rejects a packet that is too short', () => {
      expect(protocol.processTimerData(Uint8Array.of(0x01))).toEqual([]);
    });

    it('rejects a length field mismatch', () => {
      expect(protocol.processTimerData(Uint8Array.of(0x05, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00))).toEqual([]);
    });

    it('rejects a shot packet that is too short for a shot', () => {
      expect(protocol.processTimerData(Uint8Array.of(0x07, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00))).toEqual([]);
    });

    it('ignores unknown event IDs', () => {
      expect(protocol.processTimerData(Uint8Array.of(0x03, 0x7F, 0x00, 0x00))).toEqual([]);
    });

    it('parses a Buffer that is a view into a larger pool', () => {
      const pool = Buffer.alloc(64);
      pool.set([0x07, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0A], 20);

      expect(protocol.processTimerData(pool.subarray(20, 28)))
        .toEqual([{ type: 'sessionStarted', sessionId: 4, startDelaySeconds: 1 }]);
    });
  });

  // Notifications captured from an SG Timer GO (SG-SST4B11880) on 75200001: one 5-shot string.
  describe('captured SG Timer GO session', () => {
    const CAPTURED_PACKETS = [
      '07006abf9cd9001e',
      '05056abf9cd9',
      '0b046abf9cd90000000005a0',
      '0b046abf9cd9000100000a28',
      '0b046abf9cd9000200000e2e',
      '0b046abf9cd9000300001220',
      '0b046abf9cd9000400001612',
      '07036abf9cd90005',
    ];

    it('decodes the whole string', () => {
      const events = CAPTURED_PACKETS.flatMap(hex => protocol.processTimerData(Buffer.from(hex, 'hex')));
      const sessionId = 0x6ABF9CD9;

      expect(events[0]).toEqual({ type: 'sessionStarted', sessionId, startDelaySeconds: 3 });
      expect(events[1]).toEqual({ type: 'countdownComplete', sessionId });

      const shots = events.slice(2, 7).map(event => (event.type === 'shotDetected' ? event.shot : undefined));
      expect(shots.map(shot => shot?.shotNumber)).toEqual([1, 2, 3, 4, 5]);
      expect(shots.map(shot => shot?.absoluteTimeMs)).toEqual([1440, 2600, 3630, 4640, 5650]);
      expect(shots.map(shot => shot?.splitTimeMs)).toEqual([0, 1160, 1030, 1010, 1010]);

      expect(events[7]).toEqual({ type: 'sessionStopped', sessionId, totalShots: 5 });
      expect(events).toHaveLength(8);
    });
  });
});
