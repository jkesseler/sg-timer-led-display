# SG Timer — BLE API v3.2

**Source:** Official SG Timer BLE API specification, version 3.2.

The device advertises the 128-bit UUID of its main service: `7520FFFF-14D2-4CDA-8B6B-697C554C9311` and an advertising name of the form `SG-SST4XYYYYY`:

| Part | Meaning |
|---|---|
| `X = 'A'` | SG Timer Sport |
| `X = 'B'` | SG Timer GO |
| `YYYYY` | Device serial number |

All multi-byte values in any characteristic are **Big Endian**.

See [../esp32-s3/timer-devices.md](../esp32-s3/timer-devices.md) for how the `SGTimer` driver implements this API.

---

## Attribute table

| Name | Type | UUID | Properties |
|---|---|---|---|
| MAIN | Service | `7520FFFF-14D2-4CDA-8B6B-697C554C9311` | — |
| COMMAND | Characteristic | `75200000-14D2-4CDA-8B6B-697C554C9311` | W, N |
| EVENT | Characteristic | `75200001-14D2-4CDA-8B6B-697C554C9311` | N |
| SAVED_SESSION_ID_LIST | Characteristic | `75200002-14D2-4CDA-8B6B-697C554C9311` | R, W |
| RESERVED | Characteristic | `75200003-14D2-4CDA-8B6B-697C554C9311` | R |
| SHOT_LIST | Characteristic | `75200004-14D2-4CDA-8B6B-697C554C9311` | R, W |
| PAR_SETUP | Characteristic | `75200005-14D2-4CDA-8B6B-697C554C9311` | R, W |
| UNIX_TIME | Characteristic | `75200006-14D2-4CDA-8B6B-697C554C9311` | R, W |
| API_VERSION | Characteristic | `7520FFFE-14D2-4CDA-8B6B-697C554C9311` | R |

Properties: R = Read, W = Write, N = Notify, I = Indicate.

---

## 1. COMMAND characteristic (`75200000`)

Used to send commands to the timer. All commands share the same general frame:

```
[len][cmd_id][cmd_data…]
```

| Field | Size | Description |
|---|---|---|
| `len` | 1 B | Number of bytes following this byte |
| `cmd_id` | 1 B | Command identifier (see below) |
| `cmd_data` | n B | Command-specific payload |

After a command is received the timer sends a notification response:

```
[len][cmd_id][resp_code]
```

Response codes: `0x00` = Success, `0x01` = Error.

Commands may be sent consecutively without waiting for each response; use `len` to parse the stream.

### Command table

| Command | ID |
|---|---|
| `SESSION_START` | `0x00` |
| `SESSION_SUSPEND` | `0x01` |
| `SESSION_RESUME` | `0x02` |
| `SESSION_STOP` | `0x03` |

#### SESSION_START (0x00)

Starts a new RO (Range Officer) session.

```
[01][00]
```

#### SESSION_SUSPEND (0x01)

Suspends the current session.

```
[01][01]
```

#### SESSION_RESUME (0x02)

Resumes a suspended session.

```
[01][02]
```

#### SESSION_STOP (0x03)

Stops the current session.

```
[01][03]
```

---

## 2. EVENT characteristic (`75200001`)

Notifies all events that occur on the device.

General event frame:

```
[len][event_id][event_data…]
```

### Event table

| Event | ID |
|---|---|
| `SESSION_STARTED` | `0x00` |
| `SESSION_SUSPENDED` | `0x01` |
| `SESSION_RESUMED` | `0x02` |
| `SESSION_STOPPED` | `0x03` |
| `SHOT_DETECTED` | `0x04` |
| `SESSION_SET_BEGIN` | `0x05` |

### 2.1 SESSION_STARTED (0x00)

Sent when a session has started (after the start delay if configured).

```
[len][0x00][sess_id 4B][start_delay 2B]
```

| Field | Size | Description |
|---|---|---|
| `sess_id` | 4 B | Session ID (Unix timestamp) |
| `start_delay` | 2 B | Start delay in units of 0.1 s |

### 2.2 SESSION_SUSPENDED (0x01)

Sent when the session is suspended.

```
[len][0x01][sess_id 4B][total_shots 2B]
```

### 2.3 SESSION_RESUMED (0x02)

Sent when a suspended session resumes.

```
[len][0x02][sess_id 4B][total_shots 2B]
```

### 2.4 SESSION_STOPPED (0x03)

Sent when the session ends.

```
[len][0x03][sess_id 4B][total_shots 2B]
```

### 2.5 SHOT_DETECTED (0x04)

Sent for every detected shot during an active session.

```
[len][0x04][sess_id 4B][shot_num 2B][shot_time 4B]
```

| Field | Size | Description |
|---|---|---|
| `sess_id` | 4 B | Session ID (Unix timestamp) |
| `shot_num` | 2 B | Shot number (1-based) |
| `shot_time` | 4 B | Shot time in **milliseconds** |

### 2.6 SESSION_SET_BEGIN (0x05)

Sent when the start delay ends and the shooting window opens (the "beep" moment).

```
[len][0x05][sess_id 4B]
```

Maps to `onCountdownComplete` in the firmware.

---

## 3. SAVED_SESSION_ID_LIST characteristic (`75200002`)

Used to enumerate saved session IDs stored on the device.

**Write** (4 B): `sess_id` — starting session ID; write `0xFFFFFFFF` to start from the newest.

**Read** (4 B): `sess_id` — returns the next session ID in the list. After the last (oldest) entry, returns `0xFFFFFFFF`. Wraps around on the next read.

Session IDs are returned in reverse order (newest → oldest).

---

## 4. SHOT_LIST characteristic (`75200004`)

Used to read the complete shot log for a particular session.

**Write** (4 B): `sess_id` — selects the session; resets the read pointer to shot 0.

**Read** (6 B per shot):

| Field | Size | Description |
|---|---|---|
| `shot_number` | 2 B | Shot number (0-based in API; firmware normalises to 1-based) |
| `shot_time` | 4 B | Shot time in milliseconds |

Repeat reads to get all shots. After the last shot, `shot_time` returns `0xFFFFFFFF`. The next read wraps back to shot 0.

---

## 5. PAR_SETUP characteristic (`75200005`)

Configures the session parameters.

**Read/Write** (6 B):

| Field | Size | Description |
|---|---|---|
| `start_delay` | 2 B | Start delay in units of 0.1 s; `0xFFFF` = random 1.0–4.0 s |
| `time_limit` | 2 B | Time limit in units of 0.1 s; `0` = unlimited |
| `shot_limit` | 2 B | Shot count limit; `0` = unlimited |

---

## 6. UNIX_TIME characteristic (`75200006`)

**Read/Write** (4 B): Unix time in seconds. Allows the host to synchronise the device clock.

---

## 7. API_VERSION characteristic (`7520FFFE`)

**Read**: Non-null-terminated ASCII string. Example: `"3.2"` → bytes `0x33 0x2E 0x32`.

---

## Firmware implementation notes

- The `SGTimer` driver subscribes only to the `EVENT` characteristic (notifications).
- `SHOT_LIST` is read after `SESSION_STOPPED` to reconstruct the full shot log for serial output.
- Shot times from the API are already in milliseconds — no unit conversion needed.
- `SESSION_SET_BEGIN` maps to `onCountdownComplete` in `ITimerDevice`.
- `SESSION_SUSPENDED` and `SESSION_RESUMED` are the only events not supported by Special Pie / ASN devices.
