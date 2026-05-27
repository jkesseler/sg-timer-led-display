# BLE-LoRa Bridge — Build and Configure

## Prerequisites

Same toolchain as the main ESP32-S3 firmware:

- Python 3.x with PlatformIO Core (`pip install platformio`)
- All `pio` commands run from the **repository root** (where `platformio.ini` lives)
- USB cable connected to the LilyGo LoRa32 T3 v1.6.1 board

Verify your setup:

```bash
pio --version          # 6.x or later
python --version       # 3.8+
```

---

## Build and flash

### Production firmware

```bash
# Build only
pio run -e lilygo-lora32-t3-v161

# Build and flash (auto-detect COM port)
pio run -e lilygo-lora32-t3-v161 -t upload

# Build and flash on a specific port (Windows)
pio run -e lilygo-lora32-t3-v161 -t upload --upload-port COM6
```

### Serial monitor

```bash
pio device monitor        # 115200 baud, all output
```

The OLED shows the current role, connection status, and packet counts. The serial log uses the same tagged macros as the main firmware (`LOG_SYSTEM`, `LOG_BLE`, `LOG_TIMER`, etc.).

---

## LoRa radio echo test (diagnostics)

A standalone echo-test tool is available to verify that two boards can communicate before deploying:

```bash
pio run -e lora-bridge-tools-lora-test -t upload
```

Both boards must be flashed with this image. Each board transmits a synthetic shot packet every 2 seconds **and** listens simultaneously. The OLED shows TX count, RX count, CRC errors, and uptime. Use this to confirm sync word, frequency, and pin assignments before production use.

---

## First-boot configuration

On first boot (or after NVS is erased), the board starts a WiFi access point:

```
SSID:     J.K. PewPew Long Range Bridge AP [deviceId]
Password: (none)
Portal:   http://192.168.4.1
```

Connect to that SSID from a phone or laptop, then open `http://192.168.4.1`. The portal presents the following settings:

| Setting | Values | Default |
|---|---|---|
| Device role | `0` = Transmitter, `1` = Receiver | 0 |
| Receiver output mode | `0` = MQTT, `1` = BLE Special Pie | 0 |
| MQTT server | IP address or hostname | *(empty)* |
| MQTT port | integer | 1883 |
| MQTT user | string | *(empty)* |
| MQTT password | string | *(empty)* |

Save and the board reboots into the selected role. Settings are persisted to NVS under namespace `bridge-cfg`. To reconfigure, hold the board in AP mode (power-cycle while pressing the boot button, or erase NVS via serial).

---

## Runtime behaviour

### Transmitter role

1. Starts BLE scan (10-second window, repeats every 5 s until a device is found).
2. Scan priority matches the main firmware: SpecialPieM1A2F → SGTimer → SpecialPieM1A2Plus → ASNTracker. First match connects immediately.
3. On connect, registers callbacks for all timer events.
4. Each event is serialised and transmitted over LoRa within the BLE callback (non-blocking; packet is ≤42 bytes, air-time ~12 ms).
5. Sends a `HEARTBEAT` packet every 30 seconds regardless of shot activity.
6. On BLE disconnect, waits `BLE_RECONNECT_INTERVAL` (5 s) before re-scanning.

### Receiver role

1. Polls `LoRaReceiver::update()` every main-loop iteration.
2. Validates application-layer CRC-16 on every received packet; increments `crcErrors` counter on failure.
3. Dispatches to:
   - **MQTT mode**: `MqttManager` publishes on `timer/<sourceId>/<event>` topics — same structure as the ESP32-S3 firmware.
   - **BLE Special Pie mode**: `SpecialPieBleServer` sends F8 F9 notifications; re-advertises after client disconnect.
4. WiFi connects lazily after the first valid LoRa packet is received (MQTT mode only).
5. RSSI of the last received packet is shown on the OLED.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Receiver shows 0 packets received | Sync word mismatch | Confirm both boards compiled with same `LORA_SYNC_WORD` (0x77); verify `LORA_FREQUENCY` matches |
| Receiver shows CRC errors only | Frequency offset or interference | Check frequency, try increasing SF temporarily to confirm link exists |
| Transmitter never connects to BLE timer | Timer not in scan priority list, or out of range | Confirm timer model is one of SGTimer / SpecialPieM1A2F / SpecialPieM1A2Plus / ASNTracker; verify BLE range; check `BridgeApplication::runTransmitter()` scan priority order |
| MQTT not publishing | WiFi credentials wrong, or broker unreachable | Check portal settings; verify broker IP and port; watch serial log for `LOG_SYSTEM` MQTT error messages |
| OLED blank after flash | I²C address mismatch | Confirm OLED at 0x3C; check SDA/SCL pin assignment in `common.h` |
| Board not found by `pio upload` | Wrong COM port or driver missing | Install CP210x / CH340 USB driver; specify `--upload-port COMx` explicitly |

---

## PlatformIO environments reference

| Environment | Purpose |
|---|---|
| `lilygo-lora32-t3-v161` | Production firmware for LilyGo LoRa32 T3 v1.6.1 |
| `lora-bridge-tools-lora-test` | Echo-test diagnostic — two boards exchange synthetic shot packets |

For main firmware environments (ESP32-S3 display board), see [../BUILD_AND_TEST.md](../BUILD_AND_TEST.md).
