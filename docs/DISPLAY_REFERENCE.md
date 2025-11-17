# Display Output Quick Reference

## Visual Display States

### 1️⃣ System Ready (Power On)
```
┌────────────────────────────────┐
│  SG TIMER        [Green Text]  │
│  READY           [Green Text]  │
│                                │
│                                │
└────────────────────────────────┘
Duration: 2 seconds, then clears
```

---

### 2️⃣ Waiting for Connection
```
┌────────────────────────────────┐
│         (blank)                │
│                                │
│                                │
│                                │
└────────────────────────────────┘
While scanning for SG Timer...
```

---

### 3️⃣ Session Started (No Shots Yet)
**Triggered by**: SG Timer SESSION_STARTED event

```
┌────────────────────────────────┐
│  READY           [Light Blue]  │
│                                │
│  00:00             [White]     │
│                                │
└────────────────────────────────┘
Waiting for first shot detection
```

**Serial Output**:
```
[TIMER] === SESSION STARTED === (ID: 1698012345, Delay: 3.0s)
[DISPLAY] Waiting for shots...
```

---

### 4️⃣ Active Session (Shots Detected)
**Triggered by**: SG Timer SHOT_DETECTED events

#### Example: Shot #1 at 2.345 seconds
```
┌────────────────────────────────┐
│  SHOT:1          [Yellow]      │
│                                │
│  02.345           [Green]      │
│                                │
└────────────────────────────────┘
```

#### Example: Shot #5 at 12.789 seconds
```
┌────────────────────────────────┐
│  SHOT:5          [Yellow]      │
│                                │
│  12.789           [Green]      │
│                                │
└────────────────────────────────┘
```

#### Example: Shot #12 at 1 minute 23.456 seconds
```
┌────────────────────────────────┐
│  SHOT:12         [Yellow]      │
│                                │
│  01:23.4          [Green]      │
│                                │
└────────────────────────────────┘
Format: MM:SS.T (tenths shown for >60s)
```

**Serial Output**:
```
[TIMER] SHOT: 1 - TIME: 2.345s | SPLIT: 0.000s
[DISPLAY] Shot: 1, Time: 00:02.345
[TIMER] SHOT: 2 - TIME: 4.123s | SPLIT: 1.778s
[DISPLAY] Shot: 2, Time: 00:04.123
```

---

### 5️⃣ Session Ended
**Triggered by**: SG Timer SESSION_STOPPED event

```
┌────────────────────────────────┐
│  SESSION END       [Red]       │
│                                │
│  SHOTS: 15        [Yellow]     │
│  Last: #15         [Green]     │
└────────────────────────────────┘
Shows total shots and last shot number
```

**Serial Output**:
```
[TIMER] === SESSION STOPPED === (ID: 1698012345, Total Shots: 15)
[DISPLAY] Session End - Total Shots: 15
[SHOT_LIST] Reading shots for session ID: 1698012345
[SHOT_LIST] Shot details:
[SHOT_LIST] #  | Time (s)
[SHOT_LIST] ---|----------
[SHOT_LIST]  0 | 2.345
[SHOT_LIST]  1 | 4.123
...
[SHOT_LIST] 14 | 45.678
```

---

## Color Coding Reference

| Color | RGB | Use Case |
|-------|-----|----------|
| 🟢 **Green** | (0, 255, 0) | Shot times, ready status, success |
| 🟡 **Yellow** | (255, 255, 0) | Shot numbers, session info |
| 🔵 **Light Blue** | (100, 100, 255) | Waiting state |
| 🔴 **Red** | (255, 0, 0) | Session end header |
| ⚪ **White** | (255, 255, 255) | Time display when waiting |

---

## Font Sizes

| Size | Height (px) | Use Case |
|------|-------------|----------|
| 1 | 8 | Small text (labels, "Last: #15") |
| 2 | 16 | Medium text (shot numbers, totals) |
| 3 | 24 | Large text (waiting time "00:00") |

---

## Time Display Formats

### Under 60 seconds
```
Format: SS.mmm
Example: 12.345 (12 seconds, 345 milliseconds)
```

