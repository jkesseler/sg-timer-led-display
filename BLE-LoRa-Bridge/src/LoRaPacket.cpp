#include "LoRaPacket.h"
#include <cstring>

namespace LoRaProtocol {

// ─── CRC-16/CCITT (poly 0x1021, init 0xFFFF) ────────────────

uint16_t crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

// ─── Internal helpers ────────────────────────────────────────

static size_t writeHeader(uint8_t* buf, PacketType type, const char* sourceId) {
  buf[0] = MAGIC_0;
  buf[1] = MAGIC_1;
  buf[2] = static_cast<uint8_t>(type);
  // Copy 6 bytes of source ID (pad with zeros if shorter)
  memset(&buf[3], 0, SOURCE_ID_LEN);
  size_t idLen = strlen(sourceId);
  if (idLen > SOURCE_ID_LEN) idLen = SOURCE_ID_LEN;
  memcpy(&buf[3], sourceId, idLen);
  return HEADER_SIZE;
}

static void writeU16(uint8_t* buf, uint16_t val) {
  buf[0] = (uint8_t)(val & 0xFF);
  buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

static void writeU32(uint8_t* buf, uint32_t val) {
  buf[0] = (uint8_t)(val & 0xFF);
  buf[1] = (uint8_t)((val >> 8) & 0xFF);
  buf[2] = (uint8_t)((val >> 16) & 0xFF);
  buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

static void writeFloat(uint8_t* buf, float val) {
  uint32_t raw;
  memcpy(&raw, &val, sizeof(raw));
  writeU32(buf, raw);
}

static uint16_t readU16(const uint8_t* buf) {
  return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

static uint32_t readU32(const uint8_t* buf) {
  return (uint32_t)buf[0]
       | ((uint32_t)buf[1] << 8)
       | ((uint32_t)buf[2] << 16)
       | ((uint32_t)buf[3] << 24);
}

static float readFloat(const uint8_t* buf) {
  uint32_t raw = readU32(buf);
  float val;
  memcpy(&val, &raw, sizeof(val));
  return val;
}

static size_t appendCrc(uint8_t* buf, size_t packetLen) {
  uint16_t crcVal = crc16(buf, packetLen);
  writeU16(&buf[packetLen], crcVal);
  return packetLen + CRC_SIZE;
}

// ─── Serialization ───────────────────────────────────────────

size_t serializeShotDetected(uint8_t* buf, size_t bufLen,
                             const char* sourceId,
                             const NormalizedShotData& shot) {
  const size_t needed = HEADER_SIZE + PAYLOAD_SHOT_DETECTED + CRC_SIZE;
  if (bufLen < needed) return 0;

  size_t pos = writeHeader(buf, PacketType::SHOT_DETECTED, sourceId);

  writeU32(&buf[pos], shot.sessionId);        pos += 4;
  writeU16(&buf[pos], shot.shotNumber);        pos += 2;
  writeU32(&buf[pos], shot.absoluteTimeMs);    pos += 4;
  writeU32(&buf[pos], shot.splitTimeMs);       pos += 4;
  buf[pos] = shot.isFirstShot ? 1 : 0;        pos += 1;

  // Device model — 16 bytes, null-padded
  memset(&buf[pos], 0, 16);
  strncpy((char*)&buf[pos], shot.deviceModel, 16);
  pos += 16;

  return appendCrc(buf, pos);
}

size_t serializeSessionStarted(uint8_t* buf, size_t bufLen,
                               const char* sourceId,
                               uint32_t sessionId, float startDelaySeconds) {
  const size_t needed = HEADER_SIZE + PAYLOAD_SESSION_STARTED + CRC_SIZE;
  if (bufLen < needed) return 0;

  size_t pos = writeHeader(buf, PacketType::SESSION_STARTED, sourceId);
  writeU32(&buf[pos], sessionId);              pos += 4;
  writeFloat(&buf[pos], startDelaySeconds);    pos += 4;

  return appendCrc(buf, pos);
}

size_t serializeSessionStopped(uint8_t* buf, size_t bufLen,
                               const char* sourceId,
                               uint32_t sessionId, uint16_t totalShots,
                               uint32_t lastShotTimeMs) {
  const size_t needed = HEADER_SIZE + PAYLOAD_SESSION_STOPPED + CRC_SIZE;
  if (bufLen < needed) return 0;

  size_t pos = writeHeader(buf, PacketType::SESSION_STOPPED, sourceId);
  writeU32(&buf[pos], sessionId);              pos += 4;
  writeU16(&buf[pos], totalShots);             pos += 2;
  writeU32(&buf[pos], lastShotTimeMs);         pos += 4;

  return appendCrc(buf, pos);
}

size_t serializeCountdownComplete(uint8_t* buf, size_t bufLen,
                                  const char* sourceId,
                                  uint32_t sessionId) {
  const size_t needed = HEADER_SIZE + PAYLOAD_COUNTDOWN_COMPLETE + CRC_SIZE;
  if (bufLen < needed) return 0;

  size_t pos = writeHeader(buf, PacketType::COUNTDOWN_COMPLETE, sourceId);
  writeU32(&buf[pos], sessionId);              pos += 4;

  return appendCrc(buf, pos);
}

size_t serializeSessionSuspended(uint8_t* buf, size_t bufLen,
                                 const char* sourceId,
                                 uint32_t sessionId) {
  const size_t needed = HEADER_SIZE + PAYLOAD_SESSION_SUSPENDED + CRC_SIZE;
  if (bufLen < needed) return 0;

  size_t pos = writeHeader(buf, PacketType::SESSION_SUSPENDED, sourceId);
  writeU32(&buf[pos], sessionId);              pos += 4;

  return appendCrc(buf, pos);
}

size_t serializeSessionResumed(uint8_t* buf, size_t bufLen,
                               const char* sourceId,
                               uint32_t sessionId) {
  const size_t needed = HEADER_SIZE + PAYLOAD_SESSION_RESUMED + CRC_SIZE;
  if (bufLen < needed) return 0;

  size_t pos = writeHeader(buf, PacketType::SESSION_RESUMED, sourceId);
  writeU32(&buf[pos], sessionId);              pos += 4;

  return appendCrc(buf, pos);
}

size_t serializeHeartbeat(uint8_t* buf, size_t bufLen,
                          const char* sourceId,
                          uint32_t uptimeMs) {
  const size_t needed = HEADER_SIZE + PAYLOAD_HEARTBEAT + CRC_SIZE;
  if (bufLen < needed) return 0;

  size_t pos = writeHeader(buf, PacketType::HEARTBEAT, sourceId);
  writeU32(&buf[pos], uptimeMs);               pos += 4;

  return appendCrc(buf, pos);
}

// ─── Deserialization ─────────────────────────────────────────

bool deserialize(const uint8_t* data, size_t len, ParsedPacket& out) {
  // Minimum packet: header(9) + smallest payload(4) + crc(2) = 15
  if (len < HEADER_SIZE + 4 + CRC_SIZE) return false;

  // Verify magic
  if (data[0] != MAGIC_0 || data[1] != MAGIC_1) return false;

  // Verify CRC over everything except the last 2 bytes
  uint16_t receivedCrc = readU16(&data[len - CRC_SIZE]);
  uint16_t computedCrc = crc16(data, len - CRC_SIZE);
  if (receivedCrc != computedCrc) return false;

  // Parse header
  out.type = static_cast<PacketType>(data[2]);
  memcpy(out.sourceId, &data[3], SOURCE_ID_LEN);
  out.sourceId[SOURCE_ID_LEN] = '\0';

  const uint8_t* payload = &data[HEADER_SIZE];
  size_t payloadLen = len - HEADER_SIZE - CRC_SIZE;

  switch (out.type) {
    case PacketType::SHOT_DETECTED: {
      if (payloadLen < PAYLOAD_SHOT_DETECTED) return false;
      memset(&out.shot, 0, sizeof(out.shot));
      out.shot.sessionId      = readU32(&payload[0]);
      out.shot.shotNumber     = readU16(&payload[4]);
      out.shot.absoluteTimeMs = readU32(&payload[6]);
      out.shot.splitTimeMs    = readU32(&payload[10]);
      out.shot.isFirstShot    = (payload[14] != 0);
      // Copy model string (16 bytes, may not be null-terminated in packet)
      memcpy(out.shot.deviceModel, &payload[15], 16);
      out.shot.deviceModel[16] = '\0';
      out.shot.timestampMs = millis();  // Local receive timestamp
      // sessionId also stored at packet level
      out.sessionId = out.shot.sessionId;
      return true;
    }
    case PacketType::SESSION_STARTED: {
      if (payloadLen < PAYLOAD_SESSION_STARTED) return false;
      out.sessionId = readU32(&payload[0]);
      out.startDelaySeconds = readFloat(&payload[4]);
      return true;
    }
    case PacketType::SESSION_STOPPED: {
      if (payloadLen < PAYLOAD_SESSION_STOPPED) return false;
      out.sessionId = readU32(&payload[0]);
      out.totalShots = readU16(&payload[4]);
      out.lastShotTimeMs = readU32(&payload[6]);
      return true;
    }
    case PacketType::COUNTDOWN_COMPLETE: {
      if (payloadLen < PAYLOAD_COUNTDOWN_COMPLETE) return false;
      out.sessionId = readU32(&payload[0]);
      return true;
    }
    case PacketType::SESSION_SUSPENDED: {
      if (payloadLen < PAYLOAD_SESSION_SUSPENDED) return false;
      out.sessionId = readU32(&payload[0]);
      return true;
    }
    case PacketType::SESSION_RESUMED: {
      if (payloadLen < PAYLOAD_SESSION_RESUMED) return false;
      out.sessionId = readU32(&payload[0]);
      return true;
    }
    case PacketType::HEARTBEAT: {
      if (payloadLen < PAYLOAD_HEARTBEAT) return false;
      out.uptimeMs = readU32(&payload[0]);
      return true;
    }
    default:
      return false;
  }
}

}  // namespace LoRaProtocol
