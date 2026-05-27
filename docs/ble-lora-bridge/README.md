# BLE-LoRa Bridge

The BLE-LoRa Bridge is a standalone ESP32 firmware for the LilyGo LoRa32 T3 v1.6.1 board. It forms a wireless relay between BLE-equipped shooting sport timers and any downstream consumer that needs the events but cannot reach the timer directly. A **Transmitter** unit sits next to the shooter's timer, receives BLE notifications, and re-broadcasts them over a long-range (≤500 m line-of-sight) LoRa radio link. A **Receiver** unit picks up those packets and forwards them either to an MQTT broker or by emulating a Special Pie M1A2+ BLE peripheral — giving the main ESP32-S3 LED display board a feed it can consume without any code changes.

Both roles share the same firmware binary. The runtime role and output mode are selected once through a WiFi configuration portal and persisted to flash (NVS).

---

## Start here

1. [build-and-configure.md](build-and-configure.md) — build, flash, and run the WiFi setup portal
2. [hardware.md](hardware.md) — supported board, pin mapping, LoRa radio parameters
3. [architecture.md](architecture.md) — class design, data flow, shared components
4. [lora-protocol.md](lora-protocol.md) — binary packet format, CRC, payload reference

---

## Physical deployment

A minimal deployment requires **two boards**:

| Unit | Role | What it does |
|---|---|---|
| Board A | **Transmitter** | Scans for a BLE timer, connects, receives shot / session events, serialises them into LoRa packets |
| Board B | **Receiver** | Listens on the LoRa radio, deserialises packets, forwards to MQTT broker *or* re-advertises as a Special Pie M1A2+ BLE peripheral |

Multiple Receiver boards can listen simultaneously; Transmitters broadcast to all in range. You can also add more Transmitters at different shooting lanes, provided they all use the same LoRa sync word (`0x77`) and frequency (868 MHz).

---

## Role reference

| Role | When to use | Output |
|---|---|---|
| TRANSMITTER | Board is placed beside the BLE timer | LoRa radio packets |
| RECEIVER / MQTT | Board is near the network; downstream consumers read MQTT | Publishes to MQTT broker on the same topic structure as the ESP32-S3 firmware |
| RECEIVER / BLE Special Pie | Board replaces or augments a physical Special Pie timer; drives the LED display directly over BLE | Advertises as `Special Pie M1A2+`, sends F8 F9 notifications |

---

## Related areas

- Main display firmware: `ESP32-S3-firmware/`
- Shared device drivers used by both firmwares: `ESP32-S3-firmware/src/SGTimer.cpp`, `SpecialPieM1A2F.cpp`, `SpecialPieM1A2Plus.cpp`
- Protocol constants authoritative source: `BLE-LoRa-Bridge/include/common.h`, `BLE-LoRa-Bridge/include/LoRaPacket.h`
