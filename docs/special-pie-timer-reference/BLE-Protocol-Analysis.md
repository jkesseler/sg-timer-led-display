# Special Pie M1A2+ BLE Protocol Analysis

## BLE Service Information
- **Characteristic UUID:** `FFF1` (`0000fff1-0000-1000-8000-00805f9b34fb`)

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

**Examples:**

| Raw Data              | Description                      |
|-----------------------|----------------------------------|
| `F8 F9 34 0C F9 F8`   | Session start (session ID = 0x0C)|

### 3. Session Stop Event

**Message Type:** `0x18` (24 decimal)

**Format:**
```
F8 F9 18 0[SESSION_ID] F9 F8
```

**Examples:**

| Raw Data              | Description                      |
|-----------------------|----------------------------------|
| `F8 F9 18 0B F9 F8`   | Session stop (session ID = 0x0B) |
| `F8 F9 18 0C F9 F8`   | Session stop (session ID = 0x0C) |

## Protocol Notes

- **Time Format:** Seconds are stored in byte 4, centiseconds (0-99) in byte 5
- **Shot Numbers:** Sequential and stored in byte 6
- **Split Times:** Calculated by tracking previous time values to determine time between shots
- **Frame Markers:**
  - `F8` = Start of frame marker
  - `F9` = End of frame marker / Start of end sequence
