# ESP32-S3 Firmware — Display Reference

## State machine

```
┌──────────┐
│  STARTUP │  (marquee scroll, STARTUP_MESSAGE_DELAY: 5 s prod / 1 s debug)
└────┬─────┘
     ↓
┌─────────────┐    ← BLE not yet found
│ DISCONNECTED│◄─────────────────────────────────────────┐
└────┬────────┘                                           │
     ↓ scan begins                                        │
┌─────────┐                                               │ disconnect
│SCANNING │                                               │
└────┬────┘                                               │
     ↓ connecting                                         │
┌──────────┐                                              │
│CONNECTING│                                              │
└────┬─────┘                                              │
     ↓ connected                                          │
┌─────────┐                                               │
│CONNECTED│                                               │
└────┬────┘                                               │
     ↓ SESSION_STARTED                                    │
┌───────────┐                                             │
│COUNTDOWN  │  (only for SG Timer with start delay > 0)  │
└────┬──────┘                                             │
     ↓ countdown complete / SESSION_STARTED (no delay)   │
┌──────────────────┐                                      │
│WAITING_FOR_SHOTS │  "READY" + "00:00"                   │
└────┬─────────────┘                                      │
     ↓ SHOT_DETECTED                                      │
┌────────────┐                                            │
│SHOWING_SHOT│  "SHOT:N" + time                           │
└────┬───────┘                                            │
     ↓ SESSION_STOPPED                                    │
┌──────────────┐                                          │
│ SESSION_ENDED│ ─────────────────────────────────────────┘
└──────────────┘
```

---

## Visual layouts

### STARTUP

```
┌────────────────────────────────┐
│  J.K. PewPew Timer Bridge      │  ← marquee scrolls left, green
└────────────────────────────────┘
```

Configurable text via WiFi portal (`STARTUP_TEXT` default, overridden by NVS).

---

### DISCONNECTED / SCANNING / CONNECTING / CONNECTED

Display is blank (black) during all connection-state transitions. Status is logged to serial.

---

### WAITING_FOR_SHOTS  (`SESSION_STARTED`, no shots yet)

```
┌────────────────────────────────┐
│  READY           [light blue]  │
│                                │
│  00:00               [white]   │
└────────────────────────────────┘
```

---

### SHOWING_SHOT  (`SHOT_DETECTED`)

Under 60 s:
```
┌────────────────────────────────┐
│  SHOT:1           [yellow]     │
│                                │
│  02.345            [green]     │
└────────────────────────────────┘
```

Over 60 s:
```
┌────────────────────────────────┐
│  SHOT:12          [yellow]     │
│                                │
│  01:23.4           [green]     │
└────────────────────────────────┘
```

---

### SESSION_ENDED  (`SESSION_STOPPED`)

```
┌────────────────────────────────┐
│  SESSION END         [red]     │
│                                │
│  SHOTS: 15        [yellow]     │
│  Last: #15         [green]     │
└────────────────────────────────┘
```

When a `session/up-next` message arrives for this session, the screen alternates every 10 s with:

```
┌────────────────────────────────┐
│  NEXT: Jan Jansen              │
│  ON DECK: Piet de Vries        │
└────────────────────────────────┘
```

Labels are yellow and names are white. Without an on-deck shooter, the second line shows `ON DECK: -`. Names longer than the panel are clipped at the right edge.

---

## Color codes (RGB565)

| Colour | RGB | Used for |
|---|---|---|
| Green | (0, 255, 0) | Shot times, ready status |
| Yellow | (255, 255, 0) | Shot numbers, session totals |
| Light blue | (100, 100, 255) | Waiting / ready label |
| Red | (255, 0, 0) | Session end header |
| White | (255, 255, 255) | "00:00" placeholder |

---

## Time display format

### Under 60 seconds

```
SS.mmm          e.g.  02.345  (2 seconds, 345 ms)
```

### 60 seconds and over

```
MM:SS.t         e.g.  01:23.4  (1 minute, 23.4 seconds)
```

Only one decimal place is shown above 60 s to fit the panel width.

Implementation: `DisplayManager::formatTime()` and `DisplayManager::formatSplitTime()` in `ESP32-S3-firmware/src/DisplayManager.cpp`.  
These are duplicated in `ESP32-S3-firmware/test/test_time_formatting/` — keep both in sync if you change the format logic.

---

## Font sizes

| Scale | Pixel height | Typical use |
|---|---|---|
| 1 | 8 px | Small labels (`Last: #15`) |
| 2 | 16 px | Shot numbers, totals |
| 3 | 24 px | Large time display |

---

## Display update timing

| Event | Latency | Notes |
|---|---|---|
| `SESSION_STARTED` | Immediate | Clears to waiting layout |
| `SHOT_DETECTED` | < 100 ms | Real-time shot update |
| `SESSION_STOPPED` | Immediate | Shows summary; then reads shot list async |
| Shot list read (SG Timer) | ~500 ms total | Sequential BLE reads of the SHOT_LIST characteristic |

---

## Dirty-flag rendering

`DisplayManager` stores the last rendered state. `update()` (called every main loop tick) compares the new state against the cached state and skips the DMA write if nothing changed. This eliminates flicker and reduces CPU usage.

Callers invoke `showXxx()` methods to mutate internal state and set `displayDirty = true`. They never write to the panel directly.

---

## Text positioning reference

The coordinate origin is top-left (0, 0).

```cpp
// Shot number (top section)
display->setCursor(2, 2);
display->printf("SHOT:%d", shotNumber);

// Shot time (lower section)
display->setCursor(2, 18);
display->printf("02.345");

// Horizontally centred text
int textWidth = strlen(text) * 6;      // ~6 px per character
int centerX   = (128 - textWidth) / 2;
display->setCursor(centerX, y);
```

Do **not** call `u8g2_for_adafruit_gfx.getUTF8Width()` for pixel-width estimates — use the `textPixelWidth` helper already in `DisplayManager`.

---

## Brightness recommendations

| Environment | Brightness (0–255) |
|---|---|
| Dark room | 20–50 |
| Indoor office | 70–100 |
| Bright indoor | 120–150 |
| Outdoor shade | 180–220 |
| Direct sunlight | 240–255 |

Default: `DEFAULT_BRIGHTNESS` = 128 (`common.h`). Changed via `display->setBrightness8(value)`.

---

## Display power draw estimates

| Content | Brightness 90 | Brightness 255 |
|---|---|---|
| All black (idle) | ~400 mA | ~400 mA |
| "00:00" white | ~1.5 A | ~5 A |
| Full shot display | ~2 A | ~6 A |
| All white | ~3 A | ~8 A |

Use a **5 V / 10 A** supply for comfortable headroom.

---

## Testing checklist

- [ ] Power on → startup marquee scrolls
- [ ] BLE scan active → serial shows scan loop
- [ ] Timer connects → serial confirms connection
- [ ] `SESSION_STARTED` → display shows "READY" + "00:00"
- [ ] First shot → display shows `SHOT:1` and time
- [ ] Multiple shots → display updates for each
- [ ] Time > 60 s → format changes to `MM:SS.t`
- [ ] `SESSION_STOPPED` → summary layout appears
- [ ] Shot list read → serial prints full list (SG Timer only)
- [ ] BLE disconnect → board auto-rescans and reconnects
