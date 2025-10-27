# SG Timer LED Display Bridge

Multi-Device Shot Timer Bridge with LED Matrix Display

A ESP32-based system that connects to Shooters Global Timer devices via Bluetooth and displays real-time timing data on HUB75 LED matrix panels.


## Motivation
As a competitive shooter and software developer, I recognized the need for a reliable and visually engaging way to display shot timer data during training and competitions. Currently Shooters Global does not offer a visual display solution like Special Pie does. SG does offer a public API for their devices allow it to be integrated into custom solutions.

This project also an experiment in AI assisted development to explore how AI tools can aid in building embedded systems.


## Project Overview

This project creates a bridge between SG Timer devices and visual display systems, providing real-time display capabilities for shooting sports applications through a device abstraction layer that can be extended to support additional timer brands in the future.

### Supported Devices

Currently there is native support for SG Timer Sport and GO models.

### Key Features

- Real-Time Display Live shot data on 128x32 LED matrix panels
- Stable connection management with auto-reconnect
- Extensible architecture for future device support
- Designed for competitive shooting environments

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Shot Timer Devices (BLE)                                   │
│  ┌───────────────┐   ┌──────────────────┐                   │
│  │  SG Timer GO  │   │  SG Timer Sport  │                   │
│  │               │   │                  │                   │
│  └────────┬──────┘   └─────────┬────────┘                   │
└───────────┼────────────────────┼──────────────────-─────────┘
            │                    │
            ▼                    ▼
┌─────────────────────────────────────────────────────────────┐
│  ESP32-S3 Bridge Firmware                                   │
│  ┌─────────────────-─┐  ┌──────────────────┐                │
│  │ Device Drivers    │  │ Unified Facade   │                │
│  │ (Protocol Parsing)│  │ (Data Normaliz.) │                │
│  └────────┬─────────-┘  └─────────┬────────┘                │
│           └───────────┬───────────┘                         │
│                       ▼                                     │
│           ┌───────────────────────┐                         │
│           │  Display Controller   │                         │
│           └───────────┬───────────┘                         │
└───────────────────────┼─────────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────────┐
│  128x32 LED Matrix Display (2x 64x32 HUB75 Panels)          │
│  ┌────────────────────────────────────────────────────────┐ │
│  │  Shot #3    │    02.45s                                │ │
│  │  Split: 0.75s                                          │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## Hardware Requirements

### ESP32 Module

- ESP-WROOM-32 with a ESP32‑D0WDQ6 chip, ESP32-S2, or ESP32-S3<br />
  RISC-V ESP32's (like the C3) are not supported as they do not have the hardware 'LCD mode' support.
- 16MB Flash, PSRAM support
- Dual-core processor for BLE + Display management
- USB-C for programming and power

### Display Hardware

- 2x HUB75 LED Matrix Panels (64x32 each = 128x32 total)
- 5V 10A Power Supply (for full brightness)
- HUB75 Cables (usually included with panels)
- Jumper Wires (ESP32 to HUB75 connections)


## Quick Start

### 1. Hardware Setup
See [`docs/HUB75_WIRING.md`](docs/HUB75_WIRING.md) for complete wiring instructions.

Essential Connections:
- ESP32-S3 to HUB75 panels (18 GPIO pins)
- 5V power supply to LED panels
- Common ground between ESP32 and panel power
- USB-C to ESP32 for programming

### 2. Software Setup

```bash
# Install PlatformIO
pip install platformio

# Clone repository (when available)
git clone <repository-url>
cd sg-timer-LED-display

# Build and upload firmware
pio run --target upload

# Monitor serial output
pio device monitor
```

### 3. Operation
1. Power on ESP32 and LED panels
2. Turn on shot timer device
3. ESP32 will automatically discover and connect
4. Start shooting session on timer
5. Real-time shot data displays on LED matrix

## Protocol Support

### SG Timer (Sport/GO)
- Connection: BLE Events and Commands
- Service UUID: `7520FFFF-14D2-4CDA-8B6B-697C554C9311`
- API Version: BLE API 3.2
- Data Format: Variable-length packets, millisecond resolution
- Features: Full remote control, session management, saved sessions

## Planned Features

### Phase 1: Core Implementation *(Current)*
- [x] Device abstraction layer for future timer support
- [x] Multi-device support, currently SG Timer Sport and GO.
- [x] Automatic device discovery and connection
- [x] Real-time LED matrix display

### Phase 2: Enhanced Features
- [ ] Additional timer device support
- [ ] Publish timer data over MQTT
- [ ] Squads, online shooter, and current shooter display
- [ ] Web interface for display on any device with a browser
      Score logging, online / current shooter management



## Documentation

- [Build & Test Guide](docs/BUILD_AND_TEST.md) - Complete setup and testing procedures
- [HUB75 Wiring](docs/HUB75_WIRING.md) - Detailed wiring diagrams and pin assignments
- [Display Reference](docs/DISPLAY_REFERENCE.md) - Visual states and display modes
- [Display Setup](docs/DISPLAY_SETUP.md) - Panel configuration and positioning
- [SG Timer API](docs/sg-timer-reference/sg_timer_public_bt_api_32.md) - BLE protocol documentation

## Architecture Details

### Device Abstraction Layer
The system uses a Facade Pattern to abstract device-specific protocols:

```cpp
// Unified interface for all timer devices
class ITimerDevice {
  virtual bool connect(BLEAddress address) = 0;
  virtual void onShotDetected(std::function<void(NormalizedShotData)> callback) = 0;
  virtual bool supportsRemoteStart() = 0;
  // ... additional interface methods
};

// Unified shot data structure
struct NormalizedShotData {
  uint32_t sessionId;
  uint16_t shotNumber;
  uint32_t absoluteTimeMs;    // Always in milliseconds
  uint32_t splitTimeMs;       // Time since previous shot
  uint64_t timestampMs;       // System timestamp
  const char* deviceModel;
};
```

### Multi-Device Support
- Auto-Detection: Identifies SG Timer devices from BLE advertisement
- Protocol Normalization: Converts device-specific data to unified format
- Automatic Connection: Connects to available SG Timer devices automatically
- Connection Recovery: Automatically reconnects to the same device if connection is lost

## Development

### Project Structure
```
sg-timer-LED-display/
├── platformio.ini              # PlatformIO configuration
├── ESP32-S3-firmware/          # Main firmware source
│   ├── src/main.cpp            # Application entry point
│   ├── lib/                    # Custom libraries
│   └── include/                # Header files
├── docs/                       # Documentation
│   ├── BUILD_AND_TEST.md       # Setup and testing guide
│   ├── HUB75_WIRING.md         # Hardware wiring diagrams
│   ├── DISPLAY_REFERENCE.md    # Display states reference
│   └── sg-timer-reference/     # Timer protocol documentation
└── memory-bank/                # Design analysis and architecture docs
    ├── ScoreDisplay-Special-Pie-Timer-Analysis.md
    └── build-options.md
```

### Build Configuration
The project uses PlatformIO with ESP32-S3 target:

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
lib_deps =
    ESP32 BLE Arduino
    https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/
    adafruit/Adafruit GFX Library@^1.12.3
build_flags =
    -DCORE_DEBUG_LEVEL=3
    -DCONFIG_BT_NIMBLE_ROLE_CENTRAL_DISABLED=0
```


## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.