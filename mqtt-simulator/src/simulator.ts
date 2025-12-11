import mqtt from 'mqtt';
import type { MqttClient } from 'mqtt';
import {
  MqttTopics,
  ConnectionState,
  type ConnectionStateMessage,
  type SessionStartedMessage,
  type ShotDetectedMessage,
  type SessionStoppedMessage,
  type CountdownCompleteMessage,
  type DeviceInfoMessage,
  type SimulatorConfig,
  DEFAULT_CONFIG
} from './types.js';

/**
 * SG Timer MQTT Simulator
 * Simulates a shot timer device publishing events to MQTT broker
 */
export class TimerSimulator {
  private client: MqttClient | null = null;
  private config: SimulatorConfig;
  private sessionId: number = 0;
  private isRunning: boolean = false;

  constructor(config: Partial<SimulatorConfig> = {}) {
    this.config = { ...DEFAULT_CONFIG, ...config };
  }

  /**
   * Connect to MQTT broker
   */
  async connect(): Promise<void> {
    return new Promise((resolve, reject) => {
      console.log(`🔌 Connecting to MQTT broker: ${this.config.brokerUrl}`);

      this.client = mqtt.connect(this.config.brokerUrl, {
        clientId: `timer-simulator-${Math.random().toString(16).substring(2, 8)}`,
        clean: true,
        reconnectPeriod: this.config.autoReconnect ? 5000 : 0
      });

      this.client.on('connect', () => {
        console.log('✅ Connected to MQTT broker');
        resolve();
      });

      this.client.on('error', (error) => {
        console.error('❌ MQTT error:', error.message);
        reject(error);
      });

      this.client.on('close', () => {
        console.log('🔌 Connection closed');
      });

      this.client.on('reconnect', () => {
        console.log('🔄 Reconnecting...');
      });
    });
  }

  /**
   * Disconnect from MQTT broker
   */
  async disconnect(): Promise<void> {
    if (this.client) {
      console.log('👋 Disconnecting from broker...');
      await this.publishConnectionState(ConnectionState.DISCONNECTED);
      this.client.end();
      this.client = null;
    }
  }

  /**
   * Publish a message to a topic
   */
  private publish(topic: string, message: object): void {
    if (!this.client) {
      throw new Error('Not connected to MQTT broker');
    }

    const payload = JSON.stringify(message);
    this.client.publish(topic, payload, { qos: 0 }, (error) => {
      if (error) {
        console.error(`❌ Failed to publish to ${topic}:`, error.message);
      }
    });
  }

  /**
   * Publish connection state
   */
  publishConnectionState(state: ConnectionState, deviceName?: string): void {
    const message: ConnectionStateMessage = {
      state,
      deviceName: deviceName || this.config.deviceName,
      timestamp: Date.now()
    };

    console.log(`📡 Connection state: ${state}${deviceName ? ` (${deviceName})` : ''}`);
    this.publish(MqttTopics.CONNECTION_STATE, message);
  }

  /**
   * Publish device info
   */
  publishDeviceInfo(): void {
    const message: DeviceInfoMessage = {
      deviceModel: this.config.deviceModel,
      deviceName: this.config.deviceName,
      timestamp: Date.now()
    };

    console.log(`ℹ️  Device: ${this.config.deviceName} (${this.config.deviceModel})`);
    this.publish(MqttTopics.DEVICE_INFO, message);
  }

  /**
   * Publish session started
   */
  publishSessionStarted(sessionId: number, startDelaySeconds: number): void {
    const message: SessionStartedMessage = {
      sessionId,
      startDelaySeconds,
      timestamp: Date.now()
    };

    console.log(`🎬 Session started: ID=${sessionId}, Delay=${startDelaySeconds}s`);
    this.publish(MqttTopics.SESSION_STARTED, message);
  }

  /**
   * Publish countdown complete
   */
  publishCountdownComplete(sessionId: number): void {
    const message: CountdownCompleteMessage = {
      sessionId,
      timestamp: Date.now()
    };

    console.log(`⏱️  Countdown complete for session ${sessionId}`);
    this.publish(MqttTopics.COUNTDOWN_COMPLETE, message);
  }

  /**
   * Publish shot detected
   */
  publishShot(
    sessionId: number,
    shotNumber: number,
    absoluteTimeMs: number,
    splitTimeMs: number,
    isFirstShot: boolean
  ): void {
    const message: ShotDetectedMessage = {
      sessionId,
      shotNumber,
      absoluteTimeMs,
      splitTimeMs,
      deviceModel: this.config.deviceModel,
      isFirstShot,
      timestamp: Date.now()
    };

    const timeSeconds = (absoluteTimeMs / 1000).toFixed(3);
    const splitSeconds = (splitTimeMs / 1000).toFixed(3);
    console.log(
      `🎯 Shot #${shotNumber}: ${timeSeconds}s (split: ${splitSeconds}s)`
    );
    this.publish(MqttTopics.SHOT_DETECTED, message);
  }

  /**
   * Publish session stopped
   */
  publishSessionStopped(sessionId: number, totalShots: number): void {
    const message: SessionStoppedMessage = {
      sessionId,
      totalShots,
      timestamp: Date.now()
    };

    console.log(`🏁 Session stopped: ID=${sessionId}, Total shots=${totalShots}`);
    this.publish(MqttTopics.SESSION_STOPPED, message);
  }

