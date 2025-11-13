# SG Timer LED Display Bridge

Multi-Device Shot Timer Bridge with LED Matrix Display

An ESP32-based system that connects to shot timer devices via Bluetooth and displays real-time timing data on HUB75 LED matrix panels.


## Motivation
As a competitive shooter and software developer, I recognized the need for a reliable and visually engaging way to display shot timer data during training and competitions. While some manufacturers offer visual display solutions, this project provides an open, extensible platform that works with multiple timer brands through a unified interface.

This project is also an experiment in AI-assisted development to explore how AI tools can aid in building embedded systems.


## Project Overview

This project creates a bridge between shot timer devices and visual display systems, providing real-time display capabilities for shooting sports applications through a device abstraction layer that supports multiple timer brands.

### Supported Devices

- **SG Timer** (Sport and GO models) - Full protocol support
- **Special Pie M1A2+ Timer** - Validated protocol support

Additional timer devices can be added through the `ITimerDevice` interface.

### Key Features

- **Multi-Device Support**: Works with SG Timer and Special Pie Timer devices
- **Real-Time Display**: Live shot data on 128x32 LED matrix panels
- **Auto-Discovery**: Automatically detects and connects to compatible timers
- **Stable Connection Management**: Auto-reconnect on disconnect
- **Extensible Architecture**: Device abstraction layer for future timer support
- **Designed for Competitive Shooting**: Optimized for match environments

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Shot Timer Devices (BLE)                                   │
│  ┌───────────────┐   ┌──────────────────┐   ┌─────────────┐ │
│  │  SG Timer GO  │   │  SG Timer Sport  │   │ Special Pie │ │
│  │               │   │                  │   │   M1A2+     │ │
│  └────────┬──────┘   └─────────┬────────┘   └──────┬──────┘ │
└───────────┼────────────────────┼───────────────────┼─────────┘
            │                    │                   │
            ▼                    ▼                   ▼
┌─────────────────────────────────────────────────────────────┐
│  ESP32-S3 Bridge Firmware                                   │
│  ┌──────────────────────────────────────────────────┐       │
│  │         TimerApplication (Coordinator)           │       │
│  └────────────────────┬─────────────────────────────┘       │
│                       │                                     │
│       ┌───────────────┼───────────────┐                     │
│       ▼               ▼               ▼                     │
│  ┌─────────┐   ┌─────────────┐  ┌────────────┐              │
│  │ SG Timer│   │ Special Pie │  │   Future   │              │
│  │ Device  │   │Timer Device │  │  Devices   │              │
│  └────┬────┘   └──────┬──────┘  └─────┬──────┘              │
│       └───────────────┼───────────────┘                     │
│                       ▼                                     │
│           ┌───────────────────────┐                         │
│           │   ITimerDevice        │                         │
│           │  (Unified Interface)  │                         │
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
- **Connection**: BLE Events and Commands
- **Service UUID**: `7520FFFF-14D2-4CDA-8B6B-697C554C9311`
- **API Version**: BLE API 3.2
- **Data Format**: Variable-length TLV packets, millisecond resolution
- **Features**: Full remote control, session management, shot list retrieval
- **Reference**: [`docs/sg-timer-reference/`](docs/sg-timer-reference/)

### Special Pie M1A2+ Timer ✅
- **Connection**: BLE Notifications
- **Service UUID**: `0000FFF0-0000-1000-8000-00805F9B34FB`
- **Characteristic**: `0000FFF1-0000-1000-8000-00805F9B34FB` (Timer events)
- **Data Format**: Frame-based messages (F8 F9 ... F9 F8), centisecond resolution
- **Features**: Shot detection, session start/stop notifications
- **Reference**: [`docs/special-pie-timer-reference/BLE-Protocol-Analysis.md`](docs/special-pie-timer-reference/BLE-Protocol-Analysis.md)
- **Status**: Protocol validated with working test implementation

## Documentation

All documentation is in the [`docs/`](docs/) directory:

