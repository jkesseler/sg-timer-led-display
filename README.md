# SG Timer LED Display Bridge

ESP32-S3 firmware and companion tooling for displaying shot timer events on HUB75 LED panels, with optional MQTT republishing for external displays.

## What this repository contains

- `ESP32-S3-firmware/` - Main PlatformIO firmware for the ESP32-S3 bridge
- `pwa-display-app/` - Web display client that can consume timer events via MQTT
- `mqtt-simulator/` - Node.js simulator for MQTT event testing
- `docs/` - Hardware, build, and protocol documentation

## Current firmware capabilities

- BLE auto-discovery and connection through a unified `ITimerDevice` interface
- Real-time rendering on 2x chained HUB75 panels (`128x32` total)
- Device normalization to `NormalizedShotData` (all timing in milliseconds)
- Non-blocking main loop with health monitoring and reconnect behavior
- Optional MQTT republish path with queued shot publishing (ring buffer)

## Supported BLE timer implementations

- `SGTimerDevice` (UUID-based discovery)
- `SpecialPieMacTimerDevice` (name-pattern discovery: `SP M1A2 Timer ...`)
- `SpecialPieTimerDevice` (UUID-based discovery)
- `ASNTrackerDevice` (UUID-based discovery)

Scan priority in `TimerApplication::scanForDevices()` is:
1. `SpecialPieMacTimerDevice`
2. `SGTimerDevice`
3. `SpecialPieTimerDevice`
4. `ASNTrackerDevice`

## Runtime modes and config

Compile-time configuration lives in `ESP32-S3-firmware/include/common.h`:

- `TIMER_TYPE` (`TIMER_TYPE_BLE` or `TIMER_TYPE_MQTT`)
- `TIMER_REPUBLISH_MQTT` (default `false`)

Current default behavior is BLE-first operation (`TIMER_TYPE_BLE`) with MQTT republish disabled unless explicitly enabled.

When MQTT republish is enabled and broker settings exist, firmware publishes to:

- `timer/connection/state`
- `timer/session/started`
- `timer/session/stopped`
- `timer/session/suspended`
- `timer/session/resumed`
- `timer/countdown/complete`
- `timer/shot/detected`
- `timer/device/info`

## Quick start (firmware)

```bash
# from repository root
pip install platformio

# build main firmware
pio run -e main-firmware

# upload
pio run -e main-firmware -t upload

# monitor serial logs
pio device monitor
```

## PlatformIO environments

- `main-firmware` - production firmware build
- `test-sg-timer` - SG timer protocol test
- `test-special-pie` - Special Pie protocol test
- `test-led-matrix` - display test
- `test-scanner` - BLE scanning test
- `special-pie-timer` - alias test environment for Special Pie timer test source

## Documentation map

- [docs/README.md](docs/README.md) - documentation index
- [docs/BUILD_AND_TEST.md](docs/BUILD_AND_TEST.md) - build/upload/test commands
- [docs/DEVICE_COMPARISON.md](docs/DEVICE_COMPARISON.md) - device behavior matrix
- [docs/HUB75_WIRING.md](docs/HUB75_WIRING.md) - wiring reference
- [docs/DISPLAY_REFERENCE.md](docs/DISPLAY_REFERENCE.md) - display states
- [docs/sg-timer-reference/sg_timer_public_bt_api_32.md](docs/sg-timer-reference/sg_timer_public_bt_api_32.md)
- [docs/special-pie-timer-reference/BLE-Protocol-Analysis.md](docs/special-pie-timer-reference/BLE-Protocol-Analysis.md)

## License

Licensed under Apache-2.0. See [LICENSE](LICENSE).