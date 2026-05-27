#include "BridgeApplication.h"
#include "SGTimer.h"
#include "SpecialPieM1A2Plus.h"
#include "SpecialPieM1A2F.h"
#include "ASNTracker.h"
#include "DeviceId.h"
#include "common.h"
#include <BLEDevice.h>

namespace {
BridgeApplication* gBridgeInstance = nullptr;
volatile bool gBleScanResultsReady = false;

void onBleScanComplete(BLEScanResults /*results*/) {
  gBleScanResultsReady = true;
}
}  // namespace

BridgeApplication::BridgeApplication()
  : role(BridgeRole::TRANSMITTER),
    outputMode(ReceiverOutputMode::MQTT_OUTPUT) {
  gBridgeInstance = this;
}

BridgeApplication::~BridgeApplication() {
  if (gBridgeInstance == this) gBridgeInstance = nullptr;
}

bool BridgeApplication::initialize() {
  Serial.begin(SERIAL_BAUD_RATE);
  Logger::setLevel(LogLevel::INFO);

  LOG_SYSTEM("=== J.K. PewPew Long Range Bridge ===");
  LOG_SYSTEM("LilyGo LoRa32 T3 v1.6.1 Starting...");

  // Initialize device ID (NVS-backed unique identifier)
  deviceId.initialize();
  LOG_SYSTEM("Device ID: %s", deviceId.get().c_str());

  // Initialize OLED display first for visual feedback
  if (!oled.initialize()) {
    LOG_ERROR("SYSTEM", "OLED init failed (continuing without display)");
  }
  oled.showStartupMessage(STARTUP_TEXT);

  // Initialize WiFi configuration portal (non-blocking)
  BridgeWiFiConfig::initialize();

  // Read role from NVS configuration
  role = BridgeWiFiConfig::getDeviceRole();
  outputMode = BridgeWiFiConfig::getOutputMode();

  LOG_SYSTEM("Role: %s", role == BridgeRole::TRANSMITTER ? "TRANSMITTER" : "RECEIVER");
  if (role == BridgeRole::RECEIVER) {
    LOG_SYSTEM("Output mode: %s",
               outputMode == ReceiverOutputMode::MQTT_OUTPUT ? "MQTT" : "BLE Special Pie");
  }

  if (role == BridgeRole::TRANSMITTER) {
    initTransmitter();
  } else {
    initReceiver();
  }

  startupTime = millis();
  lastActivityTime = millis();

  // Force initial OLED draw with role info before the main loop starts
  bridgeStatus.role = role;
  bridgeStatus.outputMode = outputMode;
  oled.update(bridgeStatus);

  LOG_SYSTEM("Bridge initialized successfully");
  return true;
}

// ═════════════════════════════════════════════════════════════
// Transmitter initialization (BLE → LoRa)
// ═════════════════════════════════════════════════════════════

void BridgeApplication::initTransmitter() {
  // Initialize BLE as central (client) for connecting to timers
  char bleName[32];
  snprintf(bleName, sizeof(bleName), "%s-%s", BLE_DEVICE_NAME, deviceId.get().c_str());
  BLEDevice::init(bleName);
  LOG_BLE("BLE Client initialized as: %s", bleName);

  // Initialize LoRa transmitter
  if (!loraTx.initialize()) {
    LOG_ERROR("SYSTEM", "LoRa TX init failed");
  }

  LOG_SYSTEM("Transmitter ready — scanning for all timer types (auto)");
}

// ═════════════════════════════════════════════════════════════
// Receiver initialization (LoRa → MQTT or LoRa → BLE)
// ═════════════════════════════════════════════════════════════

void BridgeApplication::initReceiver() {
  // Initialize LoRa receiver
  if (!loraRx.initialize()) {
    LOG_ERROR("SYSTEM", "LoRa RX init failed");
  }
  setupLoRaCallbacks();

  if (outputMode == ReceiverOutputMode::MQTT_OUTPUT) {
    // MQTT output — needs WiFi
    mqttManager = std::unique_ptr<MqttManager>(new MqttManager());
    if (!mqttManager->initialize()) {
      LOG_SYSTEM("MQTT disabled — server not configured");
    }
    LOG_SYSTEM("Receiver ready (MQTT output)");
  } else {
    // BLE Special Pie output — no WiFi needed for BLE
    char bleName[32];
    snprintf(bleName, sizeof(bleName), "Special Pie M1A2+-%s", deviceId.get().c_str());
    BLEDevice::init(bleName);
    if (!bleServer.initialize()) {
      LOG_ERROR("SYSTEM", "BLE server init failed");
    }
    LOG_SYSTEM("Receiver ready (BLE Special Pie output)");
  }
}

// ═════════════════════════════════════════════════════════════
// Main loop
// ═════════════════════════════════════════════════════════════

