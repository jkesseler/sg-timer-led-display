# Documentation Index

This directory contains all documentation for the SG Timer LED Display project.

## Quick Start

1. **New to the project?** Start with [BUILD_AND_TEST.md](BUILD_AND_TEST.md)
2. **Hardware setup?** See [HUB75_WIRING.md](HUB75_WIRING.md) and [DISPLAY_SETUP.md](DISPLAY_SETUP.md)
3. **Testing a specific device?** See [DEVICE_COMPARISON.md](DEVICE_COMPARISON.md)

## Documentation Structure

### Build & Testing
- **[BUILD_AND_TEST.md](BUILD_AND_TEST.md)** - Complete build, upload, and testing guide
  - Prerequisites and setup
  - Build process with PlatformIO
  - Step-by-step testing procedures for both timer types
  - Troubleshooting guide
  - Test checklists

### Hardware Setup
- **[HUB75_WIRING.md](HUB75_WIRING.md)** - HUB75 LED panel wiring to ESP32-S3
  - Pin mappings
  - Power requirements
  - Wiring diagrams

- **[DISPLAY_SETUP.md](DISPLAY_SETUP.md)** - Display configuration and setup
  - Panel specifications
  - Configuration options
  - Brightness control

- **[DISPLAY_REFERENCE.md](DISPLAY_REFERENCE.md)** - Display API and usage reference
  - DisplayManager API
  - Color schemes
  - Layout specifications

### Device Protocols

- **[DEVICE_COMPARISON.md](DEVICE_COMPARISON.md)** - Comparison of supported timer devices ⭐
  - Feature comparison table
  - Protocol differences
  - Testing strategies
  - Implementation details

- **[sg-timer-reference/](sg-timer-reference/)** - SG Timer BLE protocol documentation
  - `sg_timer_public_bt_api_32.md` - Official BLE API specification
  - `sg_timer_public_bt_api-32.pdf` - PDF version

- **[special-pie-timer-reference/](special-pie-timer-reference/)** - Special Pie Timer protocol
  - `BLE-Protocol-Analysis.md` - Validated protocol documentation ✅
  - Message formats
  - Implementation examples

## Supported Timer Devices

### SG Timer
- **Protocol**: Custom TLV format
- **Discovery**: By MAC address
- **Features**: Full bidirectional control, shot list retrieval
- **Reference**: [sg-timer-reference/](sg-timer-reference/)
- **Implementation**: `ESP32-S3-firmware/src/SGTimerDevice.cpp`

### Special Pie M1A2+ Timer
- **Protocol**: Frame-based messaging
- **Discovery**: By service UUID
- **Features**: Shot notifications, session events
- **Reference**: [special-pie-timer-reference/BLE-Protocol-Analysis.md](special-pie-timer-reference/BLE-Protocol-Analysis.md)
- **Implementation**: `ESP32-S3-firmware/src/SpecialPieTimerDevice.cpp`
- **Test**: `ESP32-S3-firmware/test/special-pie-timer.cpp` ✅ Validated

## Architecture Overview

```
┌─────────────────────────────────────────┐
│        TimerApplication                 │
│  (Multi-device coordinator)             │
└────────────┬────────────────────────────┘
             │
     ┌───────┴────────┐
     │                │
┌────▼────┐    ┌──────▼──────┐
│ SG Timer│    │Special Pie  │
│ Device  │    │Timer Device │
└────┬────┘    └──────┬──────┘
     │                │
     └────────┬───────┘
              │
    ┌─────────▼──────────┐
    │  ITimerDevice      │
    │  (Common Interface)│
    └────────────────────┘
```

All devices implement `ITimerDevice` interface and output normalized shot data.

## Testing

### Test Files
- `ESP32-S3-firmware/test/sg-timer.cpp` - SG Timer standalone test
- `ESP32-S3-firmware/test/special-pie-timer.cpp` - Special Pie Timer test ✅ **Validated**
- `ESP32-S3-firmware/test/led-matrix.cpp` - Display test

### Running Tests
See [BUILD_AND_TEST.md](BUILD_AND_TEST.md) for detailed testing procedures.

## Common Tasks

### I want to...

**Build and upload firmware**
→ See [BUILD_AND_TEST.md](BUILD_AND_TEST.md) section "Build Process"

**Wire up the LED panels**
→ See [HUB75_WIRING.md](HUB75_WIRING.md)

**Test with my SG Timer**
→ See [DEVICE_COMPARISON.md](DEVICE_COMPARISON.md) section "Testing SG Timer"

**Test with my Special Pie Timer**
→ See [DEVICE_COMPARISON.md](DEVICE_COMPARISON.md) section "Testing Special Pie Timer"

**Understand the Special Pie protocol**
→ See [special-pie-timer-reference/BLE-Protocol-Analysis.md](special-pie-timer-reference/BLE-Protocol-Analysis.md)

**Troubleshoot connection issues**
→ See [BUILD_AND_TEST.md](BUILD_AND_TEST.md) section "Troubleshooting"

**Add support for a new timer device**
→ See [DEVICE_COMPARISON.md](DEVICE_COMPARISON.md) and `ESP32-S3-firmware/include/ITimerDevice.h`

## Validation Status

| Document | Status | Last Validated |
|----------|--------|----------------|
| BUILD_AND_TEST.md | ✅ Updated | 2025-11-13 |
| DEVICE_COMPARISON.md | ✅ Created | 2025-11-13 |
| Special Pie BLE Protocol | ✅ Validated | 2025-11-13 |
| SG Timer BLE Protocol | 📄 Documented | - |
| HUB75_WIRING.md | 📄 Documented | - |
| DISPLAY_SETUP.md | 📄 Documented | - |
| DISPLAY_REFERENCE.md | 📄 Documented | - |

## Additional Resources

### Memory Bank
Project analysis and design documents are in `../memory-bank/`:
- `Device-Implementation-Guide.md` - Device implementation patterns
- `project-state-analysis.md` - Current project state
- `test-infrastructure-summary.md` - Testing approach

### Code Structure
Main source code is in `../ESP32-S3-firmware/`:
- `src/` - Production implementation
- `include/` - Header files
- `test/` - Standalone test programs

## Contributing

When adding new documentation:
1. Update this README.md index
2. Follow existing markdown formatting
3. Include code examples where helpful
4. Mark validation status
5. Cross-reference related documents

## Questions?

- Protocol questions → See device-specific reference docs
- Build issues → See BUILD_AND_TEST.md troubleshooting
- Hardware issues → See wiring and setup docs
- Architecture questions → See DEVICE_COMPARISON.md
