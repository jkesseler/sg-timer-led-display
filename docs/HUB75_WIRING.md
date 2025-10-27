# HUB75 Panel Pinout Reference

## HUB75 Connector Pinout (Top View)

```
╔═══════════════════════════════════════╗
║  R1  G1  B1  GND  R2  G2   B2  GND    ║  ← Top row
║   A   B   C   D   CLK LAT  OE  GND    ║  ← Bottom row
╚═══════════════════════════════════════╝
```

## Pin Descriptions

### Data Pins
- **R1, G1, B1**: RGB data for top half of panel
- **R2, G2, B2**: RGB data for bottom half of panel

### Row Selection (Multiplexing)
- **A, B, C, D, E**: Select which row to illuminate (5 pins = 32 rows)
  - A+B+C+D for 1/16 scan (16 rows)
  - A+B+C+D+E for 1/32 scan (32 rows)

### Control Pins
- **CLK**: Clock signal for shift registers
- **LAT**: Latch signal to transfer data to display
- **OE**: Output Enable (active low, controls brightness via PWM)

### Power & Ground
- **GND**: Ground (multiple pins)
- **5V**: Power input (not on all HUB75 connectors)

## ESP32-S3 Complete Wiring Table

| Function | HUB75 Pin | Wire Color | ESP32-S3 Pin | Notes |
|----------|-----------|------------|--------------|-------|
| Red (top) | R1 | Red | GPIO 25 | Top half red data |
| Green (top) | G1 | Green | GPIO 26 | Top half green data |
| Blue (top) | B1 | Blue | GPIO 27 | Top half blue data |
| Red (bottom) | R2 | Red/Brown | GPIO 14 | Bottom half red data |
| Green (bottom) | G2 | Green/Brown | GPIO 12 | Bottom half green data |
| Blue (bottom) | B2 | Blue/Brown | GPIO 13 | Bottom half blue data |
| Row Select A | A | Yellow | GPIO 23 | Row addr bit 0 |
| Row Select B | B | Orange | GPIO 19 | Row addr bit 1 |
| Row Select C | C | Purple | GPIO 5 | Row addr bit 2 |
| Row Select D | D | Gray | GPIO 17 | Row addr bit 3 |
| Row Select E | E | White | GPIO 18 | Row addr bit 4 |
| Clock | CLK | Black | GPIO 16 | Shift clock |
| Latch | LAT | Brown | GPIO 4 | Data latch |
| Output Enable | OE | Pink | GPIO 15 | Brightness PWM |
| Ground | GND | Black | GND | Common ground |

## Power Connection Diagram

```
┌─────────────────┐
│  5V Power Supply│
│   (5V, 10A)     │
└────┬────┬───────┘
     │    │
     │    └─────────────┐
     │                  │
     ▼                  ▼
┌────────────┐    ┌────────────┐
│  Panel 1   │    │  Panel 2   │
│  (64x32)   │───▶│  (64x32)   │
└──────┬─────┘    └──────┬─────┘
       │                 │
       └────────┬────────┘
                │
                ▼
           Common GND ◄────── ESP32-S3 GND
```

**Important**:
- ESP32-S3 is powered separately (USB or 5V pin)
- Panels powered by dedicated 5V supply
- **MUST** connect grounds together (common ground)

## Cable Connection Order

### 1. First Panel (Connected to ESP32-S3)
```
ESP32-S3 GPIOs ──► Panel 1 INPUT (HUB75 connector)
```

### 2. Second Panel (Chained from First)
```
Panel 1 OUTPUT ──► Panel 2 INPUT
```

### 3. Power Distribution
```
5V PSU (+) ──► Panel 1 (+5V) ──► Panel 2 (+5V)
5V PSU (GND) ──► Panel 1 (GND) ──► Panel 2 (GND) ──► ESP32 (GND)
```

## Custom Pin Configuration

If you need to use different GPIO pins, modify the library initialization in `initDisplay()`:

