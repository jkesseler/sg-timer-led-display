# Build & Test Guide

## Prerequisites

### Software
```bash
# Install Python 3.7+
python --version

# Install PlatformIO Core
pip install platformio

# Verify installation
pio --version
```

### Hardware
- ESP32-S3 DevKit-C (connected via USB)
- 2x HUB75 LED panels (64x32 each)
- 5V 10A power supply
- Wired according to [HUB75_WIRING.md](HUB75_WIRING.md)
- Compatible timer device (SG Timer or Special Pie M1A2+ Timer)

---

## Build Process

### 1. Clean Build
```bash
cd ScoreDisplay/firmware
pio run --target clean
```

### 2. Compile
```bash
pio run
```

**Expected output**:
```
Processing esp32-s3-devkitc-1 (platform: espressif32; board: esp32-s3-devkitc-1)
...
RAM:   [=         ]  10.2% (used 33472 bytes)
Flash: [===       ]  25.4% (used 417234 bytes)
======================== [SUCCESS] ========================
```

### 3. Upload to ESP32-S3
```bash
# Auto-detect port and upload
pio run --target upload

# Or specify port manually (Windows)
pio run --target upload --upload-port COM3

# Linux/Mac
pio run --target upload --upload-port /dev/ttyUSB0
```

**Expected output**:
```
Connecting.....
Chip is ESP32-S3
...
Writing at 0x00010000... (100 %)
Wrote 417234 bytes at 0x00010000 in 6.2 seconds
Hard resetting via RTS pin...
======================== [SUCCESS] ========================
```

### 4. Monitor Serial Output
```bash
pio device monitor
```

**To exit monitor**: `Ctrl + C`

---

## Testing Procedure

### Test 1: Display Initialization
**Goal**: Verify HUB75 panels are working

**Steps**:
1. Power on 5V supply for panels
2. Upload firmware to ESP32-S3
3. Watch display during boot

**Expected behavior**:
- Display shows "SG TIMER" (green)
- Display shows "READY" (green)
- Message appears for 2 seconds
- Display clears

**Serial output**:
```
=== SG Shot Timer BLE Bridge ===
ESP32-S3 DevKit-C Starting...
[DISPLAY] Initializing HUB75 LED panels...
[DISPLAY] HUB75 panels initialized
```

**If display is blank**: See troubleshooting below

---

### Test 2: BLE Scanning
**Goal**: Verify BLE hardware is working

**Steps**:
1. Keep timer devices OFF initially
2. Observe serial monitor

**Expected serial output**:
```
[SYSTEM] === SG Shot Timer BLE Bridge ===
[SYSTEM] ESP32-S3 DevKit-C Starting...
[DISPLAY] Initializing display...
[BLE] ESP32-S3 BLE Client initialized
[SYSTEM] Ready to scan for timer devices (SG Timer or Special Pie Timer)
[SYSTEM] Application initialized successfully
[SYSTEM] Scanning for timer devices...
[SYSTEM] No compatible timer devices found. Retrying...
```

**Expected display**: Blank (waiting)

---

### Test 3: BLE Connection (SG Timer)
**Goal**: Connect to SG Timer

**Steps**:
1. Turn ON your SG Timer
2. Wait for auto-connection (~10 seconds max)

**Expected serial output**:
```
[SYSTEM] Scanning for timer devices...
[SYSTEM] SG Timer found! Connecting...
[SG-TIMER] Initializing SG Timer device interface
[SG-TIMER] Attempting to connect to SG Timer...
[SG-TIMER] Creating BLE client...
[SG-TIMER] Connecting to device...
[SG-TIMER] Connected! Discovering services...
[SG-TIMER] Service discovered, getting characteristic...
[SG-TIMER] Characteristic found, registering for notifications...
[SG-TIMER] Successfully connected to SG Timer
```

**Expected display**: Still blank (waiting for session)

---

### Test 3B: BLE Connection (Special Pie Timer)
**Goal**: Connect to Special Pie M1A2+ Timer

**Steps**:
1. Turn ON your Special Pie Timer
2. Wait for auto-connection (~10 seconds max)

**Expected serial output**:
```
[SYSTEM] Scanning for timer devices...
[SYSTEM] Special Pie Timer found! Connecting...
[SPECIAL-PIE] Initializing Special Pie Timer device interface
Special Pie Timer found: M1A2+ (xx:xx:xx:xx:xx:xx)
Waiting 2 seconds before connecting...
Attempting connection...
Connected to device!
Service found
FFF1 characteristic found
Registering for notifications...
SUCCESS: Registered for notifications!
Listening for events indefinitely...
[SYSTEM] Successfully connected to Special Pie Timer
```

**Expected display**: Still blank (waiting for session)

---

### Test 4: Session Start
**Goal**: Verify session start event handling

