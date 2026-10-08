import { describe, it, expect } from 'vitest';
import { formatTimeValue } from '@/lib/display/utils';
import { mqttSlice, shotDetected } from '@/store/mqttSlice';
import type { ShotDetectedMessage } from '@/lib/mqtt/types';
import type { UnknownAction } from '@reduxjs/toolkit';

function buildShot(shotNumber: number, absoluteTimeMs: number): ShotDetectedMessage {
  return {
    sessionId: 1,
    shotNumber,
    absoluteTimeMs,
    splitTimeMs: 500,
    deviceModel: 'SG Timer',
    isFirstShot: shotNumber === 1,
    timestamp: 0
  };
}

describe('formatTimeValue', () => {
  it('formats as seconds with hundredths', () => {
    expect(formatTimeValue(2340)).toBe('2.34');
    expect(formatTimeValue(0)).toBe('0.00');
    expect(formatTimeValue(1005)).toBe('1.01');
  });

  it('keeps a minute and up in seconds', () => {
    expect(formatTimeValue(70000)).toBe('70.00');
    expect(formatTimeValue(59996)).toBe('60.00');
    expect(formatTimeValue(125050)).toBe('125.05');
  });
});

describe('mqttSlice shotDetected', () => {
  const initialState = mqttSlice.reducer(undefined, { type: '@@init' } as UnknownAction);

  it('replaces a redelivered shot instead of appending a duplicate', () => {
    let state = mqttSlice.reducer(initialState, shotDetected(buildShot(1, 1000)));
    state = mqttSlice.reducer(state, shotDetected(buildShot(2, 1500)));
    state = mqttSlice.reducer(state, shotDetected(buildShot(2, 1520)));

    expect(state.shots.map(shot => shot.shotNumber)).toEqual([1, 2]);
    expect(state.shots[1].absoluteTimeMs).toBe(1520);
  });
});
