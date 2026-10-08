import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { TimerPublisher } from '../src/publisher.js';
import type { IClientPublishOptions } from 'mqtt';
import type { PublishClient } from '../src/publisher.js';

const NOW_MS = 1760000000000;

interface PublishedMessage {
  topic: string;
  payload: string;
  options: IClientPublishOptions;
}

function createFakeClient() {
  const messages: PublishedMessage[] = [];
  const client: PublishClient = {
    publish(topic, payload, options, callback) {
      messages.push({ topic, payload, options });
      callback();
    },
  };

  return { client, messages };
}

describe('TimerPublisher', () => {
  let messages: PublishedMessage[];
  let publisher: TimerPublisher;

  beforeEach(() => {
    vi.useFakeTimers();
    vi.setSystemTime(NOW_MS);
    const fake = createFakeClient();
    messages = fake.messages;
    publisher = new TimerPublisher(fake.client, 'BRG001');
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it('publishes presence as a raw retained string', async () => {
    await publisher.publishPresence(true);
    await publisher.publishPresence(false, 1);

    expect(messages).toEqual([
      { topic: 'timer/BRG001/presence', payload: 'online', options: { qos: 0, retain: true } },
      { topic: 'timer/BRG001/presence', payload: 'offline', options: { qos: 1, retain: true } },
    ]);
  });

  it('publishes retained connection state, omitting the timer while scanning', async () => {
    await publisher.publishConnectionState('SCANNING');
    await publisher.publishConnectionState('CONNECTED', { deviceName: 'SP M1A2 Timer 2003', deviceModel: 'SP M1A2 Timer' });

    expect(messages[0].topic).toBe('timer/BRG001/connection/state');
    expect(messages[0].options.retain).toBe(true);
    expect(JSON.parse(messages[0].payload)).toEqual({ state: 'SCANNING', timestamp: NOW_MS });
    expect(JSON.parse(messages[1].payload)).toEqual({
      state: 'CONNECTED',
      deviceName: 'SP M1A2 Timer 2003',
      deviceModel: 'SP M1A2 Timer',
      timestamp: NOW_MS,
    });
  });

  it('publishes retained device info with the bridge device ID', async () => {
    await publisher.publishDeviceInfo({ deviceName: 'SG-SST4A12345', deviceModel: 'SG Timer Sport' });

    expect(messages[0].topic).toBe('timer/BRG001/device/info');
    expect(messages[0].options.retain).toBe(true);
    expect(JSON.parse(messages[0].payload)).toEqual({
      deviceName: 'SG-SST4A12345',
      deviceModel: 'SG Timer Sport',
      deviceId: 'BRG001',
      timestamp: NOW_MS,
    });
  });

  it('publishes a session with the firmware payloads, not retained', async () => {
    await publisher.publishTimerEvent({ type: 'sessionStarted', sessionId: 7, startDelaySeconds: 3 });
    await publisher.publishTimerEvent({ type: 'countdownComplete', sessionId: 7 });
    await publisher.publishTimerEvent({
      type: 'shotDetected',
      shot: { sessionId: 7, shotNumber: 1, absoluteTimeMs: 1500, splitTimeMs: 0, deviceModel: 'SG Timer', isFirstShot: true },
    });
    await publisher.publishTimerEvent({
      type: 'shotDetected',
      shot: { sessionId: 7, shotNumber: 2, absoluteTimeMs: 2200, splitTimeMs: 700, deviceModel: 'SG Timer', isFirstShot: false },
    });
    await publisher.publishTimerEvent({ type: 'sessionSuspended', sessionId: 7, totalShots: 2 });
    await publisher.publishTimerEvent({ type: 'sessionResumed', sessionId: 7, totalShots: 2 });
    await publisher.publishTimerEvent({ type: 'sessionStopped', sessionId: 7, totalShots: 2 });

    expect(messages.map(message => message.topic)).toEqual([
      'timer/BRG001/session/started',
      'timer/BRG001/countdown/complete',
      'timer/BRG001/shot/detected',
      'timer/BRG001/shot/detected',
      'timer/BRG001/session/suspended',
      'timer/BRG001/session/resumed',
      'timer/BRG001/session/stopped',
    ]);
    expect(messages.every(message => message.options.retain === false)).toBe(true);
    expect(messages.map(message => message.payload)).toEqual([
      `{"sessionId":7,"startDelaySeconds":3,"timestamp":${NOW_MS}}`,
      `{"sessionId":7,"timestamp":${NOW_MS}}`,
      `{"sessionId":7,"shotNumber":1,"absoluteTimeMs":1500,"splitTimeMs":0,"deviceModel":"SG Timer","isFirstShot":true,"timestamp":${NOW_MS}}`,
      `{"sessionId":7,"shotNumber":2,"absoluteTimeMs":2200,"splitTimeMs":700,"deviceModel":"SG Timer","isFirstShot":false,"timestamp":${NOW_MS}}`,
      `{"sessionId":7,"timestamp":${NOW_MS}}`,
      `{"sessionId":7,"timestamp":${NOW_MS}}`,
      `{"sessionId":7,"totalShots":2,"lastShotTimeMs":2200,"timestamp":${NOW_MS}}`,
    ]);
  });

  it('omits lastShotTimeMs when the session had no shots', async () => {
    await publisher.publishTimerEvent({
      type: 'shotDetected',
      shot: { sessionId: 1, shotNumber: 1, absoluteTimeMs: 900, splitTimeMs: 0, deviceModel: 'SG Timer', isFirstShot: true },
    });
    await publisher.publishTimerEvent({ type: 'sessionStarted', sessionId: 2, startDelaySeconds: 0 });
    await publisher.publishTimerEvent({ type: 'sessionStopped', sessionId: 2, totalShots: 0 });

    expect(JSON.parse(messages[2].payload)).toEqual({ sessionId: 2, totalShots: 0, timestamp: NOW_MS });
  });
});