  /**
   * Simulate device connection sequence
   */
  async simulateConnection(): Promise<void> {
    console.log('\n📱 Simulating device connection...\n');

    // Scanning
    this.publishConnectionState(ConnectionState.SCANNING);
    await this.sleep(2000);

    // Connecting
    this.publishConnectionState(ConnectionState.CONNECTING);
    await this.sleep(1500);

    // Connected
    this.publishConnectionState(ConnectionState.CONNECTED, this.config.deviceName);
    await this.sleep(500);

    // Send device info
    this.publishDeviceInfo();
  }

  /**
   * Simulate a complete shooting session
   */
  async simulateSession(): Promise<void> {
    if (this.isRunning) {
      console.log('⚠️  Session already running');
      return;
    }

    this.isRunning = true;
    this.sessionId++;

    console.log('\n🎯 Starting shooting session simulation...\n');

    // Start session with countdown
    this.publishSessionStarted(this.sessionId, this.config.startDelay);

    // Countdown delay
    console.log(`⏳ Countdown: ${this.config.startDelay} seconds...`);
    await this.sleep(this.config.startDelay * 1000);

    // Countdown complete
    this.publishCountdownComplete(this.sessionId);
    await this.sleep(500);

    console.log(`\n🔫 Firing ${this.config.shotCount} shots...\n`);

    // Simulate shots
    let absoluteTime = 0;
    for (let i = 1; i <= this.config.shotCount; i++) {
      // Add some random variation to shot timing
      const variation = Math.random() * this.config.splitTimeVariation -
                        this.config.splitTimeVariation / 2;
      const splitTime = i === 1 ? 0 : this.config.shotInterval + variation;
      absoluteTime += splitTime;

      this.publishShot(
        this.sessionId,
        i,
        Math.round(absoluteTime),
        Math.round(splitTime),
        i === 1
      );

      // Wait before next shot
      if (i < this.config.shotCount) {
        await this.sleep(this.config.shotInterval);
      }
    }

    // End session
    await this.sleep(1000);
    this.publishSessionStopped(this.sessionId, this.config.shotCount);

    this.isRunning = false;
    console.log('\n✅ Session simulation complete\n');
  }

  /**
   * Simulate a realistic competitive shooting scenario
   */
  async simulateCompetitiveStage(): Promise<void> {
    if (this.isRunning) {
      console.log('⚠️  Session already running');
      return;
    }

    this.isRunning = true;
    this.sessionId++;

    console.log('\n🏆 Simulating competitive USPSA/IPSC stage...\n');

    // Typical stage: 12-15 rounds
    const rounds = 12 + Math.floor(Math.random() * 4); // 12-15 rounds
    const startDelay = 3.0; // Random start delay

    this.publishSessionStarted(this.sessionId, startDelay);
    console.log(`⏳ Countdown: ${startDelay} seconds...`);
    await this.sleep(startDelay * 1000);
    this.publishCountdownComplete(this.sessionId);

    console.log(`\n🎯 Competitor firing ${rounds} rounds...\n`);

    let absoluteTime = 0;

    // First shot (draw and first shot)
    const drawTime = 1200 + Math.random() * 400; // 1.2-1.6s draw
    absoluteTime += drawTime;
    this.publishShot(this.sessionId, 1, Math.round(absoluteTime), 0, true);
    await this.sleep(drawTime);

    // Subsequent shots with realistic cadence
    for (let i = 2; i <= rounds; i++) {
      // Realistic split times: 0.2-0.35s for close targets, longer for transitions
      let splitTime: number;

      if (i % 6 === 0) {
        // Magazine reload
        splitTime = 1800 + Math.random() * 400; // 1.8-2.2s reload
        console.log('   🔄 Magazine reload...');
      } else if (i % 4 === 1) {
        // Target transition
        splitTime = 500 + Math.random() * 300; // 0.5-0.8s transition
      } else {
        // Normal cadence
        splitTime = 200 + Math.random() * 150; // 0.2-0.35s split
      }

      absoluteTime += splitTime;
      this.publishShot(
        this.sessionId,
        i,
        Math.round(absoluteTime),
        Math.round(splitTime),
        false
      );
      await this.sleep(splitTime);
    }

    // End session
    await this.sleep(1000);
    this.publishSessionStopped(this.sessionId, rounds);

    this.isRunning = false;
    const totalTime = (absoluteTime / 1000).toFixed(2);
    console.log(`\n✅ Stage complete: ${rounds} rounds in ${totalTime}s\n`);
  }

  /**
   * Run full simulation scenario
   */
  async run(): Promise<void> {
    try {
      await this.connect();
      await this.sleep(500);
      await this.simulateConnection();
      await this.sleep(2000);
      await this.simulateSession();
      await this.sleep(2000);
    } catch (error) {
      console.error('❌ Simulation error:', error);
      throw error;
    }
  }

  /**
   * Helper: sleep for ms
   */
  private sleep(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  /**
   * Check if simulator is running
   */
  isSessionRunning(): boolean {
    return this.isRunning;
  }
}
