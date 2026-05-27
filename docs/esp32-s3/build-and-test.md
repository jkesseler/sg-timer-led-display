# ESP32-S3 Firmware — Build and Test

This guide reflects the current `platformio.ini` and firmware layout in this repository.

---

## Prerequisites

### Software

```bash
python --version       # 3.8+
pip install platformio
pio --version          # 6.x or later
```

### Hardware (production firmware)

- ESP32-S3 DevKit-C (`esp32-s3-devkitc-1`, N16R8V variant recommended: 16 MB flash, PSRAM)
- 2× HUB75 64×32 LED matrix panels (chained as 128×32)
- 5 V / 10 A power supply for panels
- USB cable to development machine

All `pio` commands run from the **repository root**.

---

## Build and flash production firmware

```bash
# Build only
pio run -e main-firmware

# Build and flash (auto-detect COM port)
pio run -e main-firmware -t upload

# Build and flash on a specific port (Windows)
pio run -e main-firmware -t upload --upload-port COM5
```

## Serial monitor

```bash
pio device monitor        # 115200 baud, with esp32_exception_decoder filter
```

Press **reset** on the ESP32-S3 after opening the monitor to catch startup output.

---

## Native unit tests (no hardware required)

The native test suite runs on the host PC via GoogleTest. No board needed.

```bash
# Run all native tests
pio test -e native-tests

# Run a single suite
pio test -e native-tests --filter test_protocol_parsing
pio test -e native-tests --filter test_ring_buffer
pio test -e native-tests --filter test_time_formatting
```

Test files live in `ESP32-S3-firmware/test/`. See [testing.md](testing.md) for full coverage details.

---

## Available PlatformIO environments

### Production

| Environment | Description |
|---|---|
| `main-firmware` | Full production firmware; builds all of `ESP32-S3-firmware/src/`; excludes test tools |

### Native tests (host)

| Environment | Description |
|---|---|
| `native-tests` | GoogleTest suite on the host PC (no hardware needed) |

### Hardware-focused test tools

| Environment | Flashes | Purpose |
|---|---|---|
| `tools-led-matrix` | Display-only test | Validates HUB75 wiring and colour output |
| `tools-scanner` | BLE scanner | Lists nearby BLE devices and their UUIDs for device debugging |
| `tools-wifi-config` | WiFi config portal | Tests the NVS/WiFiManager portal in isolation |
| `test-sg-timer` | SG Timer raw client | Dumps raw SG Timer BLE events to serial |
| `test-special-pie` | Special Pie raw client | Dumps raw Special Pie BLE events to serial |

```bash
pio run -e <environment> -t upload
pio device monitor
```

---

## Repository layout used by PlatformIO

```
platformio.ini          ← root-level config; all pio commands run from here
ESP32-S3-firmware/
├── src/                ← Production sources (compiled by main-firmware)
├── include/            ← Headers
└── test/               ← Native GoogleTest suites + stubs
BLE-LoRa-Bridge/        ← Separate firmware (see ble-lora-bridge/ docs)
```

---

## Typical validation workflow

1. `pio test -e native-tests` — confirm protocol parsing and time formatting pass on host
2. `pio run -e main-firmware` — verify clean production build
3. `pio run -e main-firmware -t upload` — flash board
4. `pio device monitor` — confirm startup marquee and BLE scan log
5. Power on a compatible timer → confirm auto-connect log line
6. Start a session → confirm display transitions and serial shot log

---

## Runtime behaviour notes

- **WiFi initialises lazily** — `WiFiConfig` does not start the WiFi stack until after the first BLE connection. MQTT is only activated when a broker IP is set in NVS.
- **BLE scanning** — `TimerApplication` scans for 10 s, then retries every 5 s until a device is found. Only one timer is connected at a time.
- **MQTT reconnect** — `MqttManager` retries in the background; the main loop never blocks on MQTT failures.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `pio` not found | PlatformIO not installed | `pip install --upgrade platformio` |
| Upload fails (port access) | Another serial tool has the port open | Close other terminals; reconnect USB; use `--upload-port COMx` |
| No serial output | Wrong baud rate | Confirm monitor baud is 115200; press board reset after opening monitor |
| Timer not discovered | Timer not advertising or out of BLE range | Confirm timer is advertising; use `tools-scanner` to inspect nearby devices |
| Native tests fail to build | Missing GoogleTest (auto-installed by PlatformIO) | Delete `.pio/` and re-run `pio test -e native-tests` |

---

## Related docs

- [hardware.md](hardware.md) — HUB75 wiring
- [timer-devices.md](timer-devices.md) — supported devices
- [testing.md](testing.md) — native test coverage details