**Steps**:
1. Start a session on timer device (press START button)
2. Observe display and serial

**Expected serial output (SG Timer)**:
```
[DATA] Received notification: 06 00 65 7a 8b 29 00 1e
[SG-TIMER] Event: SESSION_STARTED
[TIMER] Session started: ID 1698012345
[DISPLAY] Waiting for shots...
```

**Expected serial output (Special Pie Timer)**:
```
*** Notification received (6 bytes): F8 F9 34 03 F9 F8
Message Type: 0x34 - SESSION_START
  Session ID: 0x03
[TIMER] Session started: ID 3
```

**Expected display**:
```
┌────────────────────────────────┐
│  READY           [Light Blue]  │
│                                │
│  00:00             [White]     │
│                                │
└────────────────────────────────┘
```

**Colors**:
- "READY" = Light blue
- "00:00" = White, large font

---

### Test 5: Shot Detection
**Goal**: Verify shot events update display

**Steps**:
1. Fire 3-5 shots into timer
2. Watch display update in real-time

**Expected serial output (SG Timer)**:
```
[DATA] Received notification: 0a 04 65 7a 8b 29 00 01 00 00 09 29
[SG-TIMER] Event: SHOT_DETECTED
[SHOT] Shot #1 at 2.345s (split: 0.000s)
[DISPLAY] Shot: 1, Time: 00:02.345

[DATA] Received notification: 0a 04 65 7a 8b 29 00 02 00 00 10 1f
[SG-TIMER] Event: SHOT_DETECTED
[SHOT] Shot #2 at 4.127s (split: 1.782s)
[DISPLAY] Shot: 2, Time: 00:04.127
```

**Expected serial output (Special Pie Timer)**:
```
*** Notification received (10 bytes): F8 F9 36 00 04 24 02 0C F9 F8
Message Type: 0x36 - SHOT_DETECTED
  Shot #2: 4.36
  Split: 0.00
[SHOT] Shot #2 at 4.360s (split: 0.000s)

*** Notification received (10 bytes): F8 F9 36 00 2A 3B 05 0C F9 F8
Message Type: 0x36 - SHOT_DETECTED
  Shot #5: 42.59
  Split: 38.23
[SHOT] Shot #5 at 42.590s (split: 38.230s)
```

**Expected display** (updates with each shot):
```
Shot 1:
┌────────────────────────────────┐
│  SHOT:1          [Yellow]      │
│                                │
│  02.345           [Green]      │
└────────────────────────────────┘

Shot 2:
┌────────────────────────────────┐
│  SHOT:2          [Yellow]      │
│                                │
│  04.127           [Green]      │
└────────────────────────────────┘
```

---

### Test 6: Session End
**Goal**: Verify session end

**Steps**:
1. Stop the session on timer (press STOP)
2. Observe serial output

**Expected serial output (SG Timer with shot list support)**:
```
[DATA] Received notification: 06 03 65 7a 8b 29 00 05
[SG-TIMER] Event: SESSION_STOPPED
[TIMER] Session stopped: ID 1698012345
[DISPLAY] Session End - Total Shots: 5
[SHOT_LIST] Reading shots for session ID: 1698012345
[SHOT_LIST] Shot details:
[SHOT_LIST] #  | Time (s)
[SHOT_LIST] ---|----------
[SHOT_LIST]  0 | 2.345
[SHOT_LIST]  1 | 4.127
[SHOT_LIST]  2 | 6.234
[SHOT_LIST]  3 | 8.456
[SHOT_LIST]  4 | 10.789
[SHOT_LIST] End of shot list
```

**Expected serial output (Special Pie Timer - no shot list)**:
```
*** Notification received (6 bytes): F8 F9 18 0B F9 F8
Message Type: 0x18 - SESSION_STOP
  Session ID: 0x0B
[TIMER] Session stopped: ID 11
```

**Expected display**:
```
┌────────────────────────────────┐
│  SESSION END       [Red]       │
│                                │
│  SHOTS: 5        [Yellow]      │
│  Last: #5         [Green]      │
└────────────────────────────────┘
```

**Note**: Special Pie Timer does not support remote shot list retrieval.

---

### Test 7: Reconnection
**Goal**: Verify auto-reconnect functionality

**Steps**:
1. Turn OFF timer device
2. Wait for disconnect message
3. Turn ON timer after 10 seconds

**Expected serial output**:
```
!!! Connection lost !!!
Will attempt to reconnect...
[SYSTEM] Scanning for timer devices...
...
[SYSTEM] SG Timer found! Connecting...
[SG-TIMER] Successfully connected to SG Timer
```

**OR**

```
!!! Connection lost !!!
Will attempt to reconnect...
[SYSTEM] Scanning for timer devices...
...
[SYSTEM] Special Pie Timer found! Connecting...
[SYSTEM] Successfully connected to Special Pie Timer
```

---

