# ESP32-S3 BLE Bridge for SG Shot Timer

This firmware connects an ESP32-S3 DevKit-C to an **SG Shot Timer** (SG-SST4) via Bluetooth Low Energy (BLE) and displays real-time shot data on dual HUB75 LED matrix panels.

## Features

### BLE Integration
- **Automatic device discovery** - Scans for SG Timer devices (SG-SST4A/B)
- **Real-time event notifications** - Receives session and shot events
- **Shot list retrieval** - Reads complete session history after completion
- **Auto-reconnect** - Automatically reconnects on disconnect

### Display Features
- **128x32 LED Display** - Two chained 64x32 HUB75 panels
- **Real-time shot tracking** - Shows shot number and time during session
- **Session start indicator** - Displays "00:00" when waiting for first shot
- **Session summary** - Shows total shots and last shot after session end
- **High visibility** - Adjustable brightness, large fonts, color-coded status

### Display Modes

| Mode | Trigger | Display |
|------|---------|---------|
| **Waiting** | SESSION_STARTED (no shots) | "READY" + "00:00" |
| **Active** | SHOT_DETECTED | Shot # + Time |
| **Ended** | SESSION_STOPPED | Total shots + Last shot # |

## Hardware Requirements

- **ESP32-S3 DevKit-C** (16MB Flash recommended)
- **2x HUB75 LED Matrix Panels** (64x32 each)
- **5V Power Supply** (10A recommended for full brightness)
- **HUB75 cables** (usually included with panels)
- **Jumper wires** (for ESP32 to HUB75 connections)

## Quick Start

### 1. Hardware Setup
See **[HUB75_WIRING.md](HUB75_WIRING.md)** for complete wiring instructions.

**Power Connection**:
- Panels: 5V 10A power supply
- ESP32-S3: USB or separate 5V
- **Important**: Connect GND between ESP32 and panel power supply

### 2. Software Setup

```bash
# Install PlatformIO
pip install platformio

# Build and upload
pio run --target upload

# Monitor serial output
pio device monitor
```

### 3. Usage

1. Power on the system
2. Display shows "SG TIMER READY"
3. Turn on your SG Shot Timer
4. ESP32 automatically connects (watch serial monitor)
5. Start a session on the timer
6. Display updates in real-time with each shot

## Configuration

### Display Brightness

Edit in `src/main.cpp`:
```cpp
display->setBrightness8(192); // 0-255, adjust for ambient light
```

### Panel Configuration

```cpp
#define PANEL_WIDTH 64      // Single panel width
#define PANEL_HEIGHT 32     // Single panel height
#define PANEL_CHAIN 2       // Number of chained panels
```

### BLE Settings

The device automatically searches for timers matching:
- Name prefix: `SG-SST4`
- Service UUID: `7520FFFF-14D2-4CDA-8B6B-697C554C9311`

## Documentation

- **[DISPLAY_SETUP.md](DISPLAY_SETUP.md)** - Display configuration, modes, and troubleshooting
- **[HUB75_WIRING.md](HUB75_WIRING.md)** - Complete wiring diagrams and pin reference
- **[docs/sg_timer_public_bt_api_32.md](../../docs/sg_timer_public_bt_api_32.md)** - SG Timer BLE API specification

## BLE Protocol

### Events Monitored (EVENT Characteristic)

| Event ID | Name | Action |
|----------|------|--------|
| 0x00 | SESSION_STARTED | Reset display, show "00:00" |
| 0x04 | SHOT_DETECTED | Update shot # and time |
| 0x03 | SESSION_STOPPED | Show summary, read shot list |

### Characteristics Used

- **EVENT** (`75200001-...`): Real-time notifications
- **SHOT_LIST** (`75200004-...`): Session history retrieval

## Serial Monitor Output

```
=== SG Shot Timer BLE Bridge ===
[DISPLAY] Initializing HUB75 LED panels...
[BLE] ESP32-S3 BLE Client initialized
[SCAN] Found SG Shot Timer: SG-SST4A12345
[BLE] Connected to SG Shot Timer
[TIMER] === SESSION STARTED ===
[DISPLAY] Waiting for shots...
[TIMER] SHOT: 1 - TIME: 2.345s | SPLIT: 0.000s
[DISPLAY] Shot: 1, Time: 00:02.345
```

## Troubleshooting

### Display Issues
- **Blank display**: Check power supply and brightness setting
- **Garbage/noise**: Try different panel driver in `initDisplay()`
- **Flickering**: Insufficient power or bad connections

### BLE Connection Issues
- **Cannot find timer**: Ensure timer is on and in range
- **Disconnects frequently**: Check battery level on timer
- **No data**: Verify timer is in RO (Review Officer) session mode

See **[DISPLAY_SETUP.md](DISPLAY_SETUP.md)** for detailed troubleshooting.

## Development

### Project Structure
```
firmware/
├── src/
│   └── main.cpp          # Main firmware code
├── include/              # Headers
├── platformio.ini        # PlatformIO config
├── DISPLAY_SETUP.md      # Display documentation
├── HUB75_WIRING.md       # Wiring guide
└── README.md             # This file
```

