# Build & Test Guide

This guide reflects the current `platformio.ini` and firmware layout in this repository.

## Prerequisites

### Software

```bash
python --version
pip install platformio
pio --version
```

### Hardware

- ESP32-S3 DevKit-C (or compatible `esp32-s3-devkitc-1` target)
- 2x HUB75 64x32 panels (chained as 128x32)
- 5V power supply for panels
- USB connection to development machine

## Repository layout used by PlatformIO

The root `platformio.ini` is configured to use:

- `ESP32-S3-firmware/src` as source directory
- `ESP32-S3-firmware/include` as include directory
- `ESP32-S3-firmware/test` as test directory

Run all `pio` commands from repository root.

## Build main firmware

```bash
pio run -e main-firmware
```

## Upload main firmware

```bash
pio run -e main-firmware -t upload
```

If auto-detect fails, specify the port:

```bash
# Windows example
pio run -e main-firmware -t upload --upload-port COM5
```

## Serial monitor

```bash
pio device monitor
```

Defaults from `platformio.ini`:

- baud: `115200`
- monitor filter: `esp32_exception_decoder`

## Available PlatformIO environments

### Production

- `main-firmware`
  - Builds `ESP32-S3-firmware/src/*`
  - Excludes `ESP32-S3-firmware/test/*`

### Test-focused environments

- `test-sg-timer` -> `ESP32-S3-firmware/test/sg-timer.cpp`
- `test-special-pie` -> `ESP32-S3-firmware/test/special-pie-timer.cpp`
- `test-led-matrix` -> `ESP32-S3-firmware/test/led-matrix.cpp`
- `test-scanner` -> `ESP32-S3-firmware/test/scanner.cpp`
- `special-pie-timer` -> `ESP32-S3-firmware/test/special-pie-timer.cpp` (legacy alias env)

Run any test environment with:

```bash
pio run -e <environment-name>
pio run -e <environment-name> -t upload
pio device monitor
```

## Typical validation workflow

1. Build production firmware: `pio run -e main-firmware`
2. Upload and monitor logs
3. Verify startup display and scan loop
4. Power on a compatible timer and confirm auto-connect
5. Start/stop a session and confirm shot updates on display

## Runtime behavior notes

- BLE scanning and connection are managed by `TimerApplication`
- Wi-Fi is initialized lazily after first BLE connection (`WiFiConfig`)
- MQTT manager is initialized only when MQTT server is set in NVM

These values are currently defined in `ESP32-S3-firmware/include/common.h`.

## Troubleshooting

### Build fails due to missing PlatformIO

```bash
pip install --upgrade platformio
```

### Upload fails (port access / not found)

- Close other serial terminals
- Reconnect USB cable
- Re-run with explicit `--upload-port`

### No serial output

- Confirm monitor baud is `115200`
- Press reset on ESP32-S3 after opening monitor

### Timer not discovered

- Confirm timer is advertising BLE
- Keep timer within close range
- Use `test-scanner` environment to inspect nearby BLE devices

## Related docs

- [DEVICE_COMPARISON.md](DEVICE_COMPARISON.md)
- [HUB75_WIRING.md](HUB75_WIRING.md)
- [DISPLAY_REFERENCE.md](DISPLAY_REFERENCE.md)