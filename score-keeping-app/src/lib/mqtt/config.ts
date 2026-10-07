import { connection } from 'next/server';
import type { MqttServerConfig } from './types';

/** Read per request, not at build time, so one build works against any broker. */
export async function getMqttConfig(): Promise<MqttServerConfig> {
  await connection();

  return {
    wsUrl: process.env.MQTT_WS_URL || null,
    username: process.env.MQTT_USERNAME || '',
    password: process.env.MQTT_PASSWORD || ''
  };
}
