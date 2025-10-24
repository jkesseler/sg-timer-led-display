# HUB75 LED Display Setup Guide

## Hardware Configuration

### Display Specifications
- **Display Type**: HUB75-D LED Matrix Panels
- **Panel Count**: 2 panels (chained)
- **Individual Panel Size**: 64x32 pixels
- **Total Display Size**: 128x32 pixels
- **Interface**: HUB75 (6-bit color depth)

### ESP32-S3 DevKit-C Wiring

The HUB75 panels connect to the ESP32-S3 using the I2S parallel interface. Below is the default pin mapping:

| HUB75 Pin | ESP32-S3 Pin | Description |
|-----------|--------------|-------------|
| R1 | GPIO 25 | Red data (top half) |
| G1 | GPIO 26 | Green data (top half) |
| B1 | GPIO 27 | Blue data (top half) |
| R2 | GPIO 14 | Red data (bottom half) |
| G2 | GPIO 12 | Green data (bottom half) |
| B2 | GPIO 13 | Blue data (bottom half) |
| A | GPIO 23 | Row select A |
| B | GPIO 19 | Row select B |
| C | GPIO 5 | Row select C |
| D | GPIO 17 | Row select D |
| E | GPIO 18 | Row select E (configured in code) |
| CLK | GPIO 16 | Clock signal |
| LAT | GPIO 4 | Latch signal |
| OE | GPIO 15 | Output enable |
| GND | GND | Ground |

**Note**: The E pin is configured as GPIO 18 in the `initDisplay()` function.

### Power Requirements

- Each 64x32 panel draws approximately **2-4A at 5V** (depending on brightness and content)
- **Total power needed**: 4-8A at 5V for both panels
- Use a dedicated 5V power supply (e.g., 5V 10A)
- **DO NOT** power the panels from the ESP32-S3 USB power
- Connect panel power and ESP32-S3 GND together (common ground)

### Panel Chaining

Connect the panels in series:
1. Connect first panel's HUB75 input to ESP32-S3
2. Connect second panel's HUB75 input to first panel's output
3. Connect both panels to the 5V power supply

## Display Modes

### 1. Session Active - Waiting (No Shots)
**Trigger**: `SESSION_STARTED` event received, no `SHOT_DETECTED` yet

```
┌────────────────────────────────┐
│       READY                    │
│                                │
│       00:00                    │
└────────────────────────────────┘
```

**Display Elements**:
- Text: "READY" (light blue)
- Time: "00:00" (white, large font)

### 2. Session Active - Shot Detected
**Trigger**: `SHOT_DETECTED` event during active session

```
┌────────────────────────────────┐
│  SHOT:5                        │
│                                │
│  12.345                        │
└────────────────────────────────┘
```

**Display Elements**:
- Shot number (yellow, medium font)
- Shot time in seconds.milliseconds (green, large font)

If time exceeds 1 minute:
```
┌────────────────────────────────┐
│  SHOT:12                       │
│                                │
│  01:23.4                       │
└────────────────────────────────┘
```

### 3. Session Ended
**Trigger**: `SESSION_STOPPED` event

```
┌────────────────────────────────┐
│     SESSION END                │
│                                │
│    SHOTS: 15                   │
│    Last: #15                   │
└────────────────────────────────┘
```

**Display Elements**:
- "SESSION END" (red, small font)
- Total shots count (yellow, medium font)
- Last shot number (green, small font)

**Additional Behavior**:
- After session ends, the system reads the `SHOT_LIST` characteristic
- Full shot list is printed to serial console for analysis

## Code Configuration

### Brightness Adjustment

In `initDisplay()`:
```cpp
display->setBrightness8(90); // 0-255 (current: 90)
```

Adjust brightness based on ambient lighting:
- Indoor: 50-90
- Outdoor/Bright: 150-255
- Night/Dark: 20-50

### Panel Driver Selection

Current driver: `FM6126A`

If you have different panels, try these drivers:
```cpp
mxconfig.driver = HUB75_I2S_CFG::FM6124;  // For FM6124 panels
mxconfig.driver = HUB75_I2S_CFG::ICN2038S; // For ICN2038S panels
mxconfig.driver = HUB75_I2S_CFG::SHIFTREG; // Generic shift register
```

### Display Dimensions

To change panel configuration, modify these constants:
```cpp
#define PANEL_WIDTH 64      // Width of one panel
#define PANEL_HEIGHT 32     // Height of one panel
#define PANEL_CHAIN 2       // Number of panels chained
```

## BLE Characteristics Used

### EVENT Characteristic (Notifications)
**UUID**: `75200001-14D2-4CDA-8B6B-697C554C9311`

Receives real-time events:
- `SESSION_STARTED` (0x00) → Trigger waiting display
- `SHOT_DETECTED` (0x04) → Update shot display
- `SESSION_STOPPED` (0x03) → Show session end

### SHOT_LIST Characteristic (Read)
**UUID**: `75200004-14D2-4CDA-8B6B-697C554C9311`

After session ends:
1. Write session ID (4 bytes, Big Endian)
2. Read repeatedly to get all shots
3. Each read returns: shot_number (2 bytes) + shot_time (4 bytes)
4. End of list marked by `0xFFFFFFFF` in shot_time field

## Troubleshooting

### Display Shows Nothing
1. Check power supply (5V, sufficient current)
2. Verify common ground between ESP32-S3 and panel power
3. Check HUB75 cable connections
4. Try increasing brightness: `display->setBrightness8(255);`

### Display Shows Garbage/Noise
1. Wrong panel driver selected → try different drivers
2. Bad ribbon cable connection
3. Insufficient power supply
4. Clock phase issue → toggle `mxconfig.clkphase`

### Display Flickers
1. Power supply unstable or insufficient current
2. Long wires causing interference → use shorter cables
3. Try different GPIO pins for E pin

### Colors Wrong
1. R1/G1/B1 or R2/G2/B2 pins swapped
2. Panel driver mismatch
3. Cable not fully inserted

## Serial Monitor Output

When connected and running, you'll see:
```
=== SG Shot Timer BLE Bridge ===
ESP32-S3 DevKit-C Starting...
[DISPLAY] Initializing HUB75 LED panels...
[DISPLAY] HUB75 panels initialized
[BLE] ESP32-S3 BLE Client initialized
[SCAN] Starting BLE scan for SG Shot Timer...
[SCAN] Found SG Shot Timer: SG-SST4A12345
[BLE] Connected to SG Shot Timer
[TIMER] === SESSION STARTED === (ID: 1698012345, Delay: 3.0s)
[DISPLAY] Waiting for shots...
[TIMER] SHOT: 1 - TIME: 2.345s | SPLIT: 0.000s
[DISPLAY] Shot: 1, Time: 00:02.345
[TIMER] === SESSION STOPPED === (ID: 1698012345, Total Shots: 5)
[DISPLAY] Session End - Total Shots: 5
[SHOT_LIST] Reading shots for session ID: 1698012345
```

## Performance Notes

- Display updates are non-blocking
- BLE notifications trigger immediate display updates
- Shot list reading happens asynchronously after session end
- Maximum tested: 100 shots per session
- Display refresh rate: ~60Hz (handled by DMA library)
