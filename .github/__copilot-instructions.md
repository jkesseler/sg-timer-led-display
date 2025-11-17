# SG Timer LED Display Bridge - AI Coding Instructions

## Project Overview

ESP32-based BLE bridge connecting shot timer devices to HUB75 LED matrix displays. Uses **Facade Pattern** for multi-device support with device-agnostic application layer.

**Key Technologies:** ESP32-S3 (Arduino framework), PlatformIO, BLE (NimBLE), HUB75 LED matrices, C++11

## Architecture Fundamentals

### Facade Pattern Structure
```
ITimerDevice (abstract)
├── SGTimerDevice (implemented - SG Timer Sport/GO)
└── [Future devices follow same interface]
     ↓
TimerApplication (device-agnostic)
     ↓
DisplayManager + SystemStateMachine + BrightnessController
```

**Critical:** `main.cpp` contains only 20 lines - all logic lives in `TimerApplication`. Application code **never** references device-specific classes directly.

### Data Normalization
All device implementations convert to standardized structures:
- **Time:** Always `uint32_t` milliseconds (SG Timer native, Special Pie converts from centiseconds)
- **Shot Data:** `NormalizedShotData` with `absoluteTimeMs`, `splitTimeMs`, `sessionId`
- **Sessions:** `SessionData` with unix timestamp IDs, active state, shot counts

### State Machine (Phase 1 Implemented)
22 distinct states in `SystemStateMachine.h`:
- **Connection:** SEARCHING → CONNECTING → CONNECTED → [RECONNECTING on error]
- **Session:** IDLE → SESSION_STARTING → SESSION_ACTIVE → [SHOT_DETECTED] → SESSION_ENDED
- **Error Recovery:** CONNECTION_ERROR, DEVICE_ERROR, SYSTEM_ERROR → RECOVERY → previous state

State transitions use formal validation with timeouts. Button press from any state → MANUAL_RESET → SEARCHING_FOR_DEVICES.

## Directory Structure

```
ESP32-S3-firmware/
├── include/           # All .h files
│   ├── ITimerDevice.h         # FACADE INTERFACE - start here for new devices
│   ├── SGTimerDevice.h        # Reference implementation
│   ├── TimerApplication.h     # Main orchestrator
│   ├── SystemStateMachine.h   # FSM with 22 states
│   ├── DisplayManager.h       # LED rendering
│   └── common.h              # Pin definitions, configuration
├── src/              # All .cpp files
│   └── main.cpp      # Minimal: setup() creates TimerApplication
└── test/
    └── minimal_test.cpp       # BLE connection test

memory-bank/          # READ FIRST when extending system
├── Device-Implementation-Guide.md  # Step-by-step for new devices
├── State-Machine-Design.md         # FSM design rationale
└── ScoreDisplay-Special-Pie-Timer-Analysis.md  # Multi-device protocols

docs/
├── BUILD_AND_TEST.md          # PlatformIO commands, serial monitoring
├── HUB75_WIRING.md           # 18 GPIO pins to LED panels
└── sg-timer-reference/sg_timer_public_bt_api_32.md  # SG BLE API spec
```

## Development Workflows

### Build Environments (`platformio.ini`)
```bash
# Production firmware (default)
pio run -e main-firmware --target upload

# BLE connection testing (no display libs)
pio run -e ble-test --target upload

# Serial monitoring with ESP32 exception decoder
pio device monitor
```

**Important:** Switch to `ble-test` environment for pure BLE debugging - faster builds, excludes HUB75 libraries.

### Adding New Timer Devices
**Follow:** `memory-bank/Device-Implementation-Guide.md` (506 lines, comprehensive)

**Quick Steps:**
1. Create `YourDevice.h/cpp` implementing `ITimerDevice` interface
2. Parse device BLE protocol → convert to `NormalizedShotData`/`SessionData`
3. Update `TimerApplication::initialize()` to instantiate your device class
4. Test with real device, document protocol in `memory-bank/`

