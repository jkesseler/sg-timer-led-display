#pragma once

#include "LoRaPacket.h"
#include "Logger.h"
#include <functional>

/**
 * @brief LoRa receiver for Device 2 (LoRa → MQTT or LoRa → BLE)
 *
 * Polls the SX1276 radio for incoming packets, validates CRC,
 * deserializes, and fires callbacks for each event type.
 */
class LoRaReceiver {
public:
  bool initialize();
  void update();  // Poll for incoming packets — call every loop iteration

  // Callback registration
  void onShotReceived(std::function<void(const LoRaProtocol::ParsedPacket&)> cb) { shotCallback = cb; }
  void onSessionStarted(std::function<void(const LoRaProtocol::ParsedPacket&)> cb) { sessionStartedCallback = cb; }
  void onSessionStopped(std::function<void(const LoRaProtocol::ParsedPacket&)> cb) { sessionStoppedCallback = cb; }
  void onCountdownComplete(std::function<void(const LoRaProtocol::ParsedPacket&)> cb) { countdownCallback = cb; }
  void onSessionSuspended(std::function<void(const LoRaProtocol::ParsedPacket&)> cb) { suspendedCallback = cb; }
  void onSessionResumed(std::function<void(const LoRaProtocol::ParsedPacket&)> cb) { resumedCallback = cb; }
  void onHeartbeatReceived(std::function<void(const LoRaProtocol::ParsedPacket&)> cb) { heartbeatCallback = cb; }

  int getLastRssi() const { return lastRssi; }
  uint32_t getPacketsReceived() const { return packetsReceived; }
  uint32_t getCrcErrors() const { return crcErrors; }

private:
  std::function<void(const LoRaProtocol::ParsedPacket&)> shotCallback;
  std::function<void(const LoRaProtocol::ParsedPacket&)> sessionStartedCallback;
  std::function<void(const LoRaProtocol::ParsedPacket&)> sessionStoppedCallback;
  std::function<void(const LoRaProtocol::ParsedPacket&)> countdownCallback;
  std::function<void(const LoRaProtocol::ParsedPacket&)> suspendedCallback;
  std::function<void(const LoRaProtocol::ParsedPacket&)> resumedCallback;
  std::function<void(const LoRaProtocol::ParsedPacket&)> heartbeatCallback;

  uint8_t rxBuffer[LoRaProtocol::MAX_PACKET_SIZE];
  int lastRssi = 0;
  uint32_t packetsReceived = 0;
  uint32_t crcErrors = 0;
};
