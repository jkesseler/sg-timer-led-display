# Device Comparison: Shot Timer Devices

This document compares the supported shot timer devices and their BLE protocol implementations.

## Quick Comparison

| Feature | SG Timer | Special Pie M1A2+ | AMG Lab Commander |
|---------|----------|-------------------|-------------------|
| **BLE Service UUID** | `7520FFFF-14D2-4CDA-8B6B-697C554C9311` | `0000FFF0-0000-1000-8000-00805F9B34FB` | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` (Nordic UART) |
| **Notification Characteristic** | Custom SG UUID | `0000FFF1-0000-1000-8000-00805F9B34FB` | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` |
| **Discovery Method** | By MAC address | By service UUID | By device name pattern |
| **Message Format** | Length-prefixed TLV | Frame markers (F8 F9 ... F9 F8) | Text commands + Binary responses |
| **Protocol Style** | Binary | Binary frames | Hybrid (text TX, binary RX) |
| **Session Start** | ✅ Event 0x00 | ✅ Message 0x34 | ✅ Type 1 Byte 5 |
| **Session Stop** | ✅ Event 0x03 | ✅ Message 0x18 | ✅ Type 1 Byte 8 |
| **Session Suspend** | ✅ Event 0x01 | ❌ Not observed | ❌ Not observed |
| **Session Resume** | ✅ Event 0x02 | ❌ Not observed | ❌ Not observed |
| **Shot Detection** | ✅ Event 0x04 | ✅ Message 0x36 | ✅ Type 1 Byte 3 |
| **Shot List Retrieval** | ✅ Via BLE read | ❌ Not available | ✅ `REQ STRING HEX` |
| **Remote Start** | ✅ Possible | ❌ Not available | ✅ `COM START` |
| **Remote Stop** | ✅ Possible | ❌ Not available | ❓ Unknown |
| **Split Time** | ✅ Included in event | 🔧 Client calculated | ✅ Included in event |
| **Time Resolution** | Milliseconds | 10ms (centiseconds) | 10ms (centiseconds) |
| **Time Encoding** | Milliseconds (uint32) | Seconds + CS (2 bytes) | Centiseconds (uint16 BE) |
| **Max Session Time** | ~49 days (uint32_t ms) | ~4.25 min (uint8_t sec) | ~10.9 min (uint16_t cs) |
| **Sensitivity Control** | ❓ Unknown | ❌ Not available | ✅ `SET SENSITIVITY` |

## Protocol Details

### SG Timer

**Protocol**: Custom TLV (Type-Length-Value)

**Message Structure**:
```
[LENGTH] [EVENT_ID] [SESSION_ID (4 bytes)] [EVENT_DATA...] [CHECKSUM]
```

**Example Session Start**:
```
06 00 65 7a 8b 29 00 1e
└─ Length: 6 bytes
   └─ Event: 0x00 (SESSION_START)
      └─ Session ID: 0x65 7a 8b 29
         └─ Start delay: 0x00 0x1e (3.0s)
```

**Example Shot**:
```
0a 04 65 7a 8b 29 00 01 00 00 09 29
└─ Length: 10 bytes
   └─ Event: 0x04 (SHOT_DETECTED)
      └─ Session ID: 0x65 7a 8b 29
         └─ Shot number: 0x00 0x01 (1)
            └─ Time: 0x00 0x00 0x09 0x29 (2345ms)
```

**Capabilities**:
- Full bidirectional control
- Shot list download after session
- Suspend/resume session
- Precise millisecond timing
- Long session support (hours)

**Reference**: `docs/sg-timer-reference/sg_timer_public_bt_api_32.md`

### Special Pie M1A2+ Timer

**Protocol**: Frame-based messaging

**Message Structure**:
```
[F8] [F9] [MESSAGE_TYPE] [DATA...] [F9] [F8]
```

**Example Session Start**:
```
F8 F9 34 03 F9 F8
└─ Frame start: F8 F9
   └─ Message: 0x34 (SESSION_START)
      └─ Session ID: 0x03
         └─ Frame end: F9 F8
```

**Example Shot**:
```
F8 F9 36 00 04 24 02 0C F9 F8
└─ Frame start: F8 F9
   └─ Message: 0x36 (SHOT_DETECTED)
      └─ Reserved: 0x00
         └─ Seconds: 0x04 (4)
            └─ Centiseconds: 0x24 (36)
               └─ Shot #: 0x02 (2)
                  └─ Unknown: 0x0C
                     └─ Frame end: F9 F8
```

**Capabilities**:
- Shot detection only
- Session start/stop notifications
- Simple notification-based protocol
- Client must calculate splits
- Limited to ~4 minute sessions

**Reference**: `docs/special-pie-timer-reference/BLE-Protocol-Analysis.md`

### AMG Lab Commander

