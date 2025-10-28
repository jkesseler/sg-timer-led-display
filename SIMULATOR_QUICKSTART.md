# SG Timer Simulator - Quick Start Guide

## What is the SG Timer Simulator?

The SG Timer Simulator is a comprehensive testing tool that allows you to develop and test the LED display firmware without needing a physical SG Timer device. It simulates all the essential behaviors of the real device, including connection sequences, shot detection, and session management.

## Quick Demo

1. **Enable the Simulator**
   - Open `ESP32-S3-firmware/include/common.h`
   - Uncomment the line: `#define USE_SIMULATOR`

2. **Build and Upload**
   ```bash
   cd sg-timer-LED-display
   pio run --target upload --target monitor
   ```

3. **Interactive Control**
   - Open Serial Monitor (115200 baud)
   - Type `help` and press Enter to see all commands
   - Try these commands:
     ```
     demo          # Quick demonstration
     status        # Show current state
     connect       # Connect simulator
     start         # Start session
     shot          # Trigger a shot
     stop          # End session
     ```

## Example Session

```
=== SG Timer Simulator Commands ===
> demo
[SIM] Starting demo sequence
[TIMER] Connected to: SG Timer Simulator v1.0
[TIMER] Session started: Timer Display Session

> shot
[SIM] Manual shot triggered
[SHOT] Shot #1 - 850ms

> status
=== Simulator Status ===
Device: SG Timer Simulator v1.0
Connection: CONNECTED
Mode: AUTO_SHOTS
Session Active: YES
Shot Count: 1
========================
```

## Simulation Modes

- **Manual**: Full control via commands
- **Auto**: Automatic shots every 3-8 seconds
- **Realistic**: Natural shooting patterns with pauses

## Development Benefits

✅ **No Hardware Required**: Test without physical SG Timer
✅ **Rapid Testing**: Instant connection and shot simulation
✅ **Deterministic**: Consistent behavior for debugging
✅ **Interactive**: Real-time control via serial commands
✅ **Complete Coverage**: Tests all display states and transitions

## Ready to Use

The simulator is now integrated and ready! Just enable it in `common.h` and start testing your display firmware immediately.

For detailed documentation, see `docs/SIMULATOR_USAGE.md`