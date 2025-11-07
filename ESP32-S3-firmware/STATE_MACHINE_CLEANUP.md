# State Machine Cleanup - Aligned with BLE API

## Summary
Cleaned up the SystemStateMachine to align with the actual BLE API events documented in `sg_timer_public_bt_api_32.md`.

## Changes Made

### Removed Non-Existent States
The following states were removed as they don't correspond to actual BLE API events:
- ❌ `SESSION_STARTING` - Not in BLE API
- ❌ `SESSION_ENDING` - Not in BLE API
- ❌ `SESSION_ENDED` - Not in BLE API

### Actual BLE API Events (from documentation)
The SG Timer sends these events via the EVENT characteristic:

| Event ID | Event Name | Description |
|----------|------------|-------------|
| 0x00 | SESSION_STARTED | Session has been started |
| 0x01 | SESSION_SUSPENDED | Session has been suspended |
| 0x02 | SESSION_RESUMED | Session has been resumed |
| 0x03 | SESSION_STOPPED | Session has been stopped |
| 0x04 | SHOT_DETECTED | Shot detected during session |
| 0x05 | SESSION_SET_BEGIN | Delay time ended, session set starts |

### Updated State Machine States

#### System States (Unchanged)
- `STARTUP` - Initial startup
- `MANUAL_RESET` - Manual reset triggered

#### Connection States (Unchanged)
- `SEARCHING_FOR_DEVICES` - Scanning for BLE devices
- `CONNECTING` - Connecting to device
- `CONNECTED` - Connected to device
- `CONNECTION_ERROR` - Connection failed
- `RECONNECTING` - Attempting reconnection
- `COMMUNICATION_ERROR` - Communication lost

#### Session States (Cleaned Up)
- `IDLE` - Connected but no active session
- `SESSION_ACTIVE` - Session is active (maps to SESSION_STARTED event)
- `SHOT_DETECTED` - Shot detected (maps to SHOT_DETECTED event)
- `SESSION_SUSPENDED` - Session paused (maps to SESSION_SUSPENDED event)

#### Error States (Unchanged)
- `DEVICE_ERROR` - Device error
- `SYSTEM_ERROR` - System error
- `RECOVERY` - Recovery mode

### Event Mapping

**BLE Event → State Transition:**
- `SESSION_STARTED` (0x00) → `SESSION_ACTIVE`
- `SHOT_DETECTED` (0x04) → `SHOT_DETECTED` (brief display) → `SESSION_ACTIVE`
- `SESSION_SUSPENDED` (0x01) → `SESSION_SUSPENDED`
- `SESSION_RESUMED` (0x02) → `SESSION_ACTIVE`
- `SESSION_STOPPED` (0x03) → `IDLE`

### State Transition Flow

#### Normal Session Flow
```
IDLE → SESSION_ACTIVE → SHOT_DETECTED → SESSION_ACTIVE → ... → IDLE
                                ↓
                         (after timeout)
                                ↓
                         SESSION_ACTIVE
```

#### Suspended Session Flow
```
SESSION_ACTIVE → SESSION_SUSPENDED → SESSION_ACTIVE (resumed)
                        ↓
                      IDLE (stopped)
```

### Code Changes Summary

#### SystemStateMachine.h
- Removed `SESSION_STARTING`, `SESSION_ENDING`, `SESSION_ENDED` from enum
- Removed `SESSION_ENDED_TIMEOUT` constant

#### SystemStateMachine.cpp
- Removed handlers: `handleSessionStarting()`, `handleSessionEnding()`, `handleSessionEnded()`
- Updated `onSessionEvent()` to map directly to BLE events:
  - "started" → `SESSION_ACTIVE` (not SESSION_STARTING)
  - "ended" → `IDLE` (not SESSION_ENDING or SESSION_ENDED)
- Updated state transition logic to remove non-existent state transitions
- Updated `stateToString()` to remove deleted states
- Updated `isInSessionState()` to only include real session states

### Benefits

1. **Accuracy**: State machine now matches actual BLE API
2. **Simplicity**: Fewer unnecessary intermediate states
3. **Clarity**: Direct mapping between BLE events and states
4. **Maintainability**: Easier to understand and debug

### State Timeouts

| State | Timeout | Purpose |
|-------|---------|---------|
| SHOT_DETECTED | 3s | Display shot briefly then return to SESSION_ACTIVE |
| All Others | Various | Connection/error recovery timeouts |

### Testing Notes

After this cleanup, verify:
1. ✅ Session starts transition to SESSION_ACTIVE immediately
2. ✅ Shots display briefly (3s) then return to SESSION_ACTIVE
3. ✅ Session end transitions directly to IDLE
4. ✅ No invalid state transitions occur
5. ✅ Display updates correctly for each state

## Build Status
✅ Compiles successfully
✅ RAM: 13.8% (45,256 bytes)
✅ Flash: 28.9% (965,365 bytes)