**Protocol**: Text commands + Binary responses (Nordic UART Service)

**Message Structure (Commands)**:
```
<TEXT_COMMAND>
```

**Message Structure (Responses)**:
```
[TYPE] [EVENT] [DATA...]
```

**Example Session Start Command**:
```
COM START
└─ UTF-8 text command
```

**Example Shot Response**:
```
01 03 00 05 04 B0 01 2C 01 90 00 00 00 01
└─ Type: 0x01 (Timer event)
   └─ Event: 0x03 (Shot)
      └─ Shot #: 0x00 0x05 (5)
         └─ Time: 0x04 0xB0 (1200cs = 12.00s)
            └─ Split: 0x01 0x2C (300cs = 3.00s)
               └─ First: 0x01 0x90 (400cs = 4.00s)
                  └─ Unknown: 0x00 0x00
                     └─ Series: 0x00 0x01 (1)
```

**Capabilities**:
- Bidirectional text commands
- Shot list retrieval
- Remote session start
- Sensitivity adjustment
- Direct split time provision
- Multi-packet shot sequences

**Reference**: `docs/amg-lab-commander-reference/BLE-Protocol-Analysis.md`

## Implementation Architecture

Both devices implement the `ITimerDevice` interface:

```cpp
class ITimerDevice {
  // Common interface
  virtual bool initialize() = 0;
  virtual bool connect(BLEAddress address) = 0;
  virtual bool isConnected() const = 0;

  // Event callbacks
  virtual void onShotDetected(std::function<void(const NormalizedShotData&)> callback) = 0;
  virtual void onSessionStarted(std::function<void(const SessionData&)> callback) = 0;
  virtual void onSessionStopped(std::function<void(const SessionData&)> callback) = 0;

  // Capability queries
  virtual bool supportsRemoteStart() const = 0;
  virtual bool supportsShotList() const = 0;
  virtual bool supportsSessionControl() const = 0;
};
```

### SGTimerDevice

- **Location**: `ESP32-S3-firmware/src/SGTimerDevice.cpp`
- **Discovery**: Hardcoded MAC address (`dd:0e:9d:04:72:c3`)
- **Features**: Full protocol support
- **Test File**: `ESP32-S3-firmware/test/sg-timer.cpp`

### SpecialPieTimerDevice

- **Location**: `ESP32-S3-firmware/src/SpecialPieTimerDevice.cpp`
- **Discovery**: Service UUID advertisement
- **Features**: Notification-only support
- **Test File**: `ESP32-S3-firmware/test/special-pie-timer.cpp` ✅ **Validated**

### AMGLabCommanderDevice

- **Location**: `ESP32-S3-firmware/src/AMGLabCommanderDevice.cpp` (to be created)
- **Discovery**: Device name pattern matching
- **Features**: Text command protocol with binary responses
- **Test File**: `ESP32-S3-firmware/test/amg-lab-commander.cpp` (to be created)

## Testing Strategy

### Testing SG Timer

1. **Preparation**:
   - Ensure SG Timer MAC address is correct in `TimerApplication.cpp`
   - Turn on SG Timer
   - Set timer to RO (Review Officer) mode

2. **Expected Behavior**:
   - Auto-connects by MAC address
   - Receives all session events (start/stop/suspend/resume)
   - Downloads shot list after session ends
   - Supports remote control (if implemented)

3. **Validation**:
   - Check shot times match timer display
   - Verify shot list completeness
   - Test reconnection after power cycle

### Testing Special Pie Timer

1. **Preparation**:
   - Turn on Special Pie Timer
   - No special mode required

2. **Expected Behavior**:
   - Auto-connects by service UUID
   - Receives session start (0x34)
   - Receives shot events (0x36)
   - Receives session stop (0x18)
   - Client calculates split times

3. **Validation**:
   - Check absolute times match timer display
   - Verify split time calculations
   - Test reconnection after power cycle
   - Confirm session IDs match between start/stop

### Testing AMG Lab Commander

1. **Preparation**:
   - Turn on AMG Lab Commander timer
   - No special mode required

2. **Expected Behavior**:
   - Auto-connects by device name pattern
   - Can send `COM START` command
   - Receives timer start event (Type 1 Byte 5)
   - Receives shot events (Type 1 Byte 3)
   - Receives timer stop event (Type 1 Byte 8)
   - Split times provided by device

3. **Validation**:
   - Check absolute times match timer display
   - Verify split times match timer display
   - Test time decoding (centiseconds to milliseconds)
   - Test `REQ STRING HEX` command for shot list
   - Validate sensitivity adjustment if implemented
   - Test reconnection after power cycle

## Common Features

Both implementations provide:

- **Normalized Data**: All shot data converted to `NormalizedShotData` structure
- **Callback System**: Event-driven architecture
- **Connection Management**: Auto-reconnect on disconnect
- **State Tracking**: Session active/inactive
- **Error Handling**: Frame validation and error recovery

