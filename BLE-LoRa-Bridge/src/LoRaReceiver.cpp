#include "LoRaReceiver.h"
#include "LoRaRadio.h"
#include "common.h"
#include <LoRa.h>

bool LoRaReceiver::initialize() {
  if (!LoRaRadio::initialize()) {
    LOG_ERROR("LORA", "SX1276 init failed");
    return false;
  }

  LOG_INFO("LORA", "Receiver initialized (SF%d BW%.0fkHz)",
           LORA_SPREADING_FACTOR, LORA_BANDWIDTH / 1000.0);
  return true;
}

void LoRaReceiver::update() {
  int packetSize = LoRa.parsePacket();
  if (packetSize <= 0) return;

  // Guard against oversized packets
  if ((size_t)packetSize > sizeof(rxBuffer)) {
    LOG_WARN("LORA", "Packet too large (%d bytes), discarding", packetSize);
    // Drain the packet
    while (LoRa.available()) LoRa.read();
    return;
  }

  // Read raw bytes
  size_t bytesRead = 0;
  while (LoRa.available() && bytesRead < (size_t)packetSize) {
    rxBuffer[bytesRead++] = (uint8_t)LoRa.read();
  }

  lastRssi = LoRa.packetRssi();

  // Deserialize (includes CRC validation)
  LoRaProtocol::ParsedPacket pkt;
  if (!LoRaProtocol::deserialize(rxBuffer, bytesRead, pkt)) {
    crcErrors++;
    LOG_WARN("LORA", "CRC/parse error (RSSI %d, %u bytes, errors: %lu)",
             lastRssi, (unsigned)bytesRead, (unsigned long)crcErrors);
    return;
  }

  packetsReceived++;
  LOG_DEBUG("LORA", "RX type=0x%02X from %s (RSSI %d, total: %lu)",
            (uint8_t)pkt.type, pkt.sourceId, lastRssi, (unsigned long)packetsReceived);

  // Dispatch to registered callbacks
  switch (pkt.type) {
    case LoRaProtocol::PacketType::SHOT_DETECTED:
      if (shotCallback) shotCallback(pkt);
      break;
    case LoRaProtocol::PacketType::SESSION_STARTED:
      if (sessionStartedCallback) sessionStartedCallback(pkt);
      break;
    case LoRaProtocol::PacketType::SESSION_STOPPED:
      if (sessionStoppedCallback) sessionStoppedCallback(pkt);
      break;
    case LoRaProtocol::PacketType::COUNTDOWN_COMPLETE:
      if (countdownCallback) countdownCallback(pkt);
      break;
    case LoRaProtocol::PacketType::SESSION_SUSPENDED:
      if (suspendedCallback) suspendedCallback(pkt);
      break;
    case LoRaProtocol::PacketType::SESSION_RESUMED:
      if (resumedCallback) resumedCallback(pkt);
      break;
    case LoRaProtocol::PacketType::HEARTBEAT:
      if (heartbeatCallback) heartbeatCallback(pkt);
      break;
  }
}
