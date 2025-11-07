# Testing Guide - Simplified Firmware

## Quick Start

### 1. Upload Firmware
```bash
cd d:\personal-workspace\sg-timer-LED-display
pio run --target upload
```

### 2. Monitor Serial Output
```bash
pio device monitor
```

## Expected Behavior

### On Startup
```
=== SG Shot Timer BLE Bridge ===
ESP32-S3 DevKit-C Starting...
[SG-TIMER] Initializing SG Timer device interface
...
--- Starting device scan ---
Scanning for 10 seconds...
```

### When Device Found
```
Target device found!
Waiting 2 seconds before connecting...
Attempting connection...
Connected to device!
Service found
EVENT characteristic found
Registering for notifications...
SUCCESS: Registered for notifications!
Listening for events indefinitely...
```

### When Session Starts
```
*** Notification received (8 bytes): 08 00 00 00 00 01 00 00
Event ID: 0x00 - SESSION_STARTED
  Session ID: 1
  Start Delay: 0.0 seconds
```

### When Shot Detected
```
*** Notification received (12 bytes): 0C 04 00 00 00 01 00 00 00 00 0A 1C
Event ID: 0x04 - SHOT_DETECTED
  Shot #1: 2:58
```

### When Session Ends
```
*** Notification received (8 bytes): 08 03 00 00 00 01 00 05
Event ID: 0x03 - SESSION_STOPPED
  Session ID: 1
  Total Shots: 5
  Last Shot: #5: 15:42
```

### Connection Lost
```
!!! Connection lost !!!
Will attempt to reconnect...

--- Starting device scan ---
```

## Troubleshooting

### Device Not Found
**Symptom:** "Target device not found. Retrying in 5 seconds..."

**Possible Causes:**
- Timer is off or out of range
- Wrong device address in code (should be `dd:0e:9d:04:72:c3`)
- BLE interference from other devices

**Solutions:**
1. Verify timer is powered on
2. Move ESP32 closer to timer
3. Check serial output for other device addresses found during scan

### Connection Fails
**Symptom:** "ERROR: Failed to connect"

**Possible Causes:**
- Timer already connected to another device
- Weak signal
- BLE stack issues

**Solutions:**
1. Power cycle the timer
2. Restart ESP32
3. Move devices closer together

### No Notifications
**Symptom:** Connected but no events received

**Possible Causes:**
- Notification registration failed
- Timer not in active session
- Characteristic UUID mismatch

**Solutions:**
1. Check for "SUCCESS: Registered for notifications!" message
2. Start a session on the timer
3. Verify SERVICE_UUID and CHARACTERISTIC_UUID in code

### Frequent Disconnections
**Symptom:** Connection drops repeatedly

**Possible Causes:**
- Signal interference
- Power issues
- BLE congestion

**Solutions:**
1. Check power supply to ESP32
2. Move away from WiFi routers
3. Reduce distance between devices

## Debug Output

### Connection State Changes
Watch for state transitions:
- `DISCONNECTED` → `SCANNING` → `CONNECTING` → `CONNECTED`

### Heartbeat Messages
Every 30 seconds when connected:
```
[SG-TIMER] Connected - waiting for events...
```

### Health Checks
Every 5 seconds:
```
[HEALTH] System uptime: 125000 ms, Free heap: 245632 bytes
```

## Performance Metrics

### Normal Operation
- **Connection Time**: 10-15 seconds (includes 10s scan)
- **Reconnection Time**: 5-15 seconds
- **Memory Usage**: ~45 KB RAM, ~965 KB Flash
- **Notification Latency**: <50ms

### Memory Health
- Free heap should stay above 200 KB
- If heap drops below 100 KB, investigate memory leaks

## Common Test Scenarios

### Test 1: Basic Connection
1. Power on timer
2. Power on ESP32
3. Wait for connection
4. ✅ Should connect within 15 seconds

### Test 2: Shot Detection
1. Connect to timer
2. Start par time session on timer
3. Fire shots
4. ✅ Should see SHOT_DETECTED events
5. ✅ Display should update with shot times

### Test 3: Session Management
1. Start session
2. Fire some shots
3. Pause session (if timer supports it)
4. Resume session
5. Stop session
6. ✅ Should see all session events

### Test 4: Reconnection
1. Connect to timer
2. Power off timer
3. Wait for "Connection lost"
4. Power on timer
5. ✅ Should reconnect automatically within 20 seconds

### Test 5: Long Running
1. Connect to timer
2. Leave running for 1 hour
3. ✅ Should remain connected
4. ✅ Heartbeat messages every 30 seconds
5. ✅ Free heap should remain stable

## Configuration Options

### Change Target Device
Edit `SGTimerDevice.cpp`:
```cpp
const char* SGTimerDevice::TARGET_DEVICE_ADDRESS = "dd:0e:9d:04:72:c3";
```

### Adjust Reconnection Delay
Edit `attemptConnection()` throttle:
```cpp
if (now - lastReconnectAttempt < 5000) {  // Change 5000 to desired ms
```

### Adjust Heartbeat Interval
Edit `update()` heartbeat check:
```cpp
if (millis() - lastHeartbeat > 30000) {  // Change 30000 to desired ms
```

## Success Criteria

✅ Firmware compiles without errors
✅ ESP32 boots and initializes BLE
✅ Device found within 15 seconds
✅ Connection established successfully
✅ Notifications registered
✅ SESSION_STARTED events received
✅ SHOT_DETECTED events received
✅ SESSION_STOPPED events received
✅ Display updates with shot data
✅ Reconnects automatically after disconnection
✅ Runs stably for extended periods