void BridgeApplication::run() {
  // Phase 1: WiFi management
  BridgeWiFiConfig::update();

  // Phase 2: Role-specific processing
  if (role == BridgeRole::TRANSMITTER) {
    runTransmitter();
  } else {
    runReceiver();
  }

  // Phase 3: OLED update
  updateOledStatus();
  oled.update(bridgeStatus);

  // Yield to FreeRTOS
  vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY));
}

// ═════════════════════════════════════════════════════════════
// Transmitter loop
// ═════════════════════════════════════════════════════════════

void BridgeApplication::runTransmitter() {
  // Check for BLE scan completion
  if (gBleScanResultsReady) {
    gBleScanResultsReady = false;
    scanResultsReady = true;
  }

  if (isScanning && scanResultsReady) {
    processScanResults();
  }

  // Start scanning if no device connected
  if (!timerDevice) {
    scanForDevices();
  }

  // Process BLE events
  if (timerDevice) {
    timerDevice->update();

    // Deferred cleanup: if device disconnected during update(), reset for next scan
    if (timerDevice->getConnectionState() == DeviceConnectionState::DISCONNECTED) {
      LOG_BLE("Device disconnected — releasing for rescan");
      timerDevice.reset();
    }
  }

  // LoRa TX heartbeat
  loraTx.update();
}

void BridgeApplication::scanForDevices() {
  unsigned long now = millis();
  if (now - startupTime < STARTUP_MESSAGE_DELAY) return;
  if (isScanning || (now - lastScanAttempt < BLE_SCAN_RETRY_INTERVAL_MS)) return;

  lastScanAttempt = now;
  isScanning = true;
  scanResultsReady = false;

  LOG_SYSTEM("Scanning for BLE timer devices...");

  BLEScan* pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(BLE_SCAN_INTERVAL);
  pScan->setWindow(BLE_SCAN_WINDOW);

  if (!pScan->start(BLE_SCAN_DURATION, onBleScanComplete, false)) {
    LOG_ERROR("BLE", "Scan start failed");
    isScanning = false;
  }
}

void BridgeApplication::processScanResults() {
  scanResultsReady = false;

  BLEScan* pScan = BLEDevice::getScan();
  BLEScanResults found = pScan->getResults();
  LOG_SYSTEM("Scan complete — %d devices found", found.getCount());

  bool deviceFound = false;

  for (int i = 0; i < found.getCount(); i++) {
    BLEAdvertisedDevice device = found.getDevice(i);

    if (SpecialPieM1A2F::matchesDevice(&device)) {
      LOG_SYSTEM("Found Special Pie Timer (MAC: %s)", device.getAddress().toString().c_str());
      SpecialPieM1A2F* dev = new SpecialPieM1A2F();
      timerDevice = std::unique_ptr<ITimerDevice>(dev);
      setupBleCallbacks();
      if (timerDevice->initialize() && dev->attemptConnection(&device)) {
        deviceFound = true;
        break;
      }
      timerDevice.reset();
    }
    else if (SGTimer::matchesDevice(&device)) {
      LOG_SYSTEM("Found SG Timer");
      SGTimer* dev = new SGTimer();
      timerDevice = std::unique_ptr<ITimerDevice>(dev);
      setupBleCallbacks();
      if (timerDevice->initialize() && dev->attemptConnection(&device)) {
        deviceFound = true;
        break;
      }
      timerDevice.reset();
    }
    else if (SpecialPieM1A2Plus::matchesDevice(&device)) {
      LOG_SYSTEM("Found Special Pie Timer (UUID)");
      SpecialPieM1A2Plus* dev = new SpecialPieM1A2Plus();
      timerDevice = std::unique_ptr<ITimerDevice>(dev);
      setupBleCallbacks();
      if (timerDevice->initialize() && dev->attemptConnection(&device)) {
        deviceFound = true;
        break;
      }
      timerDevice.reset();
    }
    else if (ASNTracker::matchesDevice(&device)) {
      LOG_SYSTEM("Found ASN Tracker");
      ASNTracker* dev = new ASNTracker();
      timerDevice = std::unique_ptr<ITimerDevice>(dev);
      setupBleCallbacks();
      if (timerDevice->initialize() && dev->attemptConnection(&device)) {
        deviceFound = true;
        break;
      }
      timerDevice.reset();
    }
  }

  pScan->clearResults();
  isScanning = false;

  if (!deviceFound) {
    LOG_SYSTEM("No compatible timer found — retrying in %d ms", BLE_SCAN_RETRY_INTERVAL_MS);
  }
}

