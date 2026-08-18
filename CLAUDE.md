# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ESP32-S3 firmware that acts as a BLE bridge between competitive shooting sport timers and a HUB75 LED matrix display. The device auto-discovers supported BLE timers, normalizes their data, renders shot times on a 128×32 LED panel, and optionally republishes events via MQTT.

## Build & test commands

All `pio` commands run from the **repository root**. The `platformio.ini` at the root points PlatformIO into `ESP32-S3-firmware/` subdirectories.

```bash
# Build and flash production firmware
pio run -e main-firmware
pio run -e main-firmware -t upload
pio run -e main-firmware -t upload --upload-port COM5   # explicit port if needed

# Serial monitor (115200 baud, with ESP32 exception decoder)
pio device monitor

# Run all native (host-only) unit tests — no hardware required
pio test -e native-tests

# Run a single test suite
pio test -e native-tests --filter test_protocol_parsing
pio test -e native-tests --filter test_ring_buffer
pio test -e native-tests --filter test_time_formatting

# Build/flash diagnostics tools (hardware required, not production firmware)
pio run -e tools-led-matrix -t upload
pio run -e tools-scanner -t upload
pio run -e tools-wifi-config -t upload
```

## File layout

```
ESP32-S3-firmware/
├── include/          # Header files (.h)
│   ├── ITimerDevice.h
│   ├── BaseTimerDevice.h
│   ├── common.h      # Pin definitions, compile-time constants
│   ├── Logger.h
│   ├── DisplayManager.h
│   └── <DeviceName>.h
├── src/              # Implementation files (.cpp)
│   ├── main.cpp
│   ├── TimerApplication.cpp
│   └── <DeviceName>.cpp
└── test/
    ├── stubs/        # Arduino/BLE header stubs for host builds
    ├── test_protocol_parsing/
    ├── test_ring_buffer/
    └── test_time_formatting/
docs/                 # Hardware & protocol reference
memory-bank/          # Architecture decision records
```

> Exclude `__NO_COMMIT__` files from code generation suggestions.

## Architecture

### Data flow

```
BLE Timer Device
  └─► ITimerDevice (device-specific impl)
        └─► NormalizedShotData / SessionData callbacks
              └─► TimerApplication (coordinator)
                    ├─► FreeRTOS queue  ──►  MqttManager  ──►  MQTT broker
                    └─► DisplayManager  ──►  HUB75 128×32 panels
```

### NormalizedShotData

All device implementations convert native formats into this struct:

```cpp
struct NormalizedShotData {
  uint32_t sessionId;
  uint16_t shotNumber;
  uint32_t absoluteTimeMs;  // ALWAYS milliseconds
  uint32_t splitTimeMs;     // Time since previous shot (ms)
  uint64_t timestampMs;     // System timestamp when detected
  const char* deviceModel;
  bool isFirstShot;
};
```

**Key constraint:** BLE notification callbacks run on the BLE stack — they must be fast and non-blocking. `onShotDetected` enqueues shot data into a FreeRTOS queue (`xQueueSend`); the main loop drains it in `publishQueuedEvents()` via batch publish.

### Main loop phases (`TimerApplication::run`)

1. `WiFiConfig::update()` — non-blocking Wi-Fi background management
2. BLE device management — scan / process scan results / `timerDevice->update()`
3. `publishQueuedEvents()` — drain FreeRTOS shot queue to MQTT (up to 8 shots/cycle)
4. `mqttManager->update()` — MQTT loop & reconnect
5. `displayManager->update()` — dirty-flag-driven display rendering
6. `performHealthCheck()` — periodic health logging
7. `vTaskDelay(MAIN_LOOP_DELAY)` — 10 ms yield to FreeRTOS

### Device abstraction (`ITimerDevice` / `BaseTimerDevice`)

All supported timer brands implement `ITimerDevice`. `BaseTimerDevice` provides shared BLE connection management, callback registration, and the heartbeat update loop. Concrete classes only need to implement `matchesDevice()`, `attemptConnection()`, and `processTimerData()`.

| Class | Discovery method | Protocol |
|---|---|---|
| `SpecialPieM1A2F` | Name pattern `SP M1A2 Timer …` | F8 F9 frame, centiseconds |
| `SGTimer` | Service UUID | Length-prefixed packets, milliseconds |
| `SpecialPieM1A2Plus` | Service UUID | F8 F9 frame, centiseconds |
| `ASNTracker` | Service UUID | F8 F9 frame, centiseconds |

Scan priority in `processScanResults()`: SpecialPieM1A2F → SGTimer → SpecialPieM1A2Plus → ASNTracker (first match wins and connects immediately).

**Critical rule:** All time values placed in `NormalizedShotData` must be in **milliseconds**. Special Pie / ASN devices report centiseconds — multiply by 10 in the device implementation.

### Adding a new timer device

1. `include/YourDevice.h` — extend `BaseTimerDevice`; define service/characteristic UUIDs as `static const char*`, and declare `static constexpr uint8_t SHOT_INDEX_BASE` = the shot number the device puts on the wire for the first shot of a session (0 or 1). Frame-protocol devices pass it to the `FrameProtocolTimerDevice` constructor; direct `BaseTimerDevice` subclasses apply it in `processTimerData()`.
2. `src/YourDevice.cpp` — implement `matchesDevice()`, `attemptConnection()`, `processTimerData()`; convert all times to ms
3. Register in `TimerApplication::processScanResults()` alongside existing devices
4. Add tests in `ESP32-S3-firmware/test/test_protocol_parsing/` following `ProtocolTestBase` pattern
5. Run `pio test -e native-tests` to verify before touching hardware

Use `SGTimer` as reference for UUID-based devices; `SpecialPieM1A2F` for name-pattern devices.

### DisplayManager

