# ESP32-S3 Firmware — Hardware Reference

## Supported board

**ESP32-S3 DevKit-C** — `esp32-s3-devkitc-1` target in PlatformIO  
Recommended variant: **N16R8V** (16 MB flash, 8 MB PSRAM).

---

## HUB75 connector pinout

```
╔═══════════════════════════════════════╗
║  R1  G1  B1  GND  R2  G2   B2  GND    ║  ← top row
║   A   B   C   D   CLK LAT  OE  GND    ║  ← bottom row
╚═══════════════════════════════════════╝
```

| Signal group | Pins | Function |
|---|---|---|
| Data (top half) | R1, G1, B1 | RGB bits for rows 0–15 |
| Data (bottom half) | R2, G2, B2 | RGB bits for rows 16–31 |
| Row select | A, B, C, D, E | 5-bit row address (1/32 scan) |
| Control | CLK, LAT, OE | Shift clock, latch, output enable (brightness PWM) |
| Power | GND | Multiple ground pins |

---

## ESP32-S3 GPIO mapping

| Function | HUB75 pin | ESP32-S3 GPIO | Notes |
|---|---|---|---|
| Red (top) | R1 | 25 | |
| Green (top) | G1 | 26 | |
| Blue (top) | B1 | 27 | |
| Red (bottom) | R2 | 14 | |
| Green (bottom) | G2 | 12 | |
| Blue (bottom) | B2 | 13 | |
| Row select A | A | 23 | |
| Row select B | B | 19 | |
| Row select C | C | 5 | |
| Row select D | D | 17 | |
| Row select E | E | 18 | Needed for 1/32-scan 32-row panels |
| Clock | CLK | 16 | |
| Latch | LAT | 4 | |
| Output enable | OE | 15 | Active-low, controls brightness via PWM |
| Ground | GND | GND | Common ground — **must connect to panel PSU GND** |

Pin assignments are configured in `DisplayManager::initDisplay()` via `HUB75_I2S_CFG::gpio` fields, not in `common.h`. To use different GPIOs, modify that struct directly:

```cpp
HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANEL_CHAIN);
mxconfig.gpio.r1 = 25;
mxconfig.gpio.g1 = 26;
// …
mxconfig.gpio.e  = 18;
```

### GPIOs to avoid on ESP32-S3

| GPIO | Reserved for |
|---|---|
| 0 | Boot mode selection |
| 43, 44 | UART0 / USB serial |
| 45, 46 | System strapping pins |

---

## Panel configuration

| Parameter | Value | `common.h` macro |
|---|---|---|
| Individual panel width | 64 px | `PANEL_WIDTH` |
| Individual panel height | 32 px | `PANEL_HEIGHT` |
| Panels chained | 2 | `PANEL_CHAIN` |
| Total display | 128 × 32 px | — |

### Coordinate system

```
     X-axis: 0 ───────────────────────────► 127
   Y │
   - │  ┌─────────────────────────────────┐
   a │  │                                 │
   x │  │       128 × 32 display area     │
   i │  │                                 │
   s │  └─────────────────────────────────┘
   : ▼
    31
```

Origin (0, 0) = top-left corner.

---

## Panel chaining and power

```
                        ESP32-S3 GPIOs
                              │
                              ▼
                      ┌───────────────┐
  5 V PSU (+) ──────► │   Panel 1     │ OUTPUT ──► Panel 2 INPUT
  5 V PSU (GND) ────► │   64×32       │
                      └───────────────┘
                              │
                     (both panel GNDs)
                              │
                              ▼
                       ESP32-S3 GND  (common ground)
```

1. Connect ESP32-S3 GPIOs to **Panel 1 INPUT** (HUB75 connector).
2. Connect **Panel 1 OUTPUT** to **Panel 2 INPUT**.
3. Power both panels from a **dedicated** 5 V supply — never from the ESP32-S3 USB rail.
4. Bridge the panel PSU GND and ESP32-S3 GND (common ground).

---

## Power requirements

| Load | Current draw (approx.) |
|---|---|
| Both panels idle (all black) | ~400 mA |
| Typical shot display at brightness 90 | ~2 A |
| All pixels white at brightness 255 | ~8 A |

**Recommended supply: 5 V / 10 A.**  
Add a 1 000–2 000 µF capacitor across VCC and GND on the back of each panel to prevent flicker from inrush current spikes.

---

## Panel driver selection

Current driver: **FM6126A**.

If your panels use a different driver chip, change the driver in `DisplayManager::initDisplay()`:

```cpp
mxconfig.driver = HUB75_I2S_CFG::FM6124;    // FM6124 panels
mxconfig.driver = HUB75_I2S_CFG::ICN2038S;  // ICN2038S panels
mxconfig.driver = HUB75_I2S_CFG::SHIFTREG;  // Generic shift register
```

---

## Brightness

Default brightness: **128** (`DEFAULT_BRIGHTNESS` in `common.h`). Set in code via `display->setBrightness8(value)` (0–255).

| Environment | Recommended brightness |
|---|---|
| Dark room | 20–50 |
| Indoor office | 70–100 |
| Bright indoor | 120–150 |
| Outdoor shade | 180–220 |
| Direct sunlight | 240–255 |

---

## Wiring test

After wiring, flash `tools-led-matrix` and run this snippet in `setup()` to confirm all connections:

```cpp
display->fillScreen(display->color565(255, 0, 0));  delay(1000);  // Red
display->fillScreen(display->color565(0, 255, 0));  delay(1000);  // Green
display->fillScreen(display->color565(0, 0, 255));  delay(1000);  // Blue
display->clearScreen();
```

All three solid-colour flashes → wiring is correct.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Entire panel blank | No power or OE not connected | Check 5 V supply and OE pin |
| Top half works, bottom half blank | R2/G2/B2 not connected | Check bottom-half data pins |
| Wrong colours | R/G/B pins swapped | Verify RGB pin order |
| Horizontal stripes | Row-select pins wrong | Check A/B/C/D/E assignments |
| Flickering | Insufficient or noisy power | Use larger PSU; add decoupling capacitors |
| Random pixels | Clock or latch issue | Check CLK and LAT connections |
| Dim display | OE or brightness too low | Increase `setBrightness8()` value |
| Half panel shows mirrored image | Wrong clock phase | Toggle `mxconfig.clkphase` |

---

## Shopping list

- 2× HUB75 LED matrix panel (64×32)
- 1× ESP32-S3 DevKit-C (16 MB flash, PSRAM)
- 1× 5 V / 10 A power supply
- 2× HUB75 IDC ribbon cables (16-pin, usually shipped with panels)
- Jumper wires (M-F) for ESP32 ↔ HUB75 connection
- *(Optional)* HUB75 breakout board for cleaner connections

---

## Further reading

- [ESP32-HUB75-MatrixPanel-DMA library](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA)
- [ESP32-S3 platform notes for the library](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/tree/master/src/platforms/esp32s3)
