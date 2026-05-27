# Documentation Index

This directory contains implementation-facing docs for all firmware in this repository.

---

## ESP32-S3 firmware  (`ESP32-S3-firmware/`)

LED matrix display board. Auto-discovers BLE shooting sport timers, normalises shot data, and renders times on a 128×32 HUB75 panel with optional MQTT republishing.

**Start here:** [esp32-s3/README.md](esp32-s3/README.md)

| Document | Contents |
|---|---|
| [esp32-s3/build-and-test.md](esp32-s3/build-and-test.md) | Build, flash, native tests, test environments |
| [esp32-s3/architecture.md](esp32-s3/architecture.md) | Data flow, class design, key patterns |
| [esp32-s3/hardware.md](esp32-s3/hardware.md) | HUB75 wiring, GPIO mapping, power requirements |
| [esp32-s3/display.md](esp32-s3/display.md) | State machine, visual layouts, colour codes, time format |
| [esp32-s3/timer-devices.md](esp32-s3/timer-devices.md) | Supported devices, event matrix, how to add a new device |
| [esp32-s3/mqtt-and-wifi.md](esp32-s3/mqtt-and-wifi.md) | MQTT topic structure, WiFi portal, NVS config |
| [esp32-s3/testing.md](esp32-s3/testing.md) | Native GoogleTest suites, coverage, stubs |

---

## BLE-LoRa Bridge  (`BLE-LoRa-Bridge/`)

LilyGo LoRa32 T3 v1.6.1 board. Relays BLE timer events over a long-range LoRa radio link. Same firmware binary; role (Transmitter / Receiver) selected at runtime via WiFi portal.

**Start here:** [ble-lora-bridge/README.md](ble-lora-bridge/README.md)

| Document | Contents |
|---|---|
| [ble-lora-bridge/build-and-configure.md](ble-lora-bridge/build-and-configure.md) | Build, flash, WiFi portal, troubleshooting |
| [ble-lora-bridge/hardware.md](ble-lora-bridge/hardware.md) | Board, pin mapping, LoRa radio parameters |
| [ble-lora-bridge/architecture.md](ble-lora-bridge/architecture.md) | Dual-role design, data flow, class reference |
| [ble-lora-bridge/lora-protocol.md](ble-lora-bridge/lora-protocol.md) | Binary packet format and CRC specification |

---

## Timer device protocol references  (`timer-protocols/`)

Byte-level BLE protocol documentation for each supported timer brand.

| Document | Covers |
|---|---|
| [timer-protocols/sg-timer-ble-api.md](timer-protocols/sg-timer-ble-api.md) | SG Timer Sport / GO — BLE API v3.2 |
| [timer-protocols/special-pie-ble.md](timer-protocols/special-pie-ble.md) | Special Pie M1A2 / M1A2+ — validated frame protocol |

---

## Related project areas

- `../pwa-display-app/` — browser display client
- `../mqtt-simulator/` — MQTT event simulation tooling
