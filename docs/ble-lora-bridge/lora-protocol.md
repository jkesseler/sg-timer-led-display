# BLE-LoRa Bridge — LoRa Packet Protocol

This document specifies the binary application-layer protocol used over the SX1276 LoRa radio link. The canonical C++ definitions live in:

- `BLE-LoRa-Bridge/include/LoRaPacket.h` — types, constants, function declarations
- `BLE-LoRa-Bridge/src/LoRaPacket.cpp` — serialization / deserialization / CRC

---

## Frame layout

```
[MAGIC 2B][TYPE 1B][SOURCE_ID 6B][PAYLOAD variable][CRC16 2B]
```

| Field | Size | Description |
|---|---|---|
| MAGIC | 2 bytes | `0x50 0x57` — ASCII "PW" (PewPew) |
| TYPE | 1 byte | Packet type identifier (see below) |
| SOURCE_ID | 6 bytes | Transmitter device ID (from `DeviceId` singleton) |
| PAYLOAD | variable | Type-specific payload (0–31 bytes) |
| CRC16 | 2 bytes | CRC-16/CCITT over all preceding bytes |

- All multi-byte integer fields are **little-endian**.
- Maximum total packet size: **42 bytes** (9-byte header + 31-byte payload + 2-byte CRC).
- The SX1276 hardware CRC is also enabled; the application-layer CRC provides an additional guard against corruption.

---

## CRC-16/CCITT

- Polynomial: `0x1021`
- Initial value: `0xFFFF`
- Input: entire packet bytes **excluding** the trailing 2 CRC bytes
- Implementation: `LoRaProtocol::crc16(const uint8_t* data, size_t len)`

---

## Packet types

| Type name | Byte | Payload size | When sent |
|---|---|---|---|
| `SHOT_DETECTED` | `0x01` | 31 bytes | Each shot detected by the BLE timer |
| `SESSION_STARTED` | `0x02` | 8 bytes | Session / stage started (start delay complete) |
| `SESSION_STOPPED` | `0x03` | 10 bytes | Session stopped or finished |
| `COUNTDOWN_COMPLETE` | `0x04` | 4 bytes | Start beep fired (countdown done, shooter may fire) |
| `SESSION_SUSPENDED` | `0x05` | 4 bytes | Session paused |
| `SESSION_RESUMED` | `0x06` | 4 bytes | Session resumed after pause |
| `HEARTBEAT` | `0x07` | 4 bytes | Periodic keepalive from Transmitter (every 30 s) |

---

## Payload layouts

### SHOT_DETECTED (31 bytes)

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| 0 | 4 | `uint32_t` | `sessionId` | Session identifier |
| 4 | 2 | `uint16_t` | `shotNumber` | 1-based shot index within session |
| 6 | 4 | `uint32_t` | `absoluteTimeMs` | Time since session start in milliseconds |
| 10 | 4 | `uint32_t` | `splitTimeMs` | Time since previous shot in milliseconds (0 for first shot) |
| 14 | 1 | `uint8_t` | `isFirstShot` | `1` if this is the first shot in the session |
| 15 | 16 | `char[16]` | `model` | Null-padded timer model string |

### SESSION_STARTED (8 bytes)

| Offset | Size | Type | Field |
|---|---|---|---|
| 0 | 4 | `uint32_t` | `sessionId` |
| 4 | 4 | `float` | `startDelaySeconds` |

### SESSION_STOPPED (10 bytes)

| Offset | Size | Type | Field |
|---|---|---|---|
| 0 | 4 | `uint32_t` | `sessionId` |
| 4 | 2 | `uint16_t` | `totalShots` |
| 6 | 4 | `uint32_t` | `lastShotTimeMs` |

### COUNTDOWN_COMPLETE, SESSION_SUSPENDED, SESSION_RESUMED (4 bytes each)

| Offset | Size | Type | Field |
|---|---|---|---|
| 0 | 4 | `uint32_t` | `sessionId` |

### HEARTBEAT (4 bytes)

| Offset | Size | Type | Field |
|---|---|---|---|
| 0 | 4 | `uint32_t` | `uptimeMs` — Transmitter uptime in milliseconds |

---

## Special Pie BLE re-emission format

When the Receiver is configured for **BLE Special Pie** output mode, it re-emits events as notifications on a GATT characteristic that mimics the Special Pie M1A2+ peripheral. The format is:

```
F8 F9 [MSG_TYPE] [DATA...] F9 F8
```

### Shot detected

```
F8 F9 36 00 [SEC] [CS] [SHOT_IDX] [CHECKSUM] F9 F8
```

| Byte | Value | Notes |
|---|---|---|
| `SEC` | `absoluteTimeMs / 1000` | Capped at 255 |
| `CS` | `(absoluteTimeMs % 1000) / 10` | Centiseconds fraction |
| `SHOT_IDX` | `shotNumber - 1` | 0-based index |
| `CHECKSUM` | `SEC XOR CS XOR SHOT_IDX` | Simple XOR checksum |

### Session started

```
F8 F9 34 [SESSION_ID_BYTE] F9 F8
```

### Session stopped

```
F8 F9 18 [SESSION_ID_BYTE] F9 F8
```

The Special Pie BLE service UUID is `0000FFF0-0000-1000-8000-00805F9B34FB`. The device advertises as `Special Pie M1A2+`.

Implementation: `BLE-LoRa-Bridge/src/SpecialPieBleServer.cpp`

---

## Worked example — SHOT_DETECTED packet

Shot 3, absolute time 2450 ms, split 820 ms, session ID 1, model "SGTimer":

```
Offset  Field        Hex bytes
──────  ───────────  ─────────────────────────────────────────
 0      MAGIC        50 57
 2      TYPE         01
 3      SOURCE_ID    41 42 43 44 45 46        (e.g. "ABCDEF")
 9      sessionId    01 00 00 00
13      shotNumber   03 00
15      absoluteMs   92 09 00 00              (0x0992 = 2450)
19      splitMs      34 03 00 00              (0x0334 =  820)
23      isFirstShot  00
24      model        53 47 54 69 6D 65 72 00  "SGTimer\0"
        (padded)     00 00 00 00 00 00 00 00
──────  ───────────  ─────────────────────────────────────────
40      CRC16-lo     (computed)
41      CRC16-hi     (computed)
```

Total: 42 bytes — header (9) + payload (31) + CRC (2).
