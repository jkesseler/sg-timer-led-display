# Special Pie M1A2+ BLE Protocol Analysis

**Status**: ✅ Validated with working test implementation (`test/special-pie-timer.cpp`)

## BLE Services

### Primary Timer Service
- **Service UUID:** `FFF0` (`0000FFF0-0000-1000-8000-00805F9B34FB`)
- **Characteristics:**
  - `FFF1` (`0000FFF1-0000-1000-8000-00805F9B34FB`) - Timer events (notify enabled) ✅
  - `FFF2` (`0000FFF2-0000-1000-8000-00805F9B34FB`) - Purpose unknown

### Device Information Service
- **Service UUID:** `0917FE11-5D37-816D-8000-00805F9B34FB`
- **Characteristics:**
  - `09170001-5D37-816D-8000-00805F9B34FB` - Purpose unknown
  - `09170002-5D37-816D-8000-00805F9B34FB` - Firmware version (read) ✅
    - Returns firmware version string (e.g., `app-1.2.1-20240719.0000`)

## Message Structure

All messages follow this pattern:

```
[F8] [F9] [MESSAGE_TYPE] [DATA...] [F9] [F8]
```

**Frame Markers**:
- Start: `0xF8 0xF9`
- End: `0xF9 0xF8`
- Minimum message length: 6 bytes

## Message Types

### 1. Shot Detected Event ✅ VALIDATED

**Message Type:** `0x36` (54 decimal)

**Format:**
```
F8 F9 36 00 [SEC] [CS] [SHOT#] [CHECKSUM?] F9 F8
```

**Total Length:** 10 bytes

**Byte Positions (0-indexed):**
- `pData[0]` = `0xF8` - Start frame marker
- `pData[1]` = `0xF9` - Start frame marker
- `pData[2]` = `0x36` (54 decimal) - Shot event identifier
- `pData[3]` = `0x00` - Unknown/reserved
- `pData[4]` = Seconds component of time (0-255)
- `pData[5]` = Centiseconds (0-99) - Each unit = 10ms
- `pData[6]` = Shot number (0-indexed or 1-indexed depending on session)
- `pData[7]` = Unknown (possibly checksum)
- `pData[8]` = `0xF9` - End frame marker
- `pData[9]` = `0xF8` - End frame marker

**Time Calculation:**
- Absolute time (ms) = (Seconds × 1000) + (Centiseconds × 10)
- Split time = Current time - Previous time (with centisecond borrowing if needed)

**Examples:**

| Raw Data                          | Shot # | Seconds | CS  | Time (s) | Notes                    |
|-----------------------------------|--------|---------|-----|----------|--------------------------|
| `F8 F9 36 00 04 24 02 0C F9 F8`  | 2      | 4       | 36  | 4.36     | 36 centiseconds = 360ms  |
| `F8 F9 36 00 2A 3B 05 0C F9 F8`  | 5      | 42      | 59  | 42.59    | 59 centiseconds = 590ms  |
| `F8 F9 36 00 4F 33 06 0C F9 F8`  | 6      | 79      | 51  | 79.51    |                          |
| `F8 F9 36 00 9D 29 0B 0C F9 F8`  | 11     | 157     | 41  | 157.41   |                          |
| `F8 F9 36 00 C2 2B 0D 0C F9 F8`  | 13     | 194     | 43  | 194.43   |                          |

### 2. Session Start Event ✅ VALIDATED

**Message Type:** `0x34` (52 decimal)

**Format:**
```
F8 F9 34 [SESSION_ID] F9 F8
```

**Total Length:** 6 bytes

**Byte Positions (0-indexed):**
- `pData[0]` = `0xF8` - Start frame marker
- `pData[1]` = `0xF9` - Start frame marker
- `pData[2]` = `0x34` (52 decimal) - Session start identifier
- `pData[3]` = Session ID (sequential counter: 0x03, 0x04, 0x05...)
- `pData[4]` = `0xF9` - End frame marker
- `pData[5]` = `0xF8` - End frame marker

**Examples:**

| Raw Data              | Session ID | Description      |
|-----------------------|------------|------------------|
| `F8 F9 34 03 F9 F8`   | 3          | Session start    |
| `F8 F9 34 0C F9 F8`   | 12         | Session start    |

**Implementation Notes:**
- Reset shot tracking when session starts
- Clear previous shot time/split calculations
- Session ID links start and stop events

### 3. Session Stop Event ✅ VALIDATED

**Message Type:** `0x18` (24 decimal)

**Format:**
```
F8 F9 18 [SESSION_ID] F9 F8
```

