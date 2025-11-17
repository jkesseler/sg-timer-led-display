# AMG Lab Commander BLE Protocol Analysis

**Status**: 📋 Documented from Android app source code (needs validation with physical device)

**Source**: Reverse-engineered from [AmgLabCommander](https://github.com/DenisZhadan/AmgLabCommander)


## Device Overview

**Device Names:**
- Advertises as: `AMG LAB COMM*` or `COMMANDER*`
- Full device name format: `AMG LAB COMMANDER [MODEL]`
- Manufacturer: AMG Lab

**Connection Type:** BLE (Bluetooth Low Energy) with UART-like service

## BLE Services

### Nordic UART Service (NUS)
AMG Lab Commander uses the **Nordic UART Service** for BLE communication - a widely-used service for serial-like communication over BLE.

- **Service UUID:** `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- **Characteristics:**
  - `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` - TX (Write to device) ✅
  - `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` - RX (Notifications from device) ✅

### Device Descriptor
- **Descriptor UUID:** `00002902-0000-1000-8000-00805F9B34FB` (Client Characteristic Configuration)
  - Used to enable/disable notifications on the RX characteristic

## Protocol Overview

The AMG Lab Commander uses a **text-based command/response protocol** over BLE UART, unlike the binary protocols of SG Timer and Special Pie Timer.

### Connection Sequence

1. Scan for devices with name containing `AMG LAB COMM` or `COMMANDER`
2. Connect to device
3. Discover Nordic UART Service (`6E400001-B5A3-F393-E0A9-E50E24DCCA9E`)
4. Get TX characteristic (`6E400002`) for writing commands
5. Get RX characteristic (`6E400003`) for receiving notifications
6. Enable notifications on RX characteristic:
   ```cpp
   gatt->setCharacteristicNotification(rxCharacteristic, true);
   descriptor->setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
   gatt->writeDescriptor(descriptor);
   ```
7. Device is ready for commands

## Command Protocol (TX Characteristic)

Commands are sent as **UTF-8 encoded text strings** to the TX characteristic (`6E400002`).

### Command Format
```
<COMMAND_TEXT>
```

### Supported Commands

#### 1. Start Session/String
**Command:** `COM START`

**Purpose:** Start a new timing session

**Response:** Shot notifications via RX characteristic when shots are detected

---

#### 2. Request Screen Data (Hexadecimal)
**Command:** `REQ SCREEN HEX`

**Purpose:** Request current screen/display data from timer (implementation-specific)

**Response Format:** Unknown (not fully implemented in source)

---

#### 3. Request String Data (Hexadecimal)
**Command:** `REQ STRING HEX`

**Purpose:** Request timing data as hexadecimal string

**Response:** Binary data via RX characteristic (see Response Protocol section)

---

#### 4. Set Sensitivity
**Command:** `SET SENSITIVITY XX`

**Format:** `XX` is a two-digit number (01-10)

**Purpose:** Adjust microphone sensitivity for shot detection

**Example:** `SET SENSITIVITY 05`

**Response:** Unknown (not observed in source)

---

## Response Protocol (RX Characteristic)

Responses are received via **notifications** on the RX characteristic (`6E400003`).

### Response Data Format

All responses are **binary byte arrays**. The first byte indicates the message type.

### Message Types

#### Type 1: Shot Events and Timer Status
**Byte 0:** `0x01`

This is the primary message type for timing data. The second byte indicates the specific event:

##### Shot Event (Push Notification)
**Byte 1:** `0x03`

**Format:**
```
[01] [03] [SH_H] [SH_L] [TM_H] [TM_L] [SP_H] [SP_L] [FS_H] [FS_L] [??_H] [??_L] [SE_H] [SE_L]
```

**Total Length:** 14 bytes

**Byte Positions (0-indexed):**
- `bytes[0]` = `0x01` - Message type (Type 1)
- `bytes[1]` = `0x03` - Shot event indicator
- `bytes[2-3]` = Shot number (16-bit big-endian)
- `bytes[4-5]` = Current time in centiseconds (16-bit)
- `bytes[6-7]` = Split time in centiseconds (16-bit)
- `bytes[8-9]` = First shot time in centiseconds (16-bit)
- `bytes[10-11]` = Unknown purpose
- `bytes[12-13]` = Series/batch number (16-bit)

**Time Conversion:**
```java
// Convert two bytes to time value
int rawValue = 256 * byte1 + byte2;
if (byte2 <= 0) {
    rawValue += 256;  // Handle sign extension
}
double timeSeconds = rawValue / 100.0;
```

**Conversion to milliseconds:**
```cpp
uint32_t timeMs = static_cast<uint32_t>(timeSeconds * 1000.0);
```

**Example:**
```
Raw: 01 03 00 05 04 B0 01 2C 01 90 00 00 00 01
Interpretation:
- Message type: 0x01
- Event type: 0x03 (shot)
- Shot number: 0x0005 = 5
- Time: 0x04B0 = 1200 centiseconds = 12.00 seconds
- Split: 0x012C = 300 centiseconds = 3.00 seconds
- First shot: 0x0190 = 400 centiseconds = 4.00 seconds
- Series: 0x0001 = 1
```

##### Timer Start Event
**Byte 1:** `0x05`

**Format:**
```
[01] [05]
```

**Total Length:** 2 bytes

**Purpose:** Indicates timer has started waiting for first shot

---

##### Timer Stop/Waiting Event
**Byte 1:** `0x08`

**Format:**
```
[01] [08]
```

**Total Length:** 2 bytes

**Purpose:** Indicates timer has stopped or is in waiting mode

---

#### Type 10-26: Screen/String Data Response
**Byte 0:** `0x0A` to `0x1A` (10-26 decimal)

**Format:**
```
[TYPE] [COUNT] [SHOT1_H] [SHOT1_L] [SHOT2_H] [SHOT2_L] ... [SHOTN_H] [SHOTN_L]
```

**Byte Positions:**
- `bytes[0]` = Message type (0x0A-0x1A)
- `bytes[1]` = Number of shot entries in this packet
- `bytes[2+i*2]` and `bytes[3+i*2]` = Shot time in centiseconds (for shot i)

**Notes:**
- Type `0x0A` (10) indicates the **first packet** of a multi-packet shot sequence
- When type is 10, the app clears previous shot data
- Data is sent in chunks, potentially across multiple notifications
- Each shot time is encoded as a 16-bit value (2 bytes)

**Example:**
```
Raw: 0A 03 00 64 01 2C 02 58
Interpretation:
- Type: 0x0A (first packet)
- Count: 3 shots in this packet
- Shot 1: 0x0064 = 100 cs = 1.00 seconds
- Shot 2: 0x012C = 300 cs = 3.00 seconds (split = 2.00s)
- Shot 3: 0x0258 = 600 cs = 6.00 seconds (split = 3.00s)
```

---

## Data Structures

### Shot Data (Normalized)

```cpp
struct NormalizedShotData {
  uint32_t sessionId;           // Derived from series/batch number
  uint16_t shotNumber;          // From bytes[2-3]
  uint32_t absoluteTimeMs;      // From bytes[4-5], converted to ms
  uint32_t splitTimeMs;         // From bytes[6-7], converted to ms
  uint64_t timestampMs;         // System timestamp when received
  const char* deviceModel;      // "AMG Lab Commander"
  bool isFirstShot;             // shotNumber == 1
};
```

### Session Data

```cpp
struct SessionData {
  uint32_t sessionId;           // From series number (bytes[12-13])
  uint64_t startTimestamp;      // System timestamp when session started
};
```

## Protocol Implementation Details

### Time Encoding/Decoding

The AMG Lab Commander encodes time as **centiseconds** (1/100th of a second) in a 16-bit big-endian format with special handling:

```cpp
// Decode function (from Java source)
double convertData(uint8_t byte1, uint8_t byte2) {
  int16_t value = 256 * byte1 + byte2;
  if (byte2 <= 0) {
    value += 256;  // Correct for negative interpretation
  }
  return value / 100.0;  // Convert centiseconds to seconds
}
```

**For ESP32 implementation:**
```cpp
uint32_t decodeTime(uint8_t highByte, uint8_t lowByte) {
  int16_t centiseconds = (highByte << 8) | lowByte;
  if (lowByte <= 0) {
    centiseconds += 256;
  }
  return static_cast<uint32_t>(centiseconds * 10);  // Convert to milliseconds
}
```

### Shot Number Handling

- Shot numbers appear to be **0-indexed** or **1-indexed** depending on context
- First shot typically has shot number `0x0001` (1)
- Shot sequence is cumulative within a session

### Split Time Handling

**Unlike Special Pie Timer, AMG Lab Commander provides split times directly:**

- Split time is sent in the shot event message (`bytes[6-7]`)
- No client-side calculation required
- Split time represents time since previous shot
- For first shot, split time equals absolute time

### Session Management

- Sessions are tracked by series/batch number (`bytes[12-13]`)
- No explicit session start/stop events observed
- Timer start event (`0x01 0x05`) indicates readiness
- Timer stop event (`0x01 0x08`) indicates end of active timing

## Device Capabilities

| Feature | Supported | Notes |
|---------|-----------|-------|
| Shot detection notifications | ✅ Yes | Via RX characteristic, Type 1 Byte 3 |
| Real-time shot data | ✅ Yes | Time, split, first shot included |
| Session tracking | ✅ Yes | Via series number in shot events |
| Timer start notification | ✅ Yes | Type 1 Byte 5 |
| Timer stop notification | ✅ Yes | Type 1 Byte 8 |
| Remote session start | ✅ Yes | `COM START` command |
| Remote session stop | ❓ Unknown | Not observed in source |
| Shot list retrieval | ✅ Yes | `REQ STRING HEX` command |
| Sensitivity adjustment | ✅ Yes | `SET SENSITIVITY XX` command |
| Split time calculation | ✅ Client + Device | Device provides split time directly |
| First shot tracking | ✅ Yes | Included in shot event data |

## Comparison with Other Timers

### vs. SG Timer Sport/GO
- **Protocol Style:** Text commands vs. Binary protocol
- **Time Format:** Centiseconds (big-endian) vs. Seconds + hundredths
- **Split Times:** Provided by device vs. Client-side calculation
- **Remote Control:** Basic vs. Full session control
- **Service:** Nordic UART vs. Custom service

### vs. Special Pie M1A2+
- **Protocol Style:** Text commands vs. Binary frames (0xF8/0xF9)
- **Time Format:** Centiseconds (16-bit) vs. Seconds + centiseconds (separate bytes)
- **Split Times:** Provided by device vs. Client-side calculation required
- **Remote Control:** Basic start command vs. None
- **Service:** Nordic UART vs. Custom service (FFF0)

## Implementation Notes for ESP32

### 1. Service Discovery
```cpp
static const char* SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* TX_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";  // Write
static const char* RX_CHAR_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";  // Notify
static const char* DESCRIPTOR_UUID = "00002902-0000-1000-8000-00805F9B34FB";
```

### 2. Sending Commands
```cpp
bool sendCommand(const char* command) {
  BLERemoteService* pService = pClient->getService(SERVICE_UUID);
  if (!pService) return false;

  BLERemoteCharacteristic* pTxChar = pService->getCharacteristic(TX_CHAR_UUID);
  if (!pTxChar) return false;

  pTxChar->writeValue(command, strlen(command));
  return true;
}
```

### 3. Processing Notifications
```cpp
void notifyCallback(BLERemoteCharacteristic* pChar,
                   uint8_t* pData, size_t length, bool isNotify) {
  if (length < 2) return;

  uint8_t messageType = pData[0];

  if (messageType == 0x01) {
    uint8_t eventType = pData[1];

    if (eventType == 0x03 && length >= 14) {
      // Shot event
      uint16_t shotNumber = (pData[2] << 8) | pData[3];
      uint32_t timeMs = decodeTime(pData[4], pData[5]);
      uint32_t splitMs = decodeTime(pData[6], pData[7]);
      uint32_t firstShotMs = decodeTime(pData[8], pData[9]);
      uint16_t seriesNumber = (pData[12] << 8) | pData[13];

      // Create NormalizedShotData and invoke callback
    }
    else if (eventType == 0x05) {
      // Timer started
    }
    else if (eventType == 0x08) {
      // Timer stopped
    }
  }
  else if (messageType >= 0x0A && messageType <= 0x1A) {
    // Shot list response
    uint8_t shotCount = pData[1];
    if (messageType == 0x0A) {
      // Clear previous shot sequence
    }
    for (int i = 0; i < shotCount; i++) {
      uint32_t shotTime = decodeTime(pData[2 + i*2], pData[3 + i*2]);
      // Store shot data
    }
  }
}
```

### 4. Device Detection
```cpp
bool isAMGLabCommander(const std::string& deviceName) {
  std::string upperName = deviceName;
  std::transform(upperName.begin(), upperName.end(),
                upperName.begin(), ::toupper);

  return upperName.find("AMG LAB COMM") != std::string::npos ||
         upperName.find("COMMANDER") != std::string::npos;
}
```

### 5. Time Conversion
```cpp
uint32_t decodeTime(uint8_t highByte, uint8_t lowByte) {
  int16_t centiseconds = (highByte << 8) | lowByte;

  // Handle sign extension quirk from original Java code
  if (lowByte <= 0) {
    centiseconds += 256;
  }

  // Convert centiseconds to milliseconds
  return static_cast<uint32_t>(centiseconds * 10);
}
```

## Known Limitations & Unknowns

### Known Limitations
- ❌ No explicit session stop command observed
- ❌ Purpose of bytes[10-11] in shot events unknown
- ❌ Maximum time range limited by 16-bit centiseconds (~655 seconds)
- ❌ Screen data format not documented
- ❌ Sensitivity range/optimal values unknown

### Unknowns/Needs Validation
- **Sign extension logic** in time decoding - seems unusual, needs testing
- **Session ID behavior** - how series number increments
- **Screen data response format** - `REQ SCREEN HEX` not fully documented
- **Error responses** - no error handling observed
- **Connection parameters** - optimal BLE connection intervals
- **Multiple packet handling** - for shot list responses types 0x0A-0x1A

## Testing Requirements

### Pre-Implementation Testing Needed
1. ✅ Validate service UUIDs with physical device
2. ✅ Confirm command format and responses
3. ✅ Test time decoding with known shot times
4. ✅ Verify sign extension logic necessity
5. ✅ Test shot list retrieval format
6. ✅ Validate session tracking behavior
7. ✅ Test sensitivity adjustment range

### Recommended Test Sequence
1. Scan and connect to device
2. Enable notifications on RX characteristic
3. Send `COM START` command
4. Record shot events and validate time decoding
5. Test `REQ STRING HEX` command for shot list
6. Test sensitivity adjustment commands
7. Validate session transitions

## Implementation Priority

### Phase 1: Basic Shot Detection (MVP)
- ✅ BLE connection and service discovery
- ✅ Command sending (TX characteristic)
- ✅ Notification reception (RX characteristic)
- ✅ Type 1 Byte 3 (shot events) parsing
- ✅ Time decoding to milliseconds
- ✅ NormalizedShotData conversion

### Phase 2: Session Management
- ✅ Timer start/stop events (Type 1 Byte 5/8)
- ✅ Session tracking via series number
- ✅ Session callbacks

### Phase 3: Advanced Features
- ✅ Shot list retrieval (`REQ STRING HEX`)
- ✅ Sensitivity adjustment commands
- ❓ Screen data requests (if needed)

## Reference Implementation

**To be created:**
- `ESP32-S3-firmware/include/AMGLabCommanderDevice.h`
- `ESP32-S3-firmware/src/AMGLabCommanderDevice.cpp`
- `ESP32-S3-firmware/test/amg-lab-commander.cpp` (protocol validation)

**Reference Android app source:**
- https://github.com/DenisZhadan/AmgLabCommander/tree/master/app/src/main/java/eu/sparksoft/amglabcommander/MainActivity.java
- https://github.com/DenisZhadan/AmgLabCommander/tree/master/app/src/main/java/eu/sparksoft/amglabcommander/CommanderBluetoothGattCallback.java

## Additional Resources

- **Nordic UART Service Specification**: [Nordic Semiconductor Documentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/libraries/bluetooth_services/services/nus.html)
- **Android BLE API Reference**: Used by original app implementation
- **Original App Package**: `eu.sparksoft.amglabcommander`

---

**Documentation Status**: 📋 Ready for implementation - requires physical device validation

**Last Updated**: November 17, 2025
