# ESP32-S3 Firmware — Testing

## Overview

The firmware has two testing tiers:

1. **Native unit tests** — run on the host PC via GoogleTest; no hardware required; cover protocol parsing, time formatting, and the ring buffer.
2. **Hardware test environments** — PlatformIO environments that flash a single focused component onto a real board for manual inspection.

---

## Native unit tests

```bash
# Run all suites
pio test -e native-tests

# Run a single suite
pio test -e native-tests --filter test_protocol_parsing
pio test -e native-tests --filter test_ring_buffer
pio test -e native-tests --filter test_time_formatting
```

Test files live under `ESP32-S3-firmware/test/`.

### Test suites

#### `test_protocol_parsing`

File: `ESP32-S3-firmware/test/test_protocol_parsing/test_protocol_parsing.cpp`

Covers all four device drivers using `#define private public` to access `processTimerData()` directly. Each test constructs a raw byte sequence and asserts the parsed `NormalizedShotData` fields.

| Test area | What is verified |
|---|---|
| SGTimer event IDs | `0x00`–`0x05` parsed correctly |
| Special Pie frame validation | `F8 F9 … F9 F8` markers, minimum length check |
| Special Pie shot parsing | Seconds + centiseconds → milliseconds conversion |
| Special Pie split time | Delta calculation with centisecond borrowing |
| Special Pie name pattern | `matchesDevice()` accepts `SP M1A2 Timer XXXX`, rejects others |
| SpecialPieM1A2Plus | Same protocol, UUID-based match |
| ASNTracker | Same protocol, zero-indexed shot number normalisation |
| Multi-shot sequences | FIFO ordering; correct split accumulation |
| Edge cases | 0 ms, 999 ms, session boundary resets |

#### `test_time_formatting`

File: `ESP32-S3-firmware/test/test_time_formatting/test_time_formatting.cpp`

Tests `DisplayManager::formatTime()` and `DisplayManager::formatSplitTime()`:

| Scenario | Expected output |
|---|---|
| 0 ms | `"00.000"` |
| 12 345 ms (< 60 s) | `"12.345"` |
| 60 000 ms (exactly 60 s) | `"01:00.0"` |
| 83 400 ms (> 60 s) | `"01:23.4"` |
| Buffer truncation | No overflow |

> **Important:** `formatTime` and `formatSplitTime` are duplicated verbatim in the test file. If you change the production implementation in `DisplayManager.cpp`, update the test copy too.

#### `test_ring_buffer`

File: `ESP32-S3-firmware/test/test_ring_buffer/test_ring_buffer.cpp`

Tests the lock-free power-of-2 ring buffer used for BLE→MQTT shot event queueing.

| Scenario | Verified |
|---|---|
| Empty / full detection | `isEmpty()` / `isFull()` |
| FIFO ordering | Items dequeued in insertion order |
| Wraparound | Head and tail wrap correctly at buffer boundary |
| Producer–consumer simulation | 32 items enqueued and dequeued in sequence |
| Maximum capacity | 31 usable slots (size 32 minus one guard slot) |

---

## Stubs

`ESP32-S3-firmware/test/stubs/` contains mock headers for Arduino and ESP32 BLE APIs so the native test build compiles without the Arduino toolchain:

| Stub file | Replaces |
|---|---|
| `Arduino.h` | `millis()`, `delay()`, `Serial`, `String` |
| `BLEDevice.h` / `BLEClient.h` / … | BLE client and characteristic types |
| `FreeRTOS.h` / `queue.h` | `xQueueCreate()`, `xQueueSend()`, `xQueueReceive()` |

The stubs provide the minimal API surface needed for protocol parsing and data structure tests. Hardware-dependent code (`DisplayManager`, `MqttManager`) is excluded from the native test build.

---

## Hardware test environments

These environments flash a focused single-component test to a real board. Monitor output at 115200 baud.

| Environment | Purpose |
|---|---|
| `tools-scanner` | Lists all nearby BLE advertisements with UUIDs — use to identify unknown timers |
| `tools-led-matrix` | Renders red/green/blue full-screen fills — validates HUB75 wiring |
| `tools-wifi-config` | Opens the WiFiManager portal in isolation — validates NVS read/write |
| `test-sg-timer` | Connects to an SG Timer and dumps raw BLE events to serial |
| `test-special-pie` | Connects to a Special Pie timer and dumps raw BLE events |

```bash
pio run -e <environment> -t upload
pio device monitor
```

---

## What is not covered by native tests

| Component | Reason |
|---|---|
| `DisplayManager` | Depends on DMA HUB75 library (hardware-only) |
| `MqttManager` | Requires live broker and WiFi stack |
| `WiFiConfig` | Requires `Preferences` / NVS flash access |
| BLE connection lifecycle | Requires BLE hardware and a real timer device |

These are covered manually with the hardware test environments and the production firmware.

---

## Adding tests for a new device

1. Add a `TEST_F` class in `test_protocol_parsing.cpp` following the pattern of `SGTimerTest` or `SpecialPieM1A2PlusTest`.
2. Use `#define private public` at the top of the file (already present) to access `processTimerData()`.
3. Construct raw BLE notification bytes and call `device.processTimerData(bytes, len)`.
4. Assert `NormalizedShotData` fields via a captured callback.
5. Run `pio test -e native-tests` to confirm all existing and new tests pass.