**Total Length:** 6 bytes

**Byte Positions (0-indexed):**
- `pData[0]` = `0xF8` - Start frame marker
- `pData[1]` = `0xF9` - Start frame marker
- `pData[2]` = `0x18` (24 decimal) - Session stop identifier
- `pData[3]` = Session ID (matches corresponding session start)
- `pData[4]` = `0xF9` - End frame marker
- `pData[5]` = `0xF8` - End frame marker

**Examples:**

| Raw Data              | Session ID | Description      |
|-----------------------|------------|------------------|
| `F8 F9 18 0B F9 F8`   | 11         | Session stop     |
| `F8 F9 18 0C F9 F8`   | 12         | Session stop     |

**Implementation Notes:**
- Clear session active flag
- Reset shot tracking variables

## Protocol Implementation Notes

### Split Time Calculation ✅ VALIDATED

Split times must be calculated client-side by tracking previous shot times:

```cpp
// Track previous shot
uint32_t previousTimeSeconds = 0;
uint32_t previousTimeCentiseconds = 0;
bool hasPreviousShot = false;

// On shot received:
uint32_t currentSeconds = pData[4];
uint32_t currentCentiseconds = pData[5];

if (hasPreviousShot) {
  int32_t deltaSeconds = currentSeconds - previousTimeSeconds;
  int32_t deltaCentiseconds = currentCentiseconds - previousTimeCentiseconds;

  // Handle negative centiseconds (borrow from seconds)
  if (deltaCentiseconds < 0) {
    deltaSeconds -= 1;
    deltaCentiseconds += 100;
  }

  // Convert to milliseconds
  uint32_t splitTimeMs = (deltaSeconds * 1000) + (deltaCentiseconds * 10);
}

// Store for next calculation
previousTimeSeconds = currentSeconds;
previousTimeCentiseconds = currentCentiseconds;
hasPreviousShot = true;
```

### Connection Sequence ✅ VALIDATED

1. Scan for devices advertising service `0000FFF0-0000-1000-8000-00805F9B34FB`
2. Connect to device
3. Discover Timer Service (`FFF0`)
4. Get characteristic `FFF1` (timer events)
5. Verify characteristic can notify (`canNotify()`)
6. Register notification callback
7. Optionally read firmware version from `09170002` characteristic
8. Wait for timer events

### Frame Validation ✅ VALIDATED

Always validate frames before processing:

```cpp
// Check minimum length
if (length < 6) return;

// Validate frame markers
if (pData[0] != 0xF8 || pData[1] != 0xF9) return;
if (pData[length - 2] != 0xF9 || pData[length - 1] != 0xF8) return;

// Extract message type
uint8_t messageType = pData[2];
```

### Time Format

- **Seconds**: Single byte (0-255), limits max time to 255 seconds (~4.25 minutes)
- **Centiseconds**: Single byte (0-99), each unit = 10 milliseconds
- **Resolution**: 10ms (0.01 seconds)
- **Conversion to ms**: `(seconds × 1000) + (centiseconds × 10)`

## Limitations & Unknown Features

### Known Limitations
- ❌ No remote session start/stop
- ❌ No shot list retrieval
- ❌ No session control from client
- ❌ Maximum time limited to 255 seconds
- ❌ Session suspend/resume not observed in protocol

### Unknown/Unimplemented
- **Characteristic `09170001-5D37-816D-8000-00805F9B34FB`** - Purpose unknown
- **Characteristic `FFF2`** - Purpose unknown (possibly for write commands?)
- **Byte `pData[3]`** in shot events - Always `0x00`, purpose unknown
- **Byte `pData[7]`** in shot events - Possibly checksum/CRC, not validated

## Device Capabilities

| Feature | Supported | Notes |
|---------|-----------|-------|
| Shot detection notifications | ✅ Yes | Via FFF1 characteristic |
| Session start notifications | ✅ Yes | Message type 0x34 |
| Session stop notifications | ✅ Yes | Message type 0x18 |
| Split time calculation | ✅ Yes | Client-side only |
| Remote session start | ❌ No | Must use physical button |
| Remote session stop | ❌ No | Must use physical button |
| Shot list retrieval | ❌ No | No characteristic available |
| Firmware version read | ✅ Yes | Via 09170002 characteristic |

## Reference Implementation

See `ESP32-S3-firmware/test/special-pie-timer.cpp` for a complete, tested implementation.

See `ESP32-S3-firmware/src/SpecialPieTimerDevice.cpp` for production implementation with ITimerDevice interface.