```cpp
HUB75_I2S_CFG mxconfig(
  PANEL_WIDTH,
  PANEL_HEIGHT,
  PANEL_CHAIN
);

// Custom pin assignments
mxconfig.gpio.r1 = 25;   // Red top
mxconfig.gpio.g1 = 26;   // Green top
mxconfig.gpio.b1 = 27;   // Blue top
mxconfig.gpio.r2 = 14;   // Red bottom
mxconfig.gpio.g2 = 12;   // Green bottom
mxconfig.gpio.b2 = 13;   // Blue bottom
mxconfig.gpio.a = 23;    // Row A
mxconfig.gpio.b = 19;    // Row B
mxconfig.gpio.c = 5;     // Row C
mxconfig.gpio.d = 17;    // Row D
mxconfig.gpio.e = 18;    // Row E
mxconfig.gpio.lat = 4;   // Latch
mxconfig.gpio.oe = 15;   // Output Enable
mxconfig.gpio.clk = 16;  // Clock
```

## Panel Physical Layout

### Single Panel (64x32)
```
    0                           63 (X axis)
  0 ┌─────────────────────────────┐
    │                             │
    │         64 x 32             │
    │                             │
 31 └─────────────────────────────┘
(Y axis)
```

### Dual Panel Chain (128x32)
```
    0                          127 (X axis)
  0 ┌──────────────┬──────────────┐
    │   Panel 1    │   Panel 2    │
    │   (64x32)    │   (64x32)    │
 31 └──────────────┴──────────────┘
(Y axis)
```

## Testing Your Connection

Upload this test code to verify all pins are working:

```cpp
// In setup(), after initDisplay()
display->fillScreen(display->color565(255, 0, 0)); // Red
delay(1000);
display->fillScreen(display->color565(0, 255, 0)); // Green
delay(1000);
display->fillScreen(display->color565(0, 0, 255)); // Blue
delay(1000);
display->clearScreen();
```

If you see red, green, then blue flashes, your wiring is correct!

## Common Issues & Solutions

| Problem | Likely Cause | Solution |
|---------|--------------|----------|
| Entire panel blank | No power / OE not connected | Check 5V supply and OE pin |
| Half panel works | R2/G2/B2 not connected | Check bottom half data pins |
| Wrong colors | R/G/B pins swapped | Verify RGB pin connections |
| Horizontal stripes | Row select pins wrong | Check A/B/C/D/E pins |
| Flickering | Power insufficient | Use larger power supply |
| Random pixels | Clock/Latch issue | Check CLK and LAT connections |
| Dim display | OE or brightness low | Increase brightness value |

## Pin Usage Conflicts

**Avoid these ESP32-S3 pins** (used by other functions):
- GPIO 0: Boot mode selection
- GPIO 43, 44: UART0 (USB Serial)
- GPIO 45, 46: System pins

**Safe to use for HUB75**:
- GPIO 4, 5, 12-19, 21, 23, 25-27, 33-39

**Default pins**

| HUB75 Pin | ESP32-S3 GPIO |
|-----------|---------------|
| R1        | 4             |
| G1        | 5             |
| B1        | 6             |
| R2        | 7             |
| G2        | 15            |
| B2        | 16            |
| A        | 18            |
| B        | 8              |
| C       | 3              |
| D       | 42             |
| E       | -1             | // required for 1/32 scan panels, like 64x64. Any available pin would do, i.e. IO32
| LAT     | 40             |
| OE      | 2              |
| CLK     | 41       |



## Shopping List

- [ ] 2x HUB75 LED Matrix Panels (64x32)
- [ ] 1x ESP32-S3 DevKit-C (16MB Flash, PSRAM)
- [ ] 1x 5V 10A Power Supply
- [ ] 2x HUB75 IDC cables (16-pin, usually included with panels)
- [ ] Jumper wires (M-F) for ESP32 to HUB75 connection
- [ ] (Optional) HUB75 breakout board for easier ESP32 connection

## Additional Resources

- [ESP32-HUB75-MatrixPanel-DMA Library](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA)
- [ESP-32S3 Notes](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/tree/master/src/platforms/esp32s3)
- [HUB75 Interface Specification](https://www.google.com/search?q=HUB75+interface+specification)

- SG Timer Documentation: `docs/sg_timer_public_bt_api_32.md`
