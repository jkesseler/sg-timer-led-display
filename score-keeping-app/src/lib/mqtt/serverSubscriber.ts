import mqtt, { type MqttClient } from 'mqtt';
import { getPayload, type Payload } from 'payload';
import config from '@/payload.config';
import { parseDeviceTopic } from './constants';
import type { SessionStartedMessage, SessionStoppedMessage } from './types';

// The server owns (session) → (shooter, discipline, round) binding exclusively.
// This subscriber is the only writer of round-results/match-sessions state —
// the browser's MQTT feed (mqttMiddleware, ported in a later phase) is
// read-only for display and never persists.

let client: MqttClient | null = null;
let payloadPromise: Promise<Payload> | null = null;

function getServerPayload(): Promise<Payload> {
  payloadPromise ??= getPayload({ config });

  return payloadPromise;
}

function getBrokerUrl(): string {
  return process.env.MQTT_BROKER_URL || 'mqtt://127.0.0.1:1883';
}

/**
 * Starts the singleton server-side MQTT subscriber. Safe to call more than
 * once — only the first call has an effect. Call once from instrumentation.ts
 * so the subscription survives the lifetime of the server process; this
 * requires a long-running Node server, not serverless/edge deployment.
 */
export function startServerMqttSubscriber(): void {
  if (client) {
    return;
  }

  const options: mqtt.IClientOptions = {
    clientId: `score-keeping-server-${Math.random().toString(16).slice(2, 10)}`,
    clean: true,
    reconnectPeriod: 5000,
    connectTimeout: 30000
  };

  if (process.env.MQTT_USERNAME) {
    options.username = process.env.MQTT_USERNAME;
    options.password = process.env.MQTT_PASSWORD;
  }

  const brokerUrl = getBrokerUrl();
  console.log(`[mqtt-server] connecting to ${brokerUrl}`);
  client = mqtt.connect(brokerUrl, options);

  client.on('connect', () => {
    console.log('[mqtt-server] connected');
    client?.subscribe('timer/+/session/started', { qos: 1 }, (error) => {
      if (error) {
        console.error('[mqtt-server] failed to subscribe to session/started', error);
      }
    });
    client?.subscribe('timer/+/session/stopped', { qos: 1 }, (error) => {
      if (error) {
        console.error('[mqtt-server] failed to subscribe to session/stopped', error);
      }
    });
  });

  client.on('error', (error) => {
    console.error('[mqtt-server] connection error', error);
  });

  client.on('message', (topic, payload) => {
    handleMessage(topic, payload).catch((error) => {
      console.error(`[mqtt-server] failed to handle message on ${topic}`, error);
    });
  });
}

async function handleMessage(topic: string, payload: Buffer): Promise<void> {
  const parsed = parseDeviceTopic(topic);
  if (!parsed) {
    return;
  }

  const { deviceId, event } = parsed;

  if (event === 'session/started') {
    await handleSessionStarted(deviceId, JSON.parse(payload.toString()) as SessionStartedMessage);

    return;
  }

  if (event === 'session/stopped') {
    await handleSessionStopped(deviceId, JSON.parse(payload.toString()) as SessionStoppedMessage);
  }
}

async function handleSessionStarted(deviceId: string, message: SessionStartedMessage): Promise<void> {
  const payload = await getServerPayload();

  const devices = await payload.find({
    collection: 'devices',
    where: { deviceId: { equals: deviceId } },
    limit: 1
  });
  const device = devices.docs[0];
  if (!device) {
    console.warn(`[mqtt-server] session/started from unregistered device ${deviceId}`);

    return;
  }

  const pendingSessions = await payload.find({
    collection: 'match-sessions',
    where: {
      and: [{ device: { equals: device.id } }, { status: { equals: 'pending' } }]
    },
    limit: 1
  });
  const pendingSession = pendingSessions.docs[0];
  if (!pendingSession) {
    // Nothing to bind — no shooter had scanned in before the range officer
    // pressed Start. Surfaced to the browser via its own live MQTT feed as
    // an "unbound timer activity" indicator; not silently lost, but there is
    // no (shooter, discipline, round) to attach a record to.
    console.warn(`[mqtt-server] session/started on ${deviceId} with no pending activation`);

    return;
  }

  await payload.update({
    collection: 'match-sessions',
    id: pendingSession.id,
    data: {
      status: 'active',
      timerSessionId: message.sessionId,
      startedAtMs: message.timestamp
    }
  });
}

async function handleSessionStopped(deviceId: string, message: SessionStoppedMessage): Promise<void> {
  const payload = await getServerPayload();

  const devices = await payload.find({
    collection: 'devices',
    where: { deviceId: { equals: deviceId } },
    limit: 1
  });
  const device = devices.docs[0];
  if (!device) {
    console.warn(`[mqtt-server] session/stopped from unregistered device ${deviceId}`);

    return;
  }

  const activeSessions = await payload.find({
    collection: 'match-sessions',
    where: {
      and: [
        { device: { equals: device.id } },
        { status: { equals: 'active' } },
        { timerSessionId: { equals: message.sessionId } }
      ]
    },
    limit: 1
  });
  const activeSession = activeSessions.docs[0];
  if (!activeSession) {
    console.warn(`[mqtt-server] session/stopped on ${deviceId} (session ${message.sessionId}) with no matching active session`);

    return;
  }

  if (!message.lastShotTimeMs) {
    // Turn ended with no shots — leave the round-result pending so the same
    // shooter can re-scan and try again.
    await payload.update({
      collection: 'match-sessions',
      id: activeSession.id,
      data: { status: 'abandoned', stoppedAtMs: message.timestamp }
    });

    return;
  }

  const roundResultId = relationshipId(activeSession.roundResult);
  const reshootForId = relationshipId(activeSession.reshootFor);

  if (roundResultId == null && reshootForId == null) {
    console.error(`[mqtt-server] active match-session ${activeSession.id} has no bound target`);

    return;
  }

  const transactionID = (await payload.db.beginTransaction()) ?? undefined;
  try {
    if (roundResultId != null) {
      await payload.update({
        collection: 'round-results',
        id: roundResultId,
        data: { status: 'timed', timeMs: message.lastShotTimeMs, stoppedAtMs: message.timestamp },
        req: { transactionID }
      });
    } else if (reshootForId != null) {
      // Deferred reshoot: the time goes onto the membership's dedicated
      // field, retryable before sign-off — never back into the RS round.
      await payload.update({
        collection: 'squad-memberships',
        id: reshootForId,
        data: { reshootTimeMs: message.lastShotTimeMs },
        req: { transactionID }
      });
    }

    await payload.update({
      collection: 'match-sessions',
      id: activeSession.id,
      data: { status: 'completed', stoppedAtMs: message.timestamp },
      req: { transactionID }
    });

    if (transactionID != null) {
      await payload.db.commitTransaction(transactionID);
    }
  } catch (error) {
    if (transactionID != null) {
      await payload.db.rollbackTransaction(transactionID);
    }
    throw error;
  }
}

function relationshipId(value: unknown): number | string | undefined {
  if (value == null) {
    return undefined;
  }
  if (typeof value === 'object') {
    return (value as { id: number | string }).id;
  }

  return value as number | string;
}
