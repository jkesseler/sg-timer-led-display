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
1. Keep SG Timer OFF initially
2. Observe serial monitor

**Expected serial output**:
```
[BLE] ESP32-S3 BLE Client initialized
[SCAN] Starting BLE scan for SG Shot Timer...
[SCAN] Found device: Device_Name (RSSI: -45)
[SCAN] Found device: Another_Device (RSSI: -67)
...
[SCAN] Scan completed. Found 5 devices
[SCAN] SG Shot Timer not found. Will retry in 5 seconds...
```

**Expected display**: Blank (waiting)

---

### Test 3: BLE Connection
**Goal**: Connect to SG Timer

**Steps**:
1. Turn ON your SG Timer
2. Wait for auto-connection (~10 seconds max)

**Expected serial output**:
```
[SCAN] Starting BLE scan for SG Shot Timer...
[SCAN] Found device: SG-SST4A12345 (RSSI: -55)
[SCAN] Found SG Shot Timer: SG-SST4A12345
[BLE] Connecting to device at address: xx:xx:xx:xx:xx:xx
[BLE] Connected to server, discovering services...
[BLE] Found service, getting characteristic...
[BLE] Found characteristic, setting up notifications...
[BLE] Notification callback registered
[BLE] === Ready to receive shot timer data ===
[BLE] Connected to SG Shot Timer
```

**Expected display**: Still blank (waiting for session)

---

### Test 4: Session Start
**Goal**: Verify session start event handling

**Steps**:
1. Start a session on SG Timer (press START button)
2. Observe display and serial

**Expected serial output**:
```
[DATA] Received 8 bytes: 06 00 65 7a 8b 29 00 1e
[PARSE] Event ID: 0 (0x00), Length: 6
[TIMER] === SESSION STARTED === (ID: 1698012345, Delay: 3.0s)
[DISPLAY] Waiting for shots...
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

**Expected serial output**:
```
[DATA] Received 12 bytes: 0a 04 65 7a 8b 29 00 01 00 00 09 29
[PARSE] Event ID: 4 (0x04), Length: 10
[TIMER] SHOT: 1 - TIME: 2.345s | SPLIT: 0.000s
[DISPLAY] Shot: 1, Time: 00:02.345

[DATA] Received 12 bytes: 0a 04 65 7a 8b 29 00 02 00 00 10 1f
[PARSE] Event ID: 4 (0x04), Length: 10
[TIMER] SHOT: 2 - TIME: 4.127s | SPLIT: 1.782s
[DISPLAY] Shot: 2, Time: 00:04.127
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
**Goal**: Verify session end and shot list retrieval

**Steps**:
1. Stop the session on SG Timer (press STOP)
2. Wait for shot list to download

**Expected serial output**:
```
[DATA] Received 8 bytes: 06 03 65 7a 8b 29 00 05
[PARSE] Event ID: 3 (0x03), Length: 6
[TIMER] === SESSION STOPPED === (ID: 1698012345, Total Shots: 5)
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

**Expected display**:
```
┌────────────────────────────────┐
│  SESSION END       [Red]       │
│                                │
│  SHOTS: 5        [Yellow]      │
│  Last: #5         [Green]      │
└────────────────────────────────┘
```

---

### Test 7: Reconnection
**Goal**: Verify auto-reconnect functionality

**Steps**:
1. Turn OFF SG Timer
2. Wait for disconnect message
3. Turn ON SG Timer after 10 seconds

**Expected serial output**:
```
[BLE] Disconnected from SG Shot Timer
[BLE] Attempting to reconnect...
[SCAN] Starting BLE scan for SG Shot Timer...
...
[SCAN] Found SG Shot Timer: SG-SST4A12345
[BLE] Connected to SG Shot Timer
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
# Verify timer is on
# Check timer battery level
# Reduce distance (< 5 meters for testing)

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
# Verify timer is in RO (Review Officer) mode
# Check notification setup in serial:
[BLE] Notification callback registered  # Should see this

# Manual notification test:
# Trigger a shot - should see [DATA] Received... message
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

- [ ] Display shows startup message
- [ ] BLE scan finds nearby devices
- [ ] BLE connects to SG Timer automatically
- [ ] "00:00" appears when session starts
- [ ] Display updates with each shot
- [ ] Shot numbers increment correctly
- [ ] Times display accurately (verify with timer)
- [ ] Session end shows correct total
- [ ] Shot list prints to serial
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

---

## Success Criteria

✅ **Minimum Viable Product**:
- Connects to SG Timer
- Shows shot number and time
- Updates in real-time
- Readable from 5 meters

✅ **Production Ready**:
- All tests pass
- No display glitches
- Reliable reconnection
- Shot list downloads correctly
- Runs for 8+ hours continuously
