#pragma once

#include <Arduino.h>
#include "ITimerDevice.h"

// =============================================================================
// LoRa Packet Protocol — Binary format with CRC-16/CCITT
//
// Frame layout:
//   [MAGIC 2B][TYPE 1B][SOURCE_ID 6B][PAYLOAD variable][CRC16 2B]
//
// Magic bytes: 0x50 0x57 ("PW" — PewPew)
// All multi-byte fields are little-endian.
// CRC-16/CCITT (polynomial 0x1021, init 0xFFFF) covers entire packet
// excluding the trailing CRC bytes.
// =============================================================================

namespace LoRaProtocol {

// Magic bytes identifying a valid PewPew LoRa packet
static constexpr uint8_t MAGIC_0 = 0x50;  // 'P'
static constexpr uint8_t MAGIC_1 = 0x57;  // 'W'

// Packet type identifiers
enum class PacketType : uint8_t {
  SHOT_DETECTED       = 0x01,
  SESSION_STARTED     = 0x02,
  SESSION_STOPPED     = 0x03,
  COUNTDOWN_COMPLETE  = 0x04,
  SESSION_SUSPENDED   = 0x05,
  SESSION_RESUMED     = 0x06,
  HEARTBEAT           = 0x07
};

// Header common to all packets: magic(2) + type(1) + sourceId(6) = 9 bytes
static constexpr size_t HEADER_SIZE     = 9;
static constexpr size_t CRC_SIZE        = 2;
static constexpr size_t SOURCE_ID_LEN   = 6;

// Maximum payload size (shot packet is largest)
static constexpr size_t MAX_PAYLOAD_SIZE = 31;  // shot: 4+2+4+4+1+16 = 31

// Maximum total packet size: header + max payload + CRC
static constexpr size_t MAX_PACKET_SIZE = HEADER_SIZE + MAX_PAYLOAD_SIZE + CRC_SIZE;

// Payload sizes per packet type
static constexpr size_t PAYLOAD_SHOT_DETECTED      = 31;  // sessionId(4)+shotNum(2)+absMs(4)+splitMs(4)+isFirst(1)+model(16)
static constexpr size_t PAYLOAD_SESSION_STARTED     = 8;   // sessionId(4)+startDelay(4 float)
static constexpr size_t PAYLOAD_SESSION_STOPPED     = 10;  // sessionId(4)+totalShots(2)+lastShotMs(4)
static constexpr size_t PAYLOAD_COUNTDOWN_COMPLETE  = 4;   // sessionId(4)
static constexpr size_t PAYLOAD_SESSION_SUSPENDED   = 4;   // sessionId(4)
static constexpr size_t PAYLOAD_SESSION_RESUMED     = 4;   // sessionId(4)
static constexpr size_t PAYLOAD_HEARTBEAT           = 4;   // uptimeMs(4)

// ─── Serialization helpers ──────────────────────────────────

/**
 * Build a SHOT_DETECTED packet into `buf`.
 * @return total bytes written (including header + CRC), or 0 on error.
 */
size_t serializeShotDetected(uint8_t* buf, size_t bufLen,
                             const char* sourceId,
                             const NormalizedShotData& shot);

/**
 * Build a SESSION_STARTED packet.
 */
size_t serializeSessionStarted(uint8_t* buf, size_t bufLen,
                               const char* sourceId,
                               uint32_t sessionId, float startDelaySeconds);

/**
 * Build a SESSION_STOPPED packet.
 */
size_t serializeSessionStopped(uint8_t* buf, size_t bufLen,
                               const char* sourceId,
                               uint32_t sessionId, uint16_t totalShots,
                               uint32_t lastShotTimeMs);

/**
 * Build a COUNTDOWN_COMPLETE packet.
 */
size_t serializeCountdownComplete(uint8_t* buf, size_t bufLen,
                                  const char* sourceId,
                                  uint32_t sessionId);

/**
 * Build a SESSION_SUSPENDED packet.
 */
size_t serializeSessionSuspended(uint8_t* buf, size_t bufLen,
                                 const char* sourceId,
                                 uint32_t sessionId);

/**
 * Build a SESSION_RESUMED packet.
 */
size_t serializeSessionResumed(uint8_t* buf, size_t bufLen,
                               const char* sourceId,
                               uint32_t sessionId);

/**
 * Build a HEARTBEAT packet.
 */
size_t serializeHeartbeat(uint8_t* buf, size_t bufLen,
                          const char* sourceId,
                          uint32_t uptimeMs);

// ─── Deserialization ─────────────────────────────────────────

/**
 * Parsed packet container returned by deserialize().
 */
struct ParsedPacket {
  PacketType type;
  char sourceId[SOURCE_ID_LEN + 1];  // null-terminated

  // Union-like fields — only the set matching `type` is valid
  // SHOT_DETECTED
  NormalizedShotData shot;

  // SESSION_STARTED
  uint32_t sessionId;
  float startDelaySeconds;

  // SESSION_STOPPED
  uint16_t totalShots;
  uint32_t lastShotTimeMs;

  // HEARTBEAT
  uint32_t uptimeMs;
};

/**
 * Validate CRC and deserialize a raw LoRa packet.
 * @param data  raw bytes received from LoRa.parsePacket()
 * @param len   number of bytes
 * @param out   output parsed packet
 * @return true if CRC is valid and packet was parsed successfully
 */
bool deserialize(const uint8_t* data, size_t len, ParsedPacket& out);

// ─── CRC utility ─────────────────────────────────────────────

/**
 * CRC-16/CCITT (polynomial 0x1021, init 0xFFFF)
 */
uint16_t crc16(const uint8_t* data, size_t len);

}  // namespace LoRaProtocol
