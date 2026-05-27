# Special Pie M1A2+ BLE Protocol

**Status**: ✅ Validated with working test implementation (`ESP32-S3-firmware/test/test-special-pie-timer.cpp`)

This protocol is used by two device classes:
- `SpecialPieM1A2F` (name-pattern discovery: `SP M1A2 Timer XXXX`)
- `SpecialPieM1A2Plus` (UUID-based discovery)

See [../esp32-s3/timer-devices.md](../esp32-s3/timer-devices.md) for how these classes differ at the firmware level.

---

## BLE services

### Primary timer service

- **Service UUID:** `FFF0` — `0000FFF0-0000-1000-8000-00805F9B34FB`

| Characteristic | UUID | Properties | Purpose |
|---|---|---|---|
| `FFF1` | `0000FFF1-0000-1000-8000-00805F9B34FB` | Notify | Timer events ✅ |
| `FFF2` | `0000FFF2-0000-1000-8000-00805F9B34FB` | Unknown | Purpose unknown |

### Device information service

- **Service UUID:** `0917FE11-5D37-816D-8000-00805F9B34FB`

| Characteristic | UUID | Properties | Purpose |
|---|---|---|---|
| `09170001` | `09170001-5D37-816D-8000-00805F9B34FB` | Unknown | Purpose unknown |
| `09170002` | `09170002-5D37-816D-8000-00805F9B34FB` | Read | Firmware version string ✅ |

Firmware version example: `app-1.2.1-20240719.0000`

---

## Message framing

All messages follow this structure:

```
[F8] [F9] [MESSAGE_TYPE] [DATA...] [F9] [F8]
```

| Marker | Position | Value |
|---|---|---|
| Start | bytes 0–1 | `0xF8 0xF9` |
| End | last 2 bytes | `0xF9 0xF8` |
| Minimum frame length | — | 6 bytes |

Validation:

```cpp
if (length < 6) return;
if (pData[0] != 0xF8 || pData[1] != 0xF9) return;
if (pData[length-2] != 0xF9 || pData[length-1] != 0xF8) return;
uint8_t messageType = pData[2];
```

---

## Message types

### Shot detected  (`0x36`)  ✅ Validated

```
F8 F9 36 00 [SEC] [CS] [SHOT#] [CHK?] F9 F8
```

| Byte | Value | Description |
|---|---|---|
| 0 | `0xF8` | Frame start |
| 1 | `0xF9` | Frame start |
| 2 | `0x36` | Shot event identifier |
| 3 | `0x00` | Reserved / unknown |
| 4 | seconds | Time seconds component (0–255) |
| 5 | centiseconds | Time centiseconds component (0–99); 1 unit = 10 ms |
| 6 | shot index | 0-based shot index |
| 7 | unknown | Possibly checksum; not validated |
| 8 | `0xF9` | Frame end |
| 9 | `0xF8` | Frame end |

**Total length: 10 bytes.**

Time in milliseconds: `(seconds × 1000) + (centiseconds × 10)`

Examples:

| Raw data | Shot # | Seconds | CS | Time |
|---|---|---|---|---|
| `F8 F9 36 00 04 24 02 0C F9 F8` | 2 | 4 | 36 | 4.36 s |
| `F8 F9 36 00 2A 3B 05 0C F9 F8` | 5 | 42 | 59 | 42.59 s |
| `F8 F9 36 00 9D 29 0B 0C F9 F8` | 11 | 157 | 41 | 157.41 s |

---

### Session start  (`0x34`)  ✅ Validated

```
F8 F9 34 [SESSION_ID] F9 F8
```

| Byte | Value |
|---|---|
| 0–1 | `0xF8 0xF9` |
| 2 | `0x34` |
| 3 | Session ID (sequential counter: 0x03, 0x04, …) |
| 4–5 | `0xF9 0xF8` |

**Total length: 6 bytes.**

---

### Session stop  (`0x18`)  ✅ Validated

```
F8 F9 18 [SESSION_ID] F9 F8
```

Same structure as session start. Session ID matches the corresponding start event.

**Total length: 6 bytes.**

---

## Split time calculation

Split times must be computed client-side by tracking the previous shot's time:

```cpp
uint32_t prevSec = 0, prevCs = 0;
bool hasPrev = false;

// On each shot received:
uint32_t sec = pData[4];
uint32_t cs  = pData[5];

if (hasPrev) {
    int32_t dSec = sec - prevSec;
    int32_t dCs  = cs  - prevCs;

    if (dCs < 0) {          // borrow from seconds
        dSec -= 1;
        dCs  += 100;
    }

    uint32_t splitMs = (dSec * 1000) + (dCs * 10);
}

prevSec = sec;
prevCs  = cs;
hasPrev = true;
```

---

## Connection sequence

1. Scan for devices advertising service `0000FFF0-0000-1000-8000-00805F9B34FB`
2. Connect to device
3. Discover Timer Service (`FFF0`)
4. Get characteristic `FFF1`
5. Verify `canNotify()`
6. Register notification callback
7. *(Optional)* Read firmware version from `09170002`
8. Receive timer events

---

## Time format limits

| Property | Value |
|---|---|
| Resolution | 10 ms (1 centisecond) |
| Maximum time | 255 seconds (~4 min 15 s) |
| Conversion | `(sec × 1000) + (cs × 10)` ms |

---

## Limitations

| Feature | Status |
|---|---|
| Remote session start/stop | ❌ Not supported |
| Shot list retrieval | ❌ Not available |
| Session suspend/resume | ❌ Not observed in protocol |
| Start delay reporting | ❌ Always 0 |
| Maximum session duration | ❌ Capped at 255 s |

---

## Unknowns

- `pData[3]` in shot events — always `0x00`; purpose unknown
- `pData[7]` in shot events — possibly a checksum; not validated in firmware
- Characteristic `09170001` — purpose unknown
- Characteristic `FFF2` — purpose unknown (possibly write commands)
