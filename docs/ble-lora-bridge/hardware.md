# BLE-LoRa Bridge — Hardware Reference

## Supported board

**LilyGo LoRa32 T3 v1.6.1** — `env:lilygo-lora32-t3-v161` in `platformio.ini`

The board integrates an ESP32 SoC, an SX1276 LoRa radio, and an SSD1306 OLED display on a single PCB. No external wiring is needed for the radio or display.

---

## SX1276 LoRa radio — SPI pin mapping

| Signal | GPIO | Notes |
|---|---|---|
| NSS (chip select) | 18 | Active low |
| RST (reset) | 14 | Some v1.6 batches use GPIO 23 — verify against board silkscreen |
| DIO0 (IRQ) | 26 | Packet-done interrupt |
| SCK | 5 | |
| MOSI | 27 | |
| MISO | 19 | |

Defined in `BLE-LoRa-Bridge/include/common.h` as `LORA_*_PIN` constants and overridden at build time by the `env:lilygo-lora32-t3-v161` build flags (`-DLORA_NSS=18`, etc.).

---

## OLED display — I²C pin mapping

| Signal | GPIO | Value |
|---|---|---|
| SDA | 21 | |
| SCL | 22 | |
| RST | — | No hardware reset (`OLED_RST_PIN = -1`) |
| I²C address | — | 0x3C |
| Resolution | — | 128 × 64 px |

Driver: `ThingPulse/ESP8266 and ESP32 OLED driver for SSD1306 displays` via PlatformIO library `OLED_BW`.

---

## Battery ADC

| Signal | GPIO |
|---|---|
| Battery voltage sense | 35 |

Used for battery level reporting (currently read-only; not surfaced in the OLED status view).

---

## LoRa radio parameters

| Parameter | Value | Notes |
|---|---|---|
| Frequency | 868 MHz | EU 868 MHz ISM band |
| Spreading factor | SF7 | Fastest; shortest range |
| Bandwidth | 500 kHz | Widest BW supported by SX1276 |
| Coding rate | 4/5 | Minimum overhead |
| TX power | 14 dBm | Maximum legal EU 868 band |
| Sync word | 0x77 | Private network — avoids LoRaWAN traffic (0x34 / 0x12) |
| Preamble length | 8 symbols | Default |
| Air-time (42-byte packet) | ~12 ms | At SF7 / BW 500 kHz |

All constants are defined in `BLE-LoRa-Bridge/include/common.h` under the `LORA_*` prefix.

### Range vs. throughput trade-off

The current SF7/BW500 profile is optimised for **minimum latency** at close range (up to ~500 m line-of-sight with clear air). For longer range, increase SF (e.g., SF10) and decrease BW (e.g., 125 kHz) at the cost of air-time, which grows exponentially. All units in a deployment must share identical SF, BW, frequency, and sync word.

---

## Notes on board revisions

The LilyGo LoRa32 product line has several hardware revisions. The firmware targets v1.6.1 specifically:

- RST pin for SX1276 varies across batches (GPIO 14 vs GPIO 23). Check the board silkscreen label next to the RST pads.
- The v2.x boards use different GPIO assignments and require a separate PlatformIO environment.
- The OLED on v1.6.1 does not have a dedicated reset pin; the firmware sets `OLED_RST_PIN = -1` accordingly.