void BridgeApplication::setupBleCallbacks() {
  ITimerDevice* dev = timerDevice.get();
  if (!dev) return;

  dev->onShotDetected([this](const NormalizedShotData& s) { onShotDetected(s); });
  dev->onSessionStarted([this](const SessionData& s) { onSessionStarted(s); });
  dev->onCountdownComplete([this](const SessionData& s) { onCountdownComplete(s); });
  dev->onSessionStopped([this](const SessionData& s) { onSessionStopped(s); });
  dev->onSessionSuspended([this](const SessionData& s) { onSessionSuspended(s); });
  dev->onSessionResumed([this](const SessionData& s) { onSessionResumed(s); });
  dev->onConnectionStateChanged([this](DeviceConnectionState st) { onConnectionStateChanged(st); });
}

// ─── BLE event handlers → LoRa TX ───────────────────────────

void BridgeApplication::onShotDetected(const NormalizedShotData& shot) {
  LOG_TIMER("Shot #%d: %.3fs (split: %.3fs)",
            shot.shotNumber, shot.absoluteTimeMs / 1000.0, shot.splitTimeMs / 1000.0);
  loraTx.sendShotDetected(shot);
  bridgeStatus.shotsTx++;
  bridgeStatus.hasLastShot = true;
  bridgeStatus.lastShotNumber = shot.shotNumber;
  bridgeStatus.lastShotTimeMs = shot.absoluteTimeMs;
  lastActivityTime = millis();
}

void BridgeApplication::onSessionStarted(const SessionData& session) {
  LOG_TIMER("Session started: ID %u, delay %.1fs", session.sessionId, session.startDelaySeconds);
  loraTx.sendSessionStarted(session.sessionId, session.startDelaySeconds);
  lastActivityTime = millis();
}

void BridgeApplication::onCountdownComplete(const SessionData& session) {
  LOG_TIMER("Countdown complete");
  loraTx.sendCountdownComplete(session.sessionId);
  lastActivityTime = millis();
}

void BridgeApplication::onSessionStopped(const SessionData& session) {
  LOG_TIMER("Session stopped: ID %u, %d shots", session.sessionId, session.totalShots);
  loraTx.sendSessionStopped(session.sessionId, session.totalShots, bridgeStatus.lastShotTimeMs);
  lastActivityTime = millis();
}

void BridgeApplication::onSessionSuspended(const SessionData& session) {
  LOG_TIMER("Session suspended");
  loraTx.sendSessionSuspended(session.sessionId);
  lastActivityTime = millis();
}

void BridgeApplication::onSessionResumed(const SessionData& session) {
  LOG_TIMER("Session resumed");
  loraTx.sendSessionResumed(session.sessionId);
  lastActivityTime = millis();
}

void BridgeApplication::onConnectionStateChanged(DeviceConnectionState state) {
  LOG_BLE("Connection state: %d", (int)state);

  bridgeStatus.bleConnected = (state == DeviceConnectionState::CONNECTED);
  bridgeStatus.bleScanning  = (state == DeviceConnectionState::SCANNING);
  if (timerDevice) {
    bridgeStatus.timerModel = timerDevice->getDeviceModel();
  }
  lastActivityTime = millis();
}

// ═════════════════════════════════════════════════════════════
// Receiver loop
// ═════════════════════════════════════════════════════════════

void BridgeApplication::runReceiver() {
  // Poll LoRa radio
  loraRx.update();

  // Output-specific maintenance
  if (outputMode == ReceiverOutputMode::MQTT_OUTPUT) {
    if (mqttManager) mqttManager->update();
  } else {
    bleServer.update();
  }
}

void BridgeApplication::setupLoRaCallbacks() {
  loraRx.onShotReceived([this](const LoRaProtocol::ParsedPacket& p) { onLoRaShotReceived(p); });
  loraRx.onSessionStarted([this](const LoRaProtocol::ParsedPacket& p) { onLoRaSessionStarted(p); });
  loraRx.onSessionStopped([this](const LoRaProtocol::ParsedPacket& p) { onLoRaSessionStopped(p); });
  loraRx.onCountdownComplete([this](const LoRaProtocol::ParsedPacket& p) { onLoRaCountdownComplete(p); });
  loraRx.onSessionSuspended([this](const LoRaProtocol::ParsedPacket& p) { onLoRaSessionSuspended(p); });
  loraRx.onSessionResumed([this](const LoRaProtocol::ParsedPacket& p) { onLoRaSessionResumed(p); });
  loraRx.onHeartbeatReceived([this](const LoRaProtocol::ParsedPacket& p) {
    LOG_DEBUG("LORA", "Heartbeat from %s (uptime %lu ms)", p.sourceId, (unsigned long)p.uptimeMs);
    lastActivityTime = millis();
  });
}

// ─── LoRa RX event handlers → MQTT or BLE ───────────────────

