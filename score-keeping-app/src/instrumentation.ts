export async function register(): Promise<void> {
  // The MQTT subscriber holds a persistent TCP connection and does local-API
  // writes — Node.js runtime only, never Edge.
  if (process.env.NEXT_RUNTIME === 'nodejs') {
    const { startServerMqttSubscriber } = await import('./lib/mqtt/serverSubscriber')
    startServerMqttSubscriber()
  }
}
