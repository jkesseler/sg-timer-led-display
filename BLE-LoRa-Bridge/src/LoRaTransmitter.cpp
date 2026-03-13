#include "LoRaTransmitter.h"
#include "DeviceId.h"
#include "common.h"
#include <LoRa.h>
#include <SPI.h>

bool LoRaTransmitter::initialize() {
  // Store device ID for packet source field
  String id = deviceId.get();
  strncpy(sourceId, id.c_str(), sizeof(sourceId) - 1);
  sourceId[sizeof(sourceId) - 1] = '\0';

  // Configure SPI pins for LoRa32 T3 v1.6.1
  SPI.begin(LORA_SCK_PIN, LORA_MISO_PIN, LORA_MOSI_PIN, LORA_CS_PIN);
  LoRa.setPins(LORA_CS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    LOG_ERROR("LORA", "SX1276 init failed");
    return false;
  }

  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.setTxPower(LORA_TX_POWER);
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.setPreambleLength(LORA_PREAMBLE_LENGTH);
  LoRa.enableCrc();  // Hardware CRC in addition to our packet-level CRC

  LOG_INFO("LORA", "Transmitter initialized (SF%d BW%.0fkHz %ddBm) src=%s",
           LORA_SPREADING_FACTOR, LORA_BANDWIDTH / 1000.0,
           LORA_TX_POWER, sourceId);
  return true;
}

void LoRaTransmitter::update() {
  // Send periodic heartbeat so the receiver knows we're alive
  unsigned long now = millis();
  if (now - lastHeartbeat >= LORA_HEARTBEAT_INTERVAL) {
    size_t len = LoRaProtocol::serializeHeartbeat(
        txBuffer, sizeof(txBuffer), sourceId, (uint32_t)now);
    if (len > 0) {
      transmitPacket(txBuffer, len);
      LOG_DEBUG("LORA", "Heartbeat sent (uptime %lu ms)", now);
    }
    lastHeartbeat = now;
  }
}

bool LoRaTransmitter::sendShotDetected(const NormalizedShotData& shot) {
  size_t len = LoRaProtocol::serializeShotDetected(
      txBuffer, sizeof(txBuffer), sourceId, shot);
  if (len == 0) return false;
  return transmitPacket(txBuffer, len);
}

bool LoRaTransmitter::sendSessionStarted(uint32_t sessionId, float startDelaySeconds) {
  size_t len = LoRaProtocol::serializeSessionStarted(
      txBuffer, sizeof(txBuffer), sourceId, sessionId, startDelaySeconds);
  if (len == 0) return false;
  return transmitPacket(txBuffer, len);
}

bool LoRaTransmitter::sendSessionStopped(uint32_t sessionId, uint16_t totalShots, uint32_t lastShotTimeMs) {
  size_t len = LoRaProtocol::serializeSessionStopped(
      txBuffer, sizeof(txBuffer), sourceId, sessionId, totalShots, lastShotTimeMs);
  if (len == 0) return false;
  return transmitPacket(txBuffer, len);
}

bool LoRaTransmitter::sendCountdownComplete(uint32_t sessionId) {
  size_t len = LoRaProtocol::serializeCountdownComplete(
      txBuffer, sizeof(txBuffer), sourceId, sessionId);
  if (len == 0) return false;
  return transmitPacket(txBuffer, len);
}

bool LoRaTransmitter::sendSessionSuspended(uint32_t sessionId) {
  size_t len = LoRaProtocol::serializeSessionSuspended(
      txBuffer, sizeof(txBuffer), sourceId, sessionId);
  if (len == 0) return false;
  return transmitPacket(txBuffer, len);
}

bool LoRaTransmitter::sendSessionResumed(uint32_t sessionId) {
  size_t len = LoRaProtocol::serializeSessionResumed(
      txBuffer, sizeof(txBuffer), sourceId, sessionId);
  if (len == 0) return false;
  return transmitPacket(txBuffer, len);
}

bool LoRaTransmitter::transmitPacket(const uint8_t* data, size_t len) {
  LoRa.beginPacket();
  LoRa.write(data, len);
  if (LoRa.endPacket()) {
    packetsSent++;
    LOG_DEBUG("LORA", "TX %u bytes (total: %lu)", (unsigned)len, (unsigned long)packetsSent);
    return true;
  }
  LOG_ERROR("LORA", "TX failed (%u bytes)", (unsigned)len);
  return false;
}
