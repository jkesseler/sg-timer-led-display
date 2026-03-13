#!/usr/bin/env node
import mqtt from 'mqtt';

/**
 * Simple test to verify MQTT broker connectivity
 */
async function testConnection() {
  const brokerUrl = process.argv[2] || 'ws://localhost:9001';

  console.log(`Testing connection to: ${brokerUrl}\n`);

  const client = mqtt.connect(brokerUrl, {
    clientId: `test-${Math.random().toString(16).substring(2, 8)}`
  });

  client.on('connect', () => {
    console.log('✅ Successfully connected to MQTT broker!');
    console.log('   Client ID:', client.options.clientId);

    // Test publish/subscribe
    const testTopic = 'test/connection';
    console.log(`\n📡 Subscribing to: ${testTopic}`);

    client.subscribe(testTopic, (err) => {
      if (err) {
        console.error('❌ Subscribe failed:', err);
        process.exit(1);
      }

      console.log('✅ Successfully subscribed');
      console.log('\n📤 Publishing test message...');

      client.publish(testTopic, JSON.stringify({
        message: 'Hello from MQTT test',
        timestamp: Date.now()
      }));
    });
  });

  client.on('message', (topic, payload) => {
    console.log(`\n📥 Received message on ${topic}:`);
    console.log('   ', payload.toString());
    console.log('\n✅ MQTT broker is working correctly!\n');

    client.end();
    process.exit(0);
  });

  client.on('error', (error) => {
    console.error('❌ Connection failed:', error.message);
    console.log('\nTroubleshooting:');
    console.log('  1. Check if Mosquitto is running');
    console.log('  2. Verify WebSocket listener on port 9001');
    console.log('  3. Check mosquitto.conf has: listener 9001 + protocol websockets');
    process.exit(1);
  });

  // Timeout after 10 seconds
  setTimeout(() => {
    console.error('\n❌ Connection timeout (10s)');
    console.log('   Broker may not be running or not accepting WebSocket connections');
    process.exit(1);
  }, 10000);
}

testConnection();