Uses the dirty-flag pattern: callers invoke `showXxx()` methods to update internal state, which sets `displayDirty = true`. `update()` (called every loop) redraws only when dirty. The display renders at 128×32 in RGB565. Time is formatted as `SS:CC` (seconds:centiseconds) for absolute times and `S:CC` / `SS:CC` for splits.

`DisplayState` enum values: `STARTUP`, `DISCONNECTED`, `SCANNING`, `CONNECTED`, `WAITING_FOR_SHOTS`, `SHOWING_SHOT`, `SESSION_ENDED`. Colors are defined in the `DisplayColors` struct (RGB565).

`DisplayManager::formatTime` and `DisplayManager::formatSplitTime` are replicated in `test_time_formatting.cpp` — **keep them in sync** if you change the production implementations.

### MqttManager

MQTT topics are per-device, built at `initialize()` time: `timer/<deviceId>/<event>`. The device ID comes from `DeviceId` (flash-backed unique ID). MQTT is non-fatal — if not configured or unavailable, the firmware continues in display-only mode. Session events publish directly; shot events go through the FreeRTOS queue.

### Compile-time configuration (`common.h`)

`TIMER_TYPE` (`TIMER_TYPE_BLE` / `TIMER_TYPE_MQTT`) controls which input path is compiled. `TIMER_TYPE_MQTT` is not yet implemented. `DEBUG_BUILD` shortens `STARTUP_MESSAGE_DELAY` from 5 s to 1 s.

## Logging

Use the component-tagged macros — no bare `Serial.print` in production code:

```cpp
LOG_SYSTEM("...");          // Application lifecycle
LOG_BLE("...");             // BLE operations
LOG_TIMER("...");           // Shot/session events
LOG_DISPLAY("...");         // Display updates
LOG_ERROR("COMP", "...");   // Errors with explicit tag
LOG_DEBUG("COMP", "...");   // Verbose debug
```

Logging level is set in `main.cpp` via `Logger::setLevel(LogLevel::INFO)`. Switch to `DEBUG` for verbose output during development.

## Native tests

Tests live in `ESP32-S3-firmware/test/` and run on the host PC via GoogleTest (`pio test -e native-tests`). They use stubs in `test/stubs/` for Arduino/BLE headers. Protocol tests use `#define private public` to access `processTimerData()` directly. Hardware-dependent code (DisplayManager, MqttManager) is not covered by native tests.

## BLE implementation patterns

**Connection sequence:** scan → connect → get service → get characteristic → register notify. Always use full 128-bit UUID strings (e.g. `"7520FFFF-14D2-4CDA-8B6B-697C554C9311"`).

**Static instance for C-style notification callbacks** (required by the BLE library):
```cpp
class MyDevice : public BaseTimerDevice {
  static MyDevice* instance;

  static void notifyCallback(BLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    if (instance) instance->processTimerData(data, len);
  }
};
```

**No heavy work inside notification callbacks** — parse the raw bytes and fire the registered `std::function` callback immediately; do not call display or MQTT code from within a callback.

No reconnection attempts without a rate-limiting delay (`BLE_RECONNECT_INTERVAL_MS`).

## Hardware configuration

Key constants defined in `common.h`:

```cpp
#define POTENTIOMETER_PIN A0       // GPIO1, 12-bit ADC (0–4095)
#define RESET_BUTTON_PIN  4
#define BUTTON_DEBOUNCE_MS 50
#define PANEL_WIDTH  64
#define PANEL_HEIGHT 32
#define PANEL_CHAIN  2             // Two panels = 128×32 total
```

HUB75 full pin mapping: `docs/HUB75_WIRING.md`. Power: 5 V / 10 A for the LED panels.

## Embedded constraints

- No heap allocation in notification callbacks or inside `publishQueuedEvents()`.
- No `delay()` inside device driver classes — use `millis()` deltas and return promptly.
- Avoid floating-point math in performance-sensitive paths (BLE callbacks, display render loop).
- Do **not** call `u8g2_for_adafruit_gfx.getUTF8Width()` — use the pixel-width estimate already in `DisplayManager` (`textPixelWidth`).
- Wi-Fi is initialized lazily after the first BLE connection; don't assume it is ready at startup.

## Code conventions

- **Classes:** PascalCase; **methods:** camelCase; **constants:** `UPPER_SNAKE_CASE`; **private members:** camelCase (no underscore prefix)
- Smart pointers (`std::unique_ptr`) for component ownership in `TimerApplication`; raw `new` for BLE library objects
- `#pragma once` for all headers
- Validate BLE payload length before parsing; null-check all BLE objects (`pClient`, `pService`, `pChar`) before use
- Keep device-specific protocol logic isolated inside the device class — nothing device-specific leaks into `TimerApplication`

## Documentation structure

```
docs/
├── BUILD_AND_TEST.md          # Build and test procedures
├── HUB75_WIRING.md            # Hardware wiring diagrams
├── DISPLAY_REFERENCE.md       # Display states and layouts
├── DEVICE_COMPARISON.md       # Supported device feature matrix
└── sg-timer-reference/        # Protocol specifications
    └── sg_timer_public_bt_api_32.md

memory-bank/                   # Architecture decision records
├── Device-Implementation-Guide.md
├── project-state-analysis.md
└── special-pie-implementation-notes.md
```

When to update:
- Protocol quirks → inline comments in the device `.cpp`
- Hardware changes → `docs/HUB75_WIRING.md`
- New display states → `docs/DISPLAY_REFERENCE.md`
- Architectural decisions → `memory-bank/`

## Commit messages

Follow conventional commits: `type(scope): description` in imperative mood, subject ≤ 50 characters.

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `chore`, `ci`.

Use a body with `*` bullet points for additional detail when needed.
