# ESP32-S3 Firmware

The ESP32-S3 firmware runs on an ESP32-S3 DevKit-C and drives a 128×32 HUB75 LED matrix panel that displays real-time shot data from competitive shooting sport timers. It auto-discovers supported BLE timers, normalises all time values to milliseconds, renders shot times on the panel, and optionally republishes events to an MQTT broker.

---

## Start here

1. [build-and-test.md](build-and-test.md) — build, flash, monitor, and run tests
2. [hardware.md](hardware.md) — HUB75 wiring, pin mapping, power requirements
3. [architecture.md](architecture.md) — data flow, class design, key patterns
4. [timer-devices.md](timer-devices.md) — supported devices, protocol comparison, how to add a new device

---

## All docs in this section

| Document | Contents |
|---|---|
| [architecture.md](architecture.md) | Data flow diagram, class descriptions, shared patterns |
| [build-and-test.md](build-and-test.md) | PlatformIO environments, build/flash commands, native tests |
| [hardware.md](hardware.md) | HUB75 connector pinout, ESP32-S3 GPIO mapping, power and panel chaining |
| [display.md](display.md) | Display state machine, visual layouts, color codes, time format |
| [timer-devices.md](timer-devices.md) | Device comparison matrix, ITimerDevice interface, how to add a new device |
| [mqtt-and-wifi.md](mqtt-and-wifi.md) | MQTT topic structure, retained messages, WiFi portal, NVS config |
| [testing.md](testing.md) | Native unit tests, test coverage, stubs, hardware test environments |

---

## Source layout

```
ESP32-S3-firmware/
├── include/          ← Headers (ITimerDevice.h, common.h, …)
├── src/              ← Production sources
│   ├── main.cpp
│   ├── TimerApplication.cpp
│   ├── DisplayManager.cpp
│   ├── MqttManager.cpp
│   ├── WiFiConfig.cpp
│   ├── SGTimer.cpp
│   ├── SpecialPieM1A2F.cpp
│   ├── SpecialPieM1A2Plus.cpp
│   └── ASNTracker.cpp
└── test/             ← GoogleTest native unit tests
    ├── test_protocol_parsing/
    ├── test_time_formatting/
    ├── test_ring_buffer/
    └── stubs/        ← Arduino/BLE mock headers for host builds
```

---

## Related areas

- BLE-LoRa bridge firmware: [../ble-lora-bridge/README.md](../ble-lora-bridge/README.md)
- Timer wire protocol references: [../timer-protocols/](../timer-protocols/)