**Reference:** `SGTimerDevice.cpp` shows complete implementation pattern (BLE scanning, connection management, packet parsing).

## Critical Code Patterns

### Callback Registration (All Device Implementations)
```cpp
// In TimerApplication::setupCallbacks()
timerDevice->onShotDetected([this](const NormalizedShotData& data) {
  this->onShotDetected(data);  // Bridge to state machine + display
});
```
**Pattern:** All event-driven communication uses `std::function` callbacks. Device classes invoke callbacks; application handles state transitions.

### BLE Packet Parsing Example (SG Timer)
```cpp
// Big Endian extraction (SG Timer protocol - see docs/sg-timer-reference/sg_timer_public_bt_api_32.md)
uint32_t sessionId = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
uint16_t shotNum = (data[6] << 8) | data[7];
uint32_t shotTimeMs = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
```

**SG Timer BLE Protocol (API v3.2):**
- **Service UUID:** `7520FFFF-14D2-4CDA-8B6B-697C554C9311`
- **Device Names:** `SG-SST4AXXXXX` (Sport) or `SG-SST4BXXXXX` (GO), where XXXXX = serial number
- **Event Characteristic:** `75200001-14D2-4CDA-8B6B-697C554C9311` (notifications)
- **Command Characteristic:** `75200000-14D2-4CDA-8B6B-697C554C9311` (write with notify)

**Event Packet Format (all Big Endian):**
```
[len][event_id][event_data...]
```

**Event Types:**
- `0x00` SESSION_STARTED: `[len=7][0x00][sess_id(4)][start_delay(2)]`
  - `sess_id`: Unix timestamp (4 bytes)
  - `start_delay`: Units of 0.1 second (2 bytes)
- `0x04` SHOT_DETECTED: `[len=11][0x04][sess_id(4)][shot_num(2)][shot_time(4)]`
  - `shot_time`: **Already in milliseconds** (4 bytes) - no conversion needed
- `0x03` SESSION_STOPPED: `[len=7][0x03][sess_id(4)][total_shots(2)]`
- `0x01`/`0x02` SESSION_SUSPENDED/RESUMED: `[len=7][0x01/0x02][sess_id(4)][total_shots(2)]`

**Remote Control Commands (write to COMMAND characteristic):**
```cpp
// Start session: [0x01, 0x00]  (len=1, cmd_id=SESSION_START)
// Stop session:  [0x01, 0x03]  (len=1, cmd_id=SESSION_STOP)
// Response:      [0x02, cmd_id, resp_code]  (0x00=success, 0x01=error)
```

**Critical:** Special Pie uses single-byte centisecond values. Check `memory-bank/ScoreDisplay-Special-Pie-Timer-Analysis.md` for comparison.

### State Transitions (State Machine)
```cpp
// In SystemStateMachine::handleEvent()
if (currentState == SystemState::CONNECTING && event == Event::DEVICE_CONNECTED) {
  transitionTo(SystemState::CONNECTED);
}
```
**Pattern:** Events drive transitions. Timeouts handled automatically via `checkTimeout()`. Invalid transitions logged but not blocked.

### Display Updates (No Direct Rendering in Application)
```cpp
// CORRECT: Use DisplayManager methods
displayManager->showShotData(shotData);

// WRONG: Don't access MatrixPanel_I2S_DMA directly from application
```
**Separation:** Application → DisplayManager → MatrixPanel_I2S_DMA. Keeps rendering logic isolated.

## Hardware-Specific Details

### HUB75 LED Panels
- **Resolution:** 2x 64x32 panels = 128x32 total
- **Wiring:** 18 GPIO pins (R1, G1, B1, R2, G2, B2, A-E row select, CLK, LAT, OE)
- **Power:** 5V 10A supply, **NOT** from ESP32 USB (panels draw 2-4A each)
- **Library:** `mrfaptastic/ESP32 HUB75 LED MATRIX PANEL DMA Display@^3.0.14`
- **Capacitors Required:** 1000-2000µF on each panel's VCC/GND to prevent flicker

