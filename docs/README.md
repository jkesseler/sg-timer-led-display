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

- `SGTimer` (`ESP32-S3-firmware/src/SGTimer.cpp`)
- `SpecialPieM1A2F` (`ESP32-S3-firmware/src/SpecialPieM1A2F.cpp`)
- `SpecialPieM1A2Plus` (`ESP32-S3-firmware/src/SpecialPieM1A2Plus.cpp`)
- `ASNTracker` (`ESP32-S3-firmware/src/ASNTracker.cpp`)

All of these implement `ITimerDevice` via `BaseTimerDevice`, and all shot times are normalized to milliseconds in `NormalizedShotData`.

## Implementation notes reflected by the docs

- Main coordinator: `ESP32-S3-firmware/src/TimerApplication.cpp`
- Display state management: `ESP32-S3-firmware/src/DisplayManager.cpp`
- Optional MQTT republish path: `ESP32-S3-firmware/src/MqttManager.cpp`
- Wi-Fi runtime config portal + NVS persistence: `ESP32-S3-firmware/src/WiFiConfig.cpp`

## BLE-LoRa Bridge

Standalone firmware for a LilyGo LoRa32 T3 v1.6.1 board that relays BLE timer events over a long-range LoRa radio link. Same binary; role (Transmitter / Receiver) is selected at runtime via a WiFi portal.

- [ble-lora-bridge/README.md](ble-lora-bridge/README.md) — start here: what it is, deployment model
- [ble-lora-bridge/build-and-configure.md](ble-lora-bridge/build-and-configure.md) — build, flash, WiFi portal, troubleshooting
- [ble-lora-bridge/hardware.md](ble-lora-bridge/hardware.md) — board, pin mapping, LoRa radio parameters
- [ble-lora-bridge/architecture.md](ble-lora-bridge/architecture.md) — dual-role design, data flow, class reference
- [ble-lora-bridge/lora-protocol.md](ble-lora-bridge/lora-protocol.md) — binary packet format and CRC specification

Source: `BLE-LoRa-Bridge/`

---

## Related project areas

- `../pwa-display-app/` - browser display client
- `../mqtt-simulator/` - MQTT event simulation tooling