### Over 60 seconds (1+ minutes)
```
Format: MM:SS.t
Example: 01:23.4 (1 minute, 23.4 seconds)
Note: Only 1 decimal place shown due to space
```

---

## Display Update Timing

| Event | Response Time | Notes |
|-------|--------------|-------|
| SESSION_STARTED | Immediate | Clears to waiting display |
| SHOT_DETECTED | < 100ms | Real-time shot update |
| SESSION_STOPPED | Immediate | Shows summary, then reads list |
| Shot List Read | ~500ms total | Reads all shots sequentially |

---

## Coordinate System

```
     X-axis: 0 ──────────────────► 127
   Y │
   - │  ┌─────────────────────────────┐
   a │  │                             │
   x │  │     Display Area            │
   i │  │      128 x 32               │
   s │  │                             │
   : │  └─────────────────────────────┘
     ▼
    31

Origin (0,0) = Top-left corner
```

### Text Positioning Examples

```cpp
// Shot number (top section)
display->setCursor(2, 2);      // X=2, Y=2
display->printf("SHOT:%d", num);

// Time (bottom section)
display->setCursor(2, 18);     // X=2, Y=18
display->printf("12.345");

// Centered text (approximate)
int textWidth = strlen(text) * 6;  // ~6px per char
int centerX = (128 - textWidth) / 2;
display->setCursor(centerX, y);
```

---

## State Machine Flow

```
┌─────────────┐
│  Power On   │
│  (2 sec)    │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Scanning   │◄──────┐
│  (Blank)    │       │
└──────┬──────┘       │
       │              │
       ▼              │
┌─────────────┐       │
│  Connected  │       │
│  (Blank)    │       │
└──────┬──────┘       │
       │              │
       │ SESSION_     │
       │ STARTED      │
       ▼              │
┌─────────────┐       │
│  Waiting    │       │
│  "00:00"    │       │ Disconnect
└──────┬──────┘       │
       │              │
       │ SHOT_        │
       │ DETECTED     │
       ▼              │
┌─────────────┐       │
│  Active     │───────┤
│  Shot Data  │       │
└──────┬──────┘       │
       │              │
       │ SESSION_     │
       │ STOPPED      │
       ▼              │
┌─────────────┐       │
│  Summary    │───────┘
│  + Shot List│
└─────────────┘
```

---

## Testing Checklist

- [ ] Power on - See "SG TIMER READY"
- [ ] BLE scan - Monitor shows scanning
- [ ] Connection - Monitor shows "Connected to SG Shot Timer"
- [ ] Session start - Display shows "READY" and "00:00"
- [ ] First shot - Display updates with shot #1 and time
- [ ] Multiple shots - Display updates for each shot
- [ ] Time > 60s - Format changes to MM:SS.t
- [ ] Session end - Display shows summary
- [ ] Shot list - Monitor prints complete shot list
- [ ] Reconnect - Disconnect timer, should auto-reconnect

---

## Common Display Content

### No Shots Detected Yet
```
READY
00:00
```

### First Shot
```
SHOT:1
02.345
```

### Typical Mid-Session
```
SHOT:8
15.234
```

### Long Time (>1 min)
```
SHOT:15
01:34.2
```

### Session Complete
```
SESSION END
SHOTS: 20
Last: #20
```

---

## Brightness Recommendations

| Environment | Brightness (0-255) | Notes |
|-------------|-------------------|-------|
| Dark Room | 20-50 | Easy on eyes |
| Indoor Office | 70-100 | Good visibility |
| Bright Indoor | 120-150 | Overhead lights |
| Outdoor Shade | 180-220 | Partial sun |
| Direct Sunlight | 240-255 | Max brightness needed |

**Current setting**: 90 (good for indoor use)

---

## Panel Power Draw Estimates

| Content | Brightness | Current Draw (Both Panels) |
|---------|-----------|---------------------------|
| All Black | Any | ~400mA (idle) |
| "00:00" White | 90 | ~1.5A |
| Full Shot Display | 90 | ~2.0A |
| All White | 255 | ~8A (maximum) |

**Recommendation**: 5V 10A power supply for safety margin