### ESP32-S3 DevKit-C
- **Flash:** 16MB, **PSRAM:** 8MB (required for dual-core BLE + display DMA)
- **Cores:** Core 0 (BLE stack), Core 1 (display rendering)
- **Build Flags:** `-DBOARD_HAS_PSRAM=1`, `-DCONFIG_BT_NIMBLE_ROLE_CENTRAL_DISABLED=0`
- **Partition:** `huge_app.csv` (large firmware due to BLE + LED libraries)

### Button & Brightness Control
- **Reset Button:** GPIO 4 (hardware interrupt, 50ms debounce, triggers MANUAL_RESET state)
- **Brightness Pot:** GPIO 1 (ADC, 12-bit, updates every 200ms, threshold=2 to reduce flicker)

## Project-Specific Conventions

### Logging System (`Logger.h`)
```cpp
LOG_INFO("TAG", "Message with %d args", value);
LOG_ERROR("TIMER", "Failed to connect");
LOG_STATE("FSM", "Transition: %s -> %s", oldState, newState);
```
**Levels:** ERROR, WARN, INFO, DEBUG, VERBOSE. Set in `TimerApplication::initialize()`. Tags identify subsystems (TIMER, DISPLAY, BLE, FSM).

### Memory Management
- **Smart Pointers:** Use `std::unique_ptr` for owned resources (see `TimerApplication.h`)
- **Static Callbacks:** BLE library requires static callbacks → use static instance pointers (see `SGTimerDevice::notifyCallback`)
- **PSRAM:** Required for display DMA buffers (fails without `-DBOARD_HAS_PSRAM=1`)

### Device Protocol Documentation Location
New device protocols → `memory-bank/ScoreDisplay-Special-Pie-Timer-Analysis.md` (existing patterns) or new `.md` file in `memory-bank/`.

SG Timer API spec → `docs/sg-timer-reference/sg_timer_public_bt_api_32.md` (vendor-provided, authoritative).

## Testing Strategy

1. **BLE-only:** `pio run -e ble-test` (test connection without display overhead)
2. **Hardware:** Physical timer + LED panels for integration validation
3. **Serial Monitor:** Watch for `[ERROR]`, `[STATE]`, `[BLE]` tags in output

## Common Pitfalls

- **Forgetting PSRAM flag:** Build succeeds, runtime crash in display init
- **Wrong endianness:** SG Timer is Big Endian, most examples online assume Little Endian
- **Blocking BLE callbacks:** Process data quickly in callbacks, defer heavy work to `update()` loop
- **Direct device references in app:** Always use `ITimerDevice*`, never `SGTimerDevice*` in `TimerApplication`
- **State machine timeouts:** States auto-transition on timeout - set appropriate values in `StateTimeouts` namespace

## Key Files to Reference

- **New Device:** `memory-bank/Device-Implementation-Guide.md` + `SGTimerDevice.cpp`
- **State Behavior:** `memory-bank/State-Machine-Design.md` + `SystemStateMachine.cpp`
- **Build Issues:** `docs/BUILD_AND_TEST.md` + `platformio.ini`
- **BLE Protocols:** `memory-bank/ScoreDisplay-Special-Pie-Timer-Analysis.md`
- **Pin Assignments:** `ESP32-S3-firmware/include/common.h` + `docs/HUB75_WIRING.md`

## Documentation Philosophy

This project extensively documents **discoverable patterns** in `memory-bank/`:
- Architecture decisions (why Facade Pattern vs alternatives)
- Device protocol reverse engineering (BLE packet formats)
- State machine design rationale (timeout values, recovery strategies)

**When making changes:** Update relevant `memory-bank/*.md` file to maintain AI/human context continuity.
