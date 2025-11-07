# Firmware Simplification - Matching Working Test Code

## Overview
The firmware has been simplified to match the working `minimal_test.cpp` implementation while preserving the existing architecture (TimerApplication, SGTimerDevice, DisplayManager, etc.).

## Key Changes Made

### 1. SGTimerDevice.h - Simplified Header
**Changes:**
- Removed complex `AdvertisedDeviceCallbacks` and `ClientCallback` classes
- Removed connection state flags: `doConnect`, `doScan`, `connectionInProgress`
- Removed `pServerAddress` pointer - now using constant target address
- Simplified to direct BLE components: `pClient`, `pEventCharacteristic`, `pService`
- Added constants from minimal_test: `lastShotNum`, `lastShotSeconds`, `lastShotHundredths`, `hasLastShot`
- Added target device address constant: `TARGET_DEVICE_ADDRESS = "dd:0e:9d:04:72:c3"`

**What Was Kept:**
- ITimerDevice interface implementation
- Callback registration methods
- Session tracking
- Event processing

### 2. SGTimerDevice.cpp - Simplified Implementation

#### Connection Flow (Now Matches minimal_test.cpp)
**Before:**
- Complex scan with callbacks
- Deferred connection with `doConnect` flag
- Separate `connectToServer()` and `discoverServices()` methods
- Complex state tracking with multiple flags

**After:**
- Direct scan in `attemptConnection()`
- Immediate connection when device found
- Inline service discovery and characteristic registration
- Simple connection status check in `update()`

#### Key Methods Simplified:

**`update()` Method:**
```cpp
// Simple pattern from minimal_test.cpp:
if (!isConnectedFlag) {
  // Throttle reconnection attempts (5 seconds)
  attemptConnection();
} else {
  // Check connection status
  if (pClient && pClient->isConnected()) {
    // Heartbeat every 30 seconds
  } else {
    // Connection lost - cleanup and retry
  }
}
```

**`attemptConnection()` Method:**
- Direct BLE scan (10 seconds)
- Loop through found devices
- Check for target address `dd:0e:9d:04:72:c3`
- Wait 2 seconds before connecting (matches minimal_test)
- Connect, get service, get characteristic
- Register for notifications immediately
- Set connection flag if successful

**`processTimerData()` Method:**
- Matches minimal_test notification parsing exactly
- Prints hex data for debugging
- Parses all event types (SESSION_STARTED, SHOT_DETECTED, etc.)
- Stores last shot info for session end display
- Creates NormalizedShotData and calls callbacks

### 3. What Was Removed
- `AdvertisedDeviceCallbacks` class - not needed for simple scan
- `ClientCallback` class - not needed for simple connection
- `scanForDevices()` method - merged into `attemptConnection()`
- `connectToServer()` method - merged into `attemptConnection()`
- `discoverServices()` method - merged into `attemptConnection()`
- `parseHexData()` method - simplified inline
- `readShotListInternal()` method - removed for now (can be added back later)
- Complex connection state machine flags

### 4. Architecture Preserved
The following architecture remains intact:
- **TimerApplication**: Main application coordinator
- **ITimerDevice**: Device interface abstraction
- **DisplayManager**: Display rendering
- **BrightnessController**: Brightness control
- **SystemStateMachine**: System state management
- **ButtonHandler**: Button input handling
- **Logger**: Logging system

All these components work exactly as before, just with a simplified BLE connection layer.

## Why This Works

### The Problem
The original implementation had too many layers of abstraction for BLE connection:
- Callback classes that complicated the connection flow
- Deferred connection with flags (`doConnect`, `doScan`)
- Separation of connection phases that made debugging difficult

### The Solution
The minimal_test.cpp proved that a simpler approach works:
1. Scan directly for the target device
2. Connect immediately when found
3. Register for notifications in one go
4. Simple connection status check in loop

### Benefits
- **Reliability**: Matches tested hardware implementation
- **Debuggability**: Linear flow is easier to debug
- **Maintainability**: Less code, fewer state flags
- **Performance**: No unnecessary delays or state checks

## Testing Recommendations

1. **Basic Connection Test**: Verify device connects and receives notifications
2. **Reconnection Test**: Verify device reconnects after timer power cycle
3. **Session Test**: Verify all session events are received and processed
4. **Display Test**: Verify shots are displayed correctly
5. **Long Run Test**: Verify system runs stably for extended periods

## Future Enhancements

The following can be re-added if needed:
- Shot list retrieval (was removed for simplification)
- Dynamic device scanning (currently targets specific address)
- Connection retry strategies
- Advanced error recovery

## Build Status
✅ Compiles successfully
✅ Architecture preserved
✅ Matches working minimal_test.cpp pattern
