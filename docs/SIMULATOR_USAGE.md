# SG Timer Simulator

The SG Timer Simulator provides a virtual implementation of the SG Timer device interface for testing and development purposes without requiring physical hardware.

## Features

- **Multiple Simulation Modes**: Manual control, auto-connection, auto-shots, and realistic shooting patterns
- **Interactive Commands**: Serial console interface for real-time control
- **Realistic Timing**: Simulates authentic shot detection delays and connection patterns
- **Development-Friendly**: Easy testing of display states and application logic

## Configuration

Enable the simulator by uncommenting the following line in `include/common.h`:

```cpp
#define USE_SIMULATOR
```

When `USE_SIMULATOR` is defined, the application will use the simulator instead of attempting to connect to a real SG Timer device.

## Simulation Modes

### MANUAL Mode
- Complete manual control over all simulator functions
- Trigger shots manually via serial commands
- Control connection state manually
- Best for: Testing specific scenarios and edge cases

### AUTO_CONNECT Mode
- Automatically simulates the connection sequence
- Waits for manual shot triggers after connection
- Best for: Testing connection handling and session management

### AUTO_SHOTS Mode
- Automatically connects and starts generating shots
- Configurable shot intervals (3-8 seconds)
- Best for: Continuous testing and demonstrations

### REALISTIC Mode
- Simulates realistic shooting patterns with variable timing
- Includes pauses between shot strings
- More authentic shooting simulation
- Best for: User interface testing and demonstrations

## Serial Commands

When the simulator is active, you can control it via the Serial Monitor (115200 baud):

### Connection Commands
- `connect` or `c` - Start connection simulation
- `disconnect` or `d` - Disconnect simulator

### Session Commands
- `start` or `st` - Start shooting session
- `stop` or `sp` - Stop shooting session
- `shot` or `sh` - Trigger manual shot (manual mode only)

### Mode Commands
- `manual` or `m` - Switch to manual control mode
- `auto` - Switch to auto-shots mode
- `realistic` - Switch to realistic shooting pattern

### Utility Commands
- `status` or `s` - Show current simulator status
- `reset` or `r` - Reset simulator to initial state
- `demo` - Start quick demonstration sequence
- `help` or `h` - Show command help

## Usage Examples

### Basic Testing
1. Enable simulator in `common.h`
2. Upload firmware to ESP32-S3
3. Open Serial Monitor at 115200 baud
4. Type `help` to see available commands
5. Type `demo` for a quick demonstration

### Manual Shot Testing
```
> manual          # Switch to manual mode
> connect         # Connect simulator
> start           # Start session
> shot            # Trigger shot 1
> shot            # Trigger shot 2
> stop            # End session
```

### Automatic Demo
```
> auto            # Switch to auto mode
> demo            # Starts auto-connection and shot generation
```

### Status Monitoring
```
> status          # Shows connection state, mode, shot count, etc.
```

## Development Notes

### Architecture
- Implements the `ITimerDevice` interface for seamless integration
- Uses the same callback system as the real device implementation
- Maintains state consistency with the actual device behavior

### Timing Characteristics
- Connection simulation: 2-4 seconds
- Shot detection delay: 50-200ms (simulating sensor lag)
- Auto-shot intervals: 3-8 seconds
- Realistic patterns: Variable timing with natural pauses

### Memory Usage
- Minimal additional RAM overhead
- Uses efficient string handling for commands
- Smart pointer management for automatic cleanup

## Troubleshooting

### Simulator Not Responding
- Verify `USE_SIMULATOR` is defined in `common.h`
- Check Serial Monitor is set to 115200 baud
- Ensure commands are terminated with Enter/newline

### Build Errors
- Make sure all simulator files are included in the build
- Verify PlatformIO dependencies are up to date
- Check that ESP32-S3 target is properly configured

### Display Issues
- Simulator uses the same display logic as real device
- Use `status` command to verify simulator state
- Check display wiring and power if no output visible

## Integration Testing

The simulator is designed to test the complete application stack:

1. **Display Manager**: All display states and transitions
2. **Brightness Controller**: Light sensor simulation
3. **Session Management**: Start/stop/suspend/resume flows
4. **Shot Detection**: Timing accuracy and data formatting
5. **Error Handling**: Connection failures and recovery

## Future Enhancements

Potential improvements for the simulator:

- Web-based control interface
- Shot pattern recording/playback
- Network connectivity simulation
- Battery level simulation
- Temperature/environmental factor simulation