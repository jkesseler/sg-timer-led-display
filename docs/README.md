# Documentation Index

This directory contains implementation-facing docs for the current ESP32-S3 firmware.

## Start here

1. [BUILD_AND_TEST.md](BUILD_AND_TEST.md) - how to build, flash, and run test environments
2. [DEVICE_COMPARISON.md](DEVICE_COMPARISON.md) - current timer device behavior and differences
3. [HUB75_WIRING.md](HUB75_WIRING.md) - panel wiring and hardware constraints

## Core docs

### Build and runtime

- [BUILD_AND_TEST.md](BUILD_AND_TEST.md)
  - PlatformIO environments from `platformio.ini`
  - Upload/monitor commands
  - Test environment mapping to source files

### Hardware and display

- [HUB75_WIRING.md](HUB75_WIRING.md)
- [DISPLAY_SETUP.md](DISPLAY_SETUP.md)
- [DISPLAY_REFERENCE.md](DISPLAY_REFERENCE.md)

### Device protocols

- [DEVICE_COMPARISON.md](DEVICE_COMPARISON.md)
- [sg-timer-reference/sg_timer_public_bt_api_32.md](sg-timer-reference/sg_timer_public_bt_api_32.md)
- [special-pie-timer-reference/BLE-Protocol-Analysis.md](special-pie-timer-reference/BLE-Protocol-Analysis.md)

## Current supported timer implementations

- `SGTimerDevice` (`ESP32-S3-firmware/src/SGTimerDevice.cpp`)
- `SpecialPieMacTimerDevice` (`ESP32-S3-firmware/src/SpecialPieMacTimerDevice.cpp`)
- `SpecialPieTimerDevice` (`ESP32-S3-firmware/src/SpecialPieTimerDevice.cpp`)
- `ASNTrackerDevice` (`ESP32-S3-firmware/src/ASNTrackerDevice.cpp`)

All of these implement `ITimerDevice` via `BaseTimerDevice`, and all shot times are normalized to milliseconds in `NormalizedShotData`.

## Implementation notes reflected by the docs

- Main coordinator: `ESP32-S3-firmware/src/TimerApplication.cpp`
- Display state management: `ESP32-S3-firmware/src/DisplayManager.cpp`
- Optional MQTT republish path: `ESP32-S3-firmware/src/MqttManager.cpp`
- Wi-Fi runtime config portal + NVS persistence: `ESP32-S3-firmware/src/WiFiConfig.cpp`

## Related project areas

- `../pwa-display-app/` - browser display client
- `../mqtt-simulator/` - MQTT event simulation tooling