### Dependencies
- ESP32 Arduino Core
- ESP32-HUB75-MatrixPanel-DMA (v3.0.0+)

### Building from Source

```bash
# Clean build
pio run --target clean

# Build only
pio run

# Upload and monitor
pio run --target upload && pio device monitor
```

## License

This project is part of the Active-Target system.

## Hardware Requirements

- **ESP32-S3-DevKitC-1** development board
- **Special Pie M1A2+ Shot Timer** with BLE capability

## Software Requirements

- **PlatformIO** IDE or extension for VS Code
- **ESP32 Arduino Framework**

## BLE Configuration

The firmware connects to the Special Pie timer using these parameters:

- **Device Name:** "M1A2 Shot Timer" (adjust if different)
- **Service UUID:** `0000fff0-0000-1000-8000-00805f9b34fb`
- **Characteristic UUID:** `0000fff1-0000-1000-8000-00805f9b34fb`

## Data Protocol

The firmware processes three types of messages from the shot timer:

### Start Signal
- **Command Type:** `52` (0x34)
- **Triggered:** When start button is pressed on timer
- **Output:** `[TIMER] === START BUTTON PRESSED ===`

### Shot Data
- **Command Type:** `54` (0x36)
- **Data Format:** `[Header][Data][Type][Reserved][Seconds][Centiseconds][Shot#]`
- **Output:** `[TIMER] SHOT: X - TIME: S.CC | SPLIT: S.CC`
  - Time format: seconds.centiseconds (e.g., "1.33" = 1.33 seconds)
  - Split time calculated from previous shot

### Stop Signal
- **Command Type:** `24` (0x18)
- **Triggered:** When stop button is pressed on timer
- **Output:** `[TIMER] === STOP BUTTON PRESSED ===`

## Installation

1. **Clone/Download** this firmware to your project directory
2. **Open** the project in PlatformIO
3. **Connect** your ESP32-S3 DevKit-C via USB
4. **Build and Upload** the firmware:
   ```bash
   pio run --target upload
   ```

## Usage

1. **Power on** the Special Pie M1A2+ Shot Timer
2. **Reset** the ESP32-S3 to start scanning
3. **Monitor** the serial output at 115200 baud:
   ```bash
   pio device monitor
   ```
4. The firmware will automatically:
   - Scan for the shot timer
   - Connect when found
   - Display connection status
   - Print received shot data

## Serial Output Example

```
=== Special Pie M1A2+ BLE Bridge ===
ESP32-S3 DevKit-C Starting...
[BLE] ESP32-S3 BLE Client initialized
[SCAN] Starting BLE scan for Special Pie Timer...
[SCAN] Found device: M1A2 Shot Timer (RSSI: -45)
[SCAN] Found Special Pie M1A2+ Shot Timer!
[BLE] Connecting to device at address: 54:14:A7:77:02:A2
[BLE] Connected to server, discovering services...
[BLE] Found service, getting characteristic...
[BLE] Found characteristic, setting up notifications...
[BLE] Notification callback registered
[BLE] === Ready to receive shot timer data ===

[DATA] Received 8 bytes: f8 01 34 00 00 00 00 f9
[TIMER] === START BUTTON PRESSED ===

[DATA] Received 8 bytes: f8 01 36 00 01 33 02 f9
[TIMER] SHOT: 2 - TIME: 1.33 | SPLIT: 0.00

[DATA] Received 8 bytes: f8 01 36 00 01 65 03 f9
[TIMER] SHOT: 3 - TIME: 1.65 | SPLIT: 0.32

[DATA] Received 8 bytes: f8 01 18 00 00 00 00 f9
[TIMER] === STOP BUTTON PRESSED ===
```

## Troubleshooting

### Connection Issues
- Ensure the shot timer is powered on and in BLE mode
- Check that the device name matches in the code
- Verify the timer is not connected to another device

### No Data Received
- Confirm the service and characteristic UUIDs are correct
- Check serial monitor is set to 115200 baud
- Verify BLE notifications are working

### Build Errors
- Ensure PlatformIO ESP32 platform is installed
- Check that the ESP32 BLE Arduino library is available
- Verify board configuration matches your hardware

## Configuration

### Device Name
If your shot timer advertises with a different name, modify this line:
```cpp
const char* DEVICE_NAME = "M1A2 Shot Timer";  // Change as needed
```

### UUIDs
If the timer uses different UUIDs, update these constants:
```cpp
const char* SERVICE_UUID = "0000fff0-0000-1000-8000-00805f9b34fb";
const char* CHARACTERISTIC_UUID = "0000fff1-0000-1000-8000-00805f9b34fb";
```

## Next Steps

This firmware provides the foundation for:
- **MQTT Integration:** Publish timer data to MQTT broker
- **WiFi Connectivity:** Connect to existing networks
- **Web Interface:** Host configuration and monitoring pages
- **Multiple Timers:** Support for multiple shot timer devices

## License

This firmware is provided as-is for educational and development purposes.