void BridgeApplication::onLoRaShotReceived(const LoRaProtocol::ParsedPacket& pkt) {
  LOG_TIMER("LoRa RX shot #%d: %.3fs (split: %.3fs) from %s",
            pkt.shot.shotNumber,
            pkt.shot.absoluteTimeMs / 1000.0,
            pkt.shot.splitTimeMs / 1000.0,
            pkt.sourceId);

  bridgeStatus.hasLastShot = true;
  bridgeStatus.lastShotNumber = pkt.shot.shotNumber;
  bridgeStatus.lastShotTimeMs = pkt.shot.absoluteTimeMs;
  bridgeStatus.shotsRx++;
  lastActivityTime = millis();

  if (outputMode == ReceiverOutputMode::MQTT_OUTPUT) {
    if (mqttManager && mqttManager->canPublish()) {
      mqttManager->publishShotDetected(pkt.shot);
    }
  } else {
    bleServer.sendShotDetected(pkt.shot.absoluteTimeMs, pkt.shot.shotNumber);
  }
}

void BridgeApplication::onLoRaSessionStarted(const LoRaProtocol::ParsedPacket& pkt) {
  LOG_TIMER("LoRa RX session started: ID %u, delay %.1fs", pkt.sessionId, pkt.startDelaySeconds);
  lastActivityTime = millis();

  if (outputMode == ReceiverOutputMode::MQTT_OUTPUT) {
    if (mqttManager && mqttManager->canPublish()) {
      mqttManager->publishSessionStarted(pkt.sessionId, pkt.startDelaySeconds);
    }
  } else {
    bleServer.sendSessionStart((uint8_t)(pkt.sessionId & 0xFF));
  }
}

void BridgeApplication::onLoRaSessionStopped(const LoRaProtocol::ParsedPacket& pkt) {
  LOG_TIMER("LoRa RX session stopped: ID %u, %u shots", pkt.sessionId, pkt.totalShots);
  lastActivityTime = millis();

  if (outputMode == ReceiverOutputMode::MQTT_OUTPUT) {
    if (mqttManager && mqttManager->canPublish()) {
      mqttManager->publishSessionStopped(pkt.sessionId, pkt.totalShots, pkt.lastShotTimeMs);
    }
  } else {
    bleServer.sendSessionStop((uint8_t)(pkt.sessionId & 0xFF));
  }
}

void BridgeApplication::onLoRaCountdownComplete(const LoRaProtocol::ParsedPacket& pkt) {
  LOG_TIMER("LoRa RX countdown complete: ID %u", pkt.sessionId);
  lastActivityTime = millis();

  if (outputMode == ReceiverOutputMode::MQTT_OUTPUT) {
    if (mqttManager && mqttManager->canPublish()) {
      mqttManager->publishCountdownComplete(pkt.sessionId);
    }
  }
  // No Special Pie equivalent for countdown
}

void BridgeApplication::onLoRaSessionSuspended(const LoRaProtocol::ParsedPacket& pkt) {
  LOG_TIMER("LoRa RX session suspended: ID %u", pkt.sessionId);
  lastActivityTime = millis();

  if (outputMode == ReceiverOutputMode::MQTT_OUTPUT) {
    if (mqttManager && mqttManager->canPublish()) {
      mqttManager->publishSessionSuspended(pkt.sessionId);
    }
  }
}

void BridgeApplication::onLoRaSessionResumed(const LoRaProtocol::ParsedPacket& pkt) {
  LOG_TIMER("LoRa RX session resumed: ID %u", pkt.sessionId);
  lastActivityTime = millis();

  if (outputMode == ReceiverOutputMode::MQTT_OUTPUT) {
    if (mqttManager && mqttManager->canPublish()) {
      mqttManager->publishSessionResumed(pkt.sessionId);
    }
  }
}

// ═════════════════════════════════════════════════════════════
// OLED status
// ═════════════════════════════════════════════════════════════

void BridgeApplication::updateOledStatus() {
  bridgeStatus.role       = role;
  bridgeStatus.outputMode = outputMode;
  bridgeStatus.wifiConnected = BridgeWiFiConfig::isConnected();
  bridgeStatus.uptimeMs   = millis();

  if (role == BridgeRole::RECEIVER) {
    bridgeStatus.lastRssi   = loraRx.getLastRssi();
    bridgeStatus.crcErrors  = loraRx.getCrcErrors();
    // shotsRx is incremented in onLoRaShotReceived(), not overwritten with total packets

    if (outputMode == ReceiverOutputMode::MQTT_OUTPUT) {
      bridgeStatus.mqttConnected = mqttManager && mqttManager->isHealthy();
    } else {
      bridgeStatus.bleClients = bleServer.getConnectedCount();
    }
  }
  // shotsTx is incremented in onShotDetected(), not overwritten with total packets
}