## Troubleshooting

### Display Issues

#### Blank Display
```bash
# Test 1: Check power
# - Measure 5V at panel power connector
# - Verify GND connection between ESP32 and panels

# Test 2: Increase brightness
# Edit src/main.cpp, line ~230:
display->setBrightness8(255); // Max brightness

# Test 3: Add test pattern
# Add after initDisplay() in setup():
display->fillScreen(display->color565(255, 0, 0)); // Red
delay(2000);
```

#### Wrong Colors
```cpp
// Try different panel driver in initDisplay():
mxconfig.driver = HUB75_I2S_CFG::FM6124;    // Option 1
mxconfig.driver = HUB75_I2S_CFG::ICN2038S;  // Option 2
mxconfig.driver = HUB75_I2S_CFG::SHIFTREG;  // Option 3
```

#### Flickering
- Check power supply current rating (needs 10A)
- Reduce brightness temporarily
- Check all wire connections

---

### BLE Issues

#### Cannot Find Timer
```bash
# Verify timer is on and nearby
# Check timer battery level
# Reduce distance (< 5 meters for testing)

# Check which device you have:
# - SG Timer: Look for hardcoded address in TimerApplication.cpp
# - Special Pie: Should auto-detect by service UUID

# Enable more verbose BLE logging:
# Edit platformio.ini, add to build_flags:
-DCORE_DEBUG_LEVEL=5
```

#### Connection Drops
```bash
# Check timer battery
# Reduce distance
# Remove obstacles between devices
# Check for 2.4GHz WiFi interference
```

#### No Events Received
```bash
# For SG Timer: Verify timer is in RO (Review Officer) mode
# For Special Pie: Start a session and fire shots

# Check notification setup in serial:
[SG-TIMER] Successfully connected to SG Timer  # SG Timer
SUCCESS: Registered for notifications!         # Special Pie

# Manual notification test:
# Trigger a shot - should see notification messages
```

---

### Compilation Errors

#### Library Not Found
```bash
# Clear library cache
pio lib install

# Or reinstall library
pio lib install "mrfaptastic/ESP32-HUB75-MatrixPanel-DMA@^3.0.0"
```

#### Insufficient Flash
```bash
# Check available flash
pio run -v

# If needed, reduce features or use partition scheme:
# Add to platformio.ini:
board_build.partitions = huge_app.csv
```

---

## Performance Verification

### Memory Usage
**Target**: < 50% RAM, < 50% Flash

Check after build:
```
RAM:   [=         ]  10.2% (used 33472 bytes)  ✓ Good
Flash: [===       ]  25.4% (used 417234 bytes) ✓ Good
```

### Display Refresh
**Target**: Smooth, no visible flickering

**Test**: Display should be rock-solid during shot updates

### BLE Latency
**Target**: < 100ms from shot to display update

**Test**: Shot appears on display almost instantly (< 1/10 second)

### Shot List Speed
**Target**: ~10 shots/second retrieval

**Test**: 50 shots should download in ~5 seconds

---

## Quick Commands

```bash
# Build only
pio run

# Upload only (after build)
pio run --target upload

# Build + Upload
pio run -t upload

# Monitor only
pio device monitor

# Upload + Monitor (most common)
pio run -t upload && pio device monitor

# Clean everything
pio run -t clean

# Update libraries
pio lib update

# Show device ports
pio device list
```

---

## Test Checklist

### SG Timer
- [ ] Display shows startup message
- [ ] BLE scan finds SG Timer by address
- [ ] BLE connects to SG Timer automatically
- [ ] "READY" and "00:00" appears when session starts
- [ ] Display updates with each shot
- [ ] Shot numbers increment correctly
- [ ] Times display accurately (verify with timer)
- [ ] Session end shows correct total
- [ ] Shot list downloads to serial
- [ ] Auto-reconnects after disconnect
- [ ] No memory warnings in build
- [ ] No crashes or reboots during session

### Special Pie Timer
- [ ] Display shows startup message
- [ ] BLE scan finds Special Pie Timer by service UUID
- [ ] BLE connects to Special Pie Timer automatically
- [ ] Session start notification received (0x34)
- [ ] Display updates with each shot
- [ ] Shot numbers increment correctly
- [ ] Times display accurately (verify with timer)
- [ ] Split times calculated correctly
- [ ] Session stop notification received (0x18)
- [ ] Auto-reconnects after disconnect
- [ ] No memory warnings in build
- [ ] No crashes or reboots during session

---

## Debug Mode

Enable maximum logging:

```cpp
// In platformio.ini, modify build_flags:
build_flags =
    -DCORE_DEBUG_LEVEL=5           // Max ESP32 logging
    -DCONFIG_ARDUHAL_LOG_COLORS=1   // Colored logs
```

Rebuild and monitor:
```bash
pio run -t upload && pio device monitor
```
