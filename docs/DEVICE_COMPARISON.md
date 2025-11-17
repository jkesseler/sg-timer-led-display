# Device Comparison: SG Timer vs Special Pie Timer

This document compares the two supported shot timer devices and their BLE protocol implementations.

## Quick Comparison

| Feature | SG Timer | Special Pie M1A2+ |
|---------|----------|-------------------|
| **BLE Service UUID** | `7520FFFF-14D2-4CDA-8B6B-697C554C9311` | `0000FFF0-0000-1000-8000-00805F9B34FB` |
| **Notification Characteristic** | Custom SG UUID | `0000FFF1-0000-1000-8000-00805F9B34FB` |
| **Discovery Method** | By MAC address | By service UUID |
| **Message Format** | Length-prefixed TLV | Frame markers (F8 F9 ... F9 F8) |
| **Session Start** | ✅ Event 0x00 | ✅ Message 0x34 |
| **Session Stop** | ✅ Event 0x03 | ✅ Message 0x18 |
| **Session Suspend** | ✅ Event 0x01 | ❌ Not observed |
| **Session Resume** | ✅ Event 0x02 | ❌ Not observed |
| **Shot Detection** | ✅ Event 0x04 | ✅ Message 0x36 |
| **Shot List Retrieval** | ✅ Via BLE read | ❌ Not available |
| **Remote Start** | ✅ Possible | ❌ Not available |
| **Remote Stop** | ✅ Possible | ❌ Not available |
| **Split Time** | ✅ Included in event | 🔧 Client calculated |
| **Time Resolution** | Milliseconds | 10ms (centiseconds) |
| **Max Session Time** | ~49 days (uint32_t ms) | ~4.25 min (uint8_t sec) |

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

### No Events Received (SG Timer)
- Set timer to RO mode
- Check notification setup succeeded
- Verify service and characteristic UUIDs

### No Events Received (Special Pie Timer)
- Start a session on the timer
- Fire a shot to generate events
- Check notification callback registration

### Wrong Split Times (Special Pie Timer)
- Verify centisecond borrowing logic
- Check conversion factor (centiseconds × 10 = milliseconds)
- Ensure previous shot tracking is working

## Migration Notes

If migrating from single-device to multi-device:

1. Update `BUILD_AND_TEST.md` test procedures
2. Verify both test files work independently
3. Test auto-discovery logic
4. Validate normalized data from both sources
5. Check display rendering for both device types

## Performance Comparison

| Metric | SG Timer | Special Pie Timer |
|--------|----------|-------------------|
| **Connection Time** | ~3-5 seconds | ~2-4 seconds |
| **Event Latency** | < 50ms | < 100ms |
| **Message Size** | 8-30 bytes | 6-10 bytes |
| **Protocol Overhead** | Medium | Low |
| **Battery Impact** | Medium | Low |

## Future Enhancements

### SG Timer
- [ ] Implement remote session control
- [ ] Add shot list download UI
- [ ] Support session suspend/resume display

### Special Pie Timer
- [ ] Investigate FFF2 characteristic (write commands?)
- [ ] Decode unknown bytes in shot messages
- [ ] Explore device info service (09170001)

### Common
- [ ] Support simultaneous multi-device scanning
- [ ] Add device preference selection
- [ ] Implement device capability auto-detection
- [ ] Create unified test framework
