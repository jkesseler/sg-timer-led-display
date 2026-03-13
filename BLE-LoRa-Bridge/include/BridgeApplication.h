#pragma once

#include "common.h"
#include "ITimerDevice.h"
#include "LoRaTransmitter.h"
#include "LoRaReceiver.h"
#include "SpecialPieBleServer.h"
#include "BridgeOledDisplay.h"
#include "BridgeWiFiConfig.h"
#include "MqttManager.h"
#include "Logger.h"
#include <memory>

/**
 * @brief Main application coordinator for the BLE-LoRa Bridge
 *
 * Role-aware: reads configuration from BridgeWiFiConfig at startup
 * and instantiates the correct component graph.
 *
 * Transmitter: ITimerDevice* → callbacks → LoRaTransmitter → SX1276 TX
 * Receiver:    SX1276 RX → LoRaReceiver → callbacks → MqttManager | SpecialPieBleServer
 */
class BridgeApplication {
public:
  BridgeApplication();
  ~BridgeApplication();

  bool initialize();
  void run();

  BridgeRole getRole() const { return role; }

private:
  BridgeRole role;
  ReceiverOutputMode outputMode;

  // ─── Transmitter components ───
  std::unique_ptr<ITimerDevice> timerDevice;
  LoRaTransmitter loraTx;

  // BLE scan state
  unsigned long lastScanAttempt = 0;
  bool isScanning = false;
  bool scanResultsReady = false;
  unsigned long startupTime = 0;

  // ─── Receiver components ───
  LoRaReceiver loraRx;
  std::unique_ptr<MqttManager> mqttManager;
  SpecialPieBleServer bleServer;

  // ─── Shared components ───
  OledDisplay oled;
  BridgeStatus bridgeStatus;

  // Health
  unsigned long lastHealthCheck = 0;
  unsigned long lastActivityTime = 0;

  // ─── Transmitter helpers ───
  void initTransmitter();
  void runTransmitter();
  void scanForDevices();
  void processScanResults();
  void setupBleCallbacks();

  // BLE event handlers (Transmitter)
  void onShotDetected(const NormalizedShotData& shot);
  void onSessionStarted(const SessionData& session);
  void onCountdownComplete(const SessionData& session);
  void onSessionStopped(const SessionData& session);
  void onSessionSuspended(const SessionData& session);
  void onSessionResumed(const SessionData& session);
  void onConnectionStateChanged(DeviceConnectionState state);

  // ─── Receiver helpers ───
  void initReceiver();
  void runReceiver();
  void setupLoRaCallbacks();

  // LoRa event handlers (Receiver)
  void onLoRaShotReceived(const LoRaProtocol::ParsedPacket& pkt);
  void onLoRaSessionStarted(const LoRaProtocol::ParsedPacket& pkt);
  void onLoRaSessionStopped(const LoRaProtocol::ParsedPacket& pkt);
  void onLoRaCountdownComplete(const LoRaProtocol::ParsedPacket& pkt);
  void onLoRaSessionSuspended(const LoRaProtocol::ParsedPacket& pkt);
  void onLoRaSessionResumed(const LoRaProtocol::ParsedPacket& pkt);

  void updateOledStatus();
};
