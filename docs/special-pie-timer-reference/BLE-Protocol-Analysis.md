# Special Pie M1A2+ BLE Protocol Analysis

## BLE Services

### Primary Timer Service
- **Service UUID:** `FFF0` (`0000FFF0-0000-1000-8000-00805F9B34FB`)
- **Characteristics:**
  - `FFF1` (`0000FFF1-0000-1000-8000-00805F9B34FB`) - Timer events (notify enabled)
  - `FFF2` (`0000FFF2-0000-1000-8000-00805F9B34FB`) - Purpose unknown

### Device Information Service
- **Service UUID:** `0917FE11-5D37-816D-8000-00805F9B34FB`
- **Characteristics:**
  - `09170001-5D37-816D-8000-00805F9B34FB` - Purpose unknown
  - `09170002-5D37-816D-8000-00805F9B34FB` - Device/firmware information (notify enabled)
    - Returns firmware version string (e.g., `app-1.2.1-20240719.0000`)
    - Returns status byte `0x80` after enabling notifications

## Message Structure

All messages follow this pattern:

```
[F8] [F9] [MESSAGE_TYPE] [DATA...] [F9] [F8]
```

## Message Types

### 1. Timer Shot Event

**Message Type:** `0x36` (54 decimal)

**Format:**
```
F8 F9 36 00 [SEC] [MS] [SHOT#] [CHECKSUM?] F9 F8
```

**Byte Positions (0-indexed):**
- `int_values[2]` = `0x36` (54) - Shot event identifier
- `int_values[4]` = Seconds component of time
- `int_values[5]` = Centiseconds (0-99)
- `int_values[6]` = Shot number

**Examples:**

| Raw Data                          | Shot # | Time (seconds) |
|-----------------------------------|--------|----------------|
| `F8 F9 36 00 04 24 02 0C F9 F8`  | 2      | 4.36           |
| `F8 F9 36 00 2A 3B 05 0C F9 F8`  | 5      | 42.59          |
| `F8 F9 36 00 4F 33 06 0C F9 F8`  | 6      | 79.51          |
| `F8 F9 36 00 9D 29 0B 0C F9 F8`  | 11     | 157.41         |
| `F8 F9 36 00 C2 2B 0D 0C F9 F8`  | 13     | 194.43         |

### 2. Session Start Event

**Message Type:** `0x34` (52 decimal)

**Format:**
```
F8 F9 34 0[SESSION_ID] F9 F8
```

**Byte Positions (0-indexed):**
- `int_values[2]` = `0x34` (52) - Session start identifier
- `int_values[3]` = Session ID (sequential counter: 0x03, 0x04, 0x05...)

**Examples:**

| Raw Data              | Session ID | Description      |
|-----------------------|------------|------------------|
| `F8 F9 34 03 F9 F8`   | 3          | Session start    |
| `F8 F9 34 0C F9 F8`   | 12         | Session start    |

### 3. Session Stop Event

**Message Type:** `0x18` (24 decimal)

**Format:**
```
F8 F9 18 0[SESSION_ID] F9 F8
```

**Byte Positions (0-indexed):**
- `int_values[2]` = `0x18` (24) - Session stop identifier
- `int_values[3]` = Session ID (matches corresponding session start)

**Examples:**

| Raw Data              | Session ID | Description      |
|-----------------------|------------|------------------|
| `F8 F9 18 0B F9 F8`   | 11         | Session stop     |
| `F8 F9 18 0C F9 F8`   | 12         | Session stop     |

## Protocol Notes

- **Time Format:** Seconds are stored in byte 4, centiseconds (0-99) in byte 5
- **Shot Numbers:** Sequential and stored in byte 6
- **Session IDs:** Sequential counter that links session start and stop events
- **Split Times:** Calculated by tracking previous time values to determine time between shots
- **Frame Markers:**
  - `F8` = Start of frame marker
  - `F9` = End of frame marker / Start of end sequence

## Connection Sequence

1. Connect to device
2. Discover services (`0917FE11-5D37-816D-8000-00805F9B34FB` and `FFF0`)
3. Enable notifications on `FFF1` (timer events)
4. Optionally enable notifications on `09170002-5D37-816D-8000-00805F9B34FB` (device info)
5. Device sends `0x80` acknowledgment when notifications enabled
6. Timer events flow through `FFF1` characteristic

## Unknown/Unimplemented

- **Characteristic:** `09170001-5D37-816D-8000-00805F9B34FB` - Purpose unknown
- **Characteristic:** `FFF2` - Purpose unknown (possibly for write commands)