- **[docs/README.md](docs/README.md)** - Documentation index and navigation
- **[docs/BUILD_AND_TEST.md](docs/BUILD_AND_TEST.md)** - Build and testing guide
- **[docs/DEVICE_COMPARISON.md](docs/DEVICE_COMPARISON.md)** - Device comparison and testing strategies
- **[docs/HUB75_WIRING.md](docs/HUB75_WIRING.md)** - LED panel wiring guide
- **[docs/DISPLAY_SETUP.md](docs/DISPLAY_SETUP.md)** - Display configuration
- **[docs/sg-timer-reference/](docs/sg-timer-reference/)** - SG Timer protocol reference
- **[docs/special-pie-timer-reference/](docs/special-pie-timer-reference/)** - Special Pie protocol reference

## Planned Features

### Phase 1: Core Implementation ✅ *(Complete)*
- [x] Device abstraction layer (`ITimerDevice` interface)
- [x] Multi-device support (SG Timer and Special Pie Timer)
- [x] Automatic device discovery and connection
- [x] Real-time LED matrix display
- [x] Protocol validation and testing

### Phase 2: Enhanced Features *(In Progress)*
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
The system uses an interface pattern to abstract device-specific protocols:

```cpp
// Unified interface for all timer devices
class ITimerDevice {
  virtual bool connect(BLEAddress address) = 0;
  virtual void onShotDetected(std::function<void(const NormalizedShotData&)> callback) = 0;
  virtual void onSessionStarted(std::function<void(const SessionData&)> callback) = 0;
  virtual bool supportsRemoteStart() const = 0;
  virtual bool supportsShotList() const = 0;
  // ... additional interface methods
};

// Unified shot data structure
struct NormalizedShotData {
  uint32_t sessionId;
  uint16_t shotNumber;
  uint32_t absoluteTimeMs;    // Always in milliseconds
  uint32_t splitTimeMs;       // Time since previous shot
  uint32_t timestampMs;       // System timestamp
  const char* deviceModel;    // "SG Timer" or "Special Pie Timer"
  bool isFirstShot;           // True for first shot in session
};
```

### Multi-Device Support
- **Auto-Detection**: Scans for SG Timer (by address) or Special Pie Timer (by service UUID)
- **Protocol Normalization**: Converts device-specific data to unified format
- **Automatic Connection**: Connects to first available compatible device
- **Connection Recovery**: Automatically reconnects if connection is lost
- **Feature Detection**: Capability queries for device-specific features

## Development

### Project Structure
```
sg-timer-LED-display/
├── platformio.ini                      # PlatformIO configuration
├── ESP32-S3-firmware/                  # Main firmware source
│   ├── src/
│   │   ├── main.cpp                    # Application entry point
│   │   ├── TimerApplication.cpp        # Multi-device coordinator
│   │   ├── SGTimerDevice.cpp           # SG Timer implementation
│   │   ├── SpecialPieTimerDevice.cpp   # Special Pie implementation
│   │   ├── DisplayManager.cpp          # LED display controller
│   │   ├── BrightnessController.cpp    # Ambient light sensing
│   │   └── ButtonHandler.cpp           # User input
│   ├── include/
│   │   ├── ITimerDevice.h              # Device interface
│   │   ├── common.h                    # Shared definitions
│   │   └── ...                         # Component headers
│   └── test/
│       ├── sg-timer.cpp                # SG Timer standalone test
│       ├── special-pie-timer.cpp       # Special Pie test
│       └── led-matrix.cpp              # Display test
├── docs/                               # Documentation
│   ├── README.md                       # Documentation index
│   ├── BUILD_AND_TEST.md               # Build and test guide
│   ├── DEVICE_COMPARISON.md            # Device comparison
│   ├── HUB75_WIRING.md                 # Hardware wiring
│   ├── sg-timer-reference/             # SG Timer protocol docs
│   └── special-pie-timer-reference/    # Special Pie protocol docs
└── memory-bank/                        # Design analysis and architecture
    ├── Device-Implementation-Guide.md
    ├── project-state-analysis.md
    └── test-infrastructure-summary.md
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

This project is licensed under the Apache License License - see the [LICENSE](LICENSE) file for details.