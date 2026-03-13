#pragma once

#include "LoRaPacket.h"
#include "Logger.h"
#include <functional>

/**
 * @brief LoRa transmitter for Device 1 (BLE → LoRa)
 *
 * Serializes NormalizedShotData and session events into LoRa packets
 * and transmits them via the SX1276 radio.
 */
class LoRaTransmitter {
public:
  bool initialize();
  void update();  // Sends periodic heartbeat

  // Event transmitters — called by BridgeApplication when BLE callbacks fire
  bool sendShotDetected(const NormalizedShotData& shot);
  bool sendSessionStarted(uint32_t sessionId, float startDelaySeconds);
  bool sendSessionStopped(uint32_t sessionId, uint16_t totalShots, uint32_t lastShotTimeMs);
  bool sendCountdownComplete(uint32_t sessionId);
  bool sendSessionSuspended(uint32_t sessionId);
  bool sendSessionResumed(uint32_t sessionId);

  uint32_t getPacketsSent() const { return packetsSent; }

private:
  bool transmitPacket(const uint8_t* data, size_t len);

  char sourceId[7] = {0};  // Populated from DeviceId at initialize()
  uint8_t txBuffer[LoRaProtocol::MAX_PACKET_SIZE];

  uint32_t packetsSent = 0;
  unsigned long lastHeartbeat = 0;
};