## Normalized Shot Data

Both devices output standardized shot data:

```cpp
struct NormalizedShotData {
  uint32_t sessionId;        // Session identifier
  uint16_t shotNumber;       // Shot number (0-indexed or 1-indexed)
  uint32_t absoluteTimeMs;   // Absolute time in milliseconds
  uint32_t splitTimeMs;      // Split time in milliseconds
  uint32_t timestampMs;      // Local timestamp when received
  const char* deviceModel;   // "SG Timer" or "Special Pie Timer"
  bool isFirstShot;          // True if first shot in session
};
```

## Multi-Device Support

`TimerApplication` scans for both device types:

1. Scan for BLE devices
2. Check for SG Timer (by address)
3. Check for Special Pie Timer (by service UUID)
4. Create appropriate device implementation
5. Set up callbacks
6. Connect to device

Only one device can be connected at a time.

## Troubleshooting

### SG Timer Not Found
- Verify MAC address in `TimerApplication::scanForDevices()`
- Check timer is powered on
- Ensure timer is in range (< 10m)
- Verify timer battery level

### Special Pie Timer Not Found
- Ensure service UUID is correct: `0000FFF0-0000-1000-8000-00805F9B34FB`
- Check timer is advertising (turn off/on)
- Move closer to device
- Check for BLE interference

### AMG Lab Commander Not Found
- Verify device name starts with "AMG LAB COMM" or "COMMANDER"
- Check timer is powered on and advertising
- Ensure Nordic UART Service UUID is correct: `6E400001-...`
- Move closer to device (< 10m)
- Try power cycling the timer

### No Events Received (SG Timer)
- Set timer to RO mode
- Check notification setup succeeded
- Verify service and characteristic UUIDs

### No Events Received (Special Pie Timer)
- Start a session on the timer
- Fire a shot to generate events
- Check notification callback registration

### No Events Received (AMG Lab Commander)
- Send `COM START` command first
- Check RX characteristic notification enabled
- Verify descriptor write succeeded
- Start a session and fire a shot

### Wrong Split Times (Special Pie Timer)
- Verify centisecond borrowing logic
- Check conversion factor (centiseconds × 10 = milliseconds)
- Ensure previous shot tracking is working

### Wrong Times (AMG Lab Commander)
- Verify sign extension logic in time decoding
- Check big-endian byte order (high byte first)
- Validate conversion: centiseconds × 10 = milliseconds
- Test with known shot times from timer display

## Migration Notes

If migrating from single-device to multi-device:

1. Update `BUILD_AND_TEST.md` test procedures
2. Verify all test files work independently
3. Test auto-discovery logic for all device types
4. Validate normalized data from all sources
5. Check display rendering for all device types
6. Test device priority/selection logic

## Performance Comparison

| Metric | SG Timer | Special Pie Timer | AMG Lab Commander |
|--------|----------|-------------------|-------------------|
| **Connection Time** | ~3-5 seconds | ~2-4 seconds | ~2-4 seconds |
| **Event Latency** | < 50ms | < 100ms | ~50-150ms |
| **Message Size** | 8-30 bytes | 6-10 bytes | 2-14 bytes |
| **Protocol Overhead** | Medium | Low | Low-Medium |
| **Battery Impact** | Medium | Low | Low-Medium |
| **Command Support** | ✅ Full | ❌ None | ✅ Text-based |

## Protocol Complexity

| Aspect | SG Timer | Special Pie Timer | AMG Lab Commander |
|--------|----------|-------------------|-------------------|
| **Learning Curve** | High | Low | Medium |
| **Implementation** | Complex TLV parsing | Simple frame validation | Hybrid text/binary |
| **Debugging** | Moderate | Easy | Easy-Moderate |
| **Extensibility** | Good (documented API) | Limited | Good (text commands) |
| **Documentation** | ✅ Official API doc | 🔧 Reverse-engineered | 🔧 From Android app |

## Future Enhancements

### SG Timer
- [ ] Implement remote session control
- [ ] Add shot list download UI
- [ ] Support session suspend/resume display

### Special Pie Timer
- [ ] Investigate FFF2 characteristic (write commands?)
- [ ] Decode unknown bytes in shot messages
- [ ] Explore device info service (09170001)

### AMG Lab Commander
- [ ] Validate with physical device
- [ ] Test time decoding sign extension logic
- [ ] Implement shot list retrieval UI
- [ ] Add sensitivity adjustment UI
- [ ] Decode screen data format
- [ ] Test remote session stop

### Common
- [ ] Support simultaneous multi-device scanning
- [ ] Add device preference selection
- [ ] Implement device capability auto-detection
- [ ] Create unified test framework
- [ ] Add device auto-reconnect preferences
