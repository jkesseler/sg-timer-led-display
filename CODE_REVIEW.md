# Code Review — ESP32-S3-firmware

Date: 2026-06-18
Scope: `ESP32-S3-firmware/` (firmware sources, headers, and native tests)
Branch: `chore/refactor`

This review covers correctness, memory-safety, concurrency, and minor cleanups.
Fixes below were applied to the working tree. The native test suite was **not**
run (build/test was explicitly skipped for this pass); see *Verification* at the end.

---

## Summary

| # | Severity | Area | Status |
|---|----------|------|--------|
| 1 | **High** | Use-after-free on BLE disconnect | ✅ Fixed |
| 2 | Medium | Dangling `deviceName` pointer in `DisplayManager` | ✅ Fixed |
| 3 | Medium | Logger macro misuse in `BaseTimerDevice` | ✅ Fixed |
| 4 | Low | Unused member `lastMqttWarningTime` | ✅ Fixed |
| 5 | Low | Magic number `5000` for scan throttle | ✅ Fixed |
| 6 | Low | `strncpy` null-terminator ordering in `SGTimer` | ✅ Fixed |
| 7 | Low | Dead null-check on array member in `MqttManager` | ✅ Fixed |
| 8 | **High** | MQTT published from BLE-stack callback (data race) | ⚠️ Documented, not changed |
| 9 | Low | Shot dropped on transient MQTT publish failure | ℹ️ Noted (by design) |
| 10 | Low | Floating-point + logging inside BLE callback | ℹ️ Noted |

---

## Fixes applied

### 1. (High) Use-after-free when a timer device disconnects

**Files:** `src/TimerApplication.cpp`, `include/TimerApplication.h`

`BaseTimerDevice::update()` detects a dropped link and calls
`handleConnectionLost()`, which calls `setConnectionState(DISCONNECTED)`. That
fires `connectionStateCallback`, i.e. `TimerApplication::onConnectionStateChanged()`,
which previously did:

```cpp
timerDevice.reset();   // deletes the device object…
```

…**while that very object's `handleConnectionLost()`/`update()` was still on the
call stack.** After `setConnectionState()` returned, `handleConnectionLost()`
continued executing and wrote to a freed member:

```cpp
setConnectionState(DeviceConnectionState::DISCONNECTED);   // -> deletes 'this'
currentSession = {};                                       // <- use-after-free
```

A second UAF followed: `onConnectionStateChanged()` passed `deviceName`
(a pointer into the just-freed device) to `displayManager->showConnectionState()`.

This is undefined behaviour and an intermittent crash/heap-corruption risk on
hardware (it depends on whether the freed block is reused before the writes).

**Fix:** the disconnect path now sets a `deviceResetPending` flag instead of
deleting in place. The actual `timerDevice.reset()` runs in `run()` (the main
loop), right after `timerDevice->update()` returns — i.e. once the device's
methods are off the stack. All connection-state transitions originate from the
main-loop task, so the flag needs no cross-thread synchronization.

### 2. (Medium) `DisplayManager` retained a dangling device-name pointer

**Files:** `src/DisplayManager.cpp`, `include/DisplayManager.h`

`DisplayManager::deviceName` stored the raw `const char*` handed in by the caller,
which points into the timer device's `deviceName[64]` buffer. That device is
destroyed on disconnect, leaving `DisplayManager` holding a dangling pointer.
Today it is only dereferenced in the `CONNECTED` state, so it was *latent*, but
it became actively dangerous in combination with finding #1.

**Fix:** `DisplayManager` now owns a `char deviceNameStorage[64]` and copies the
name into it (`strncpy` + explicit termination). `deviceName` points at the owned
copy, or is `nullptr` when no name is supplied — preserving existing
`if (deviceName && …)` logic.

### 3. (Medium) Logger macro misused in `BaseTimerDevice`

**File:** `include/BaseTimerDevice.h`

The logging macros are `LOG_INFO(component, format, …)`. Two call sites passed
the format string as the *component* and the device model as the *format*:

```cpp
LOG_INFO("Initializing %s device interface", deviceModel);
```

Effect: the `%s` was never expanded, the "component" column printed the literal
format string, and — more importantly — `deviceModel` was used as a `printf`
format string. That is a latent format-string bug: any `%` in a device/model
name would read undefined varargs.

**Fix:** supply a real component tag:

```cpp
LOG_INFO("DEVICE", "Initializing %s device interface", deviceModel);
```

### 4. (Low) Removed unused `lastMqttWarningTime`

**Files:** `include/TimerApplication.h`, `src/TimerApplication.cpp`

The member was declared and constructor-initialized but never read or written.
Removed the field and its initializer.

### 5. (Low) Replaced magic `5000` with the existing named constant

**File:** `src/TimerApplication.cpp`

`scanForDevices()` throttled with a hardcoded `5000`. `common.h` already defines
`BLE_SCAN_RETRY_INTERVAL_MS` (= 5000) for exactly this purpose; the code now uses it.

### 6. (Low) Fixed `strncpy` terminator ordering in `SGTimer`

**File:** `src/SGTimer.cpp`

One branch wrote the terminator *before* the `strncpy` (works by accident because
`strncpy(dst, src, sizeof-1)` never touches the last byte). Reordered to the
conventional copy-then-terminate, matching every other call site in the codebase.

### 7. (Low) Removed dead null-check on a fixed-size array member

**File:** `src/MqttManager.cpp`

`publishShotDetected()` had `shotData.deviceModel ? shotData.deviceModel : "unknown"`.
`deviceModel` is a `char[32]` struct member — it can never be null, so the
fallback was dead code. Simplified and commented.

---

## Findings documented but **not** changed

### 8. (High) MQTT is published directly from the BLE-stack callback for session events

`processTimerData()` runs in the BLE host task. For **session** events
(`SESSION_STARTED/STOPPED/SUSPENDED/RESUMED`, countdown), the registered
`TimerApplication` handlers call `mqttManager->publish…()` **synchronously from
that callback**. Meanwhile the main loop calls `publishShotDetected()` and
`mqttClient.loop()`. `PubSubClient` and the shared `jsonBuffer` are not
thread-safe, so two tasks can touch them concurrently → buffer/protocol
corruption.

Note this is exactly what the shot path already avoids: shots are pushed onto a
FreeRTOS queue and published from the main loop. `CLAUDE.md` also states *"do not
call display or MQTT code from within a callback."* Session events violate that
rule.

**Why not fixed here:** the correct fix is to route session events through the
queue (or a deferred-publish flag set + drained in `run()`), which is a non-trivial
change carrying several payloads (sessionId, totalShots, delay, event kind) and
should be validated on hardware. It was left for a dedicated change rather than an
untested refactor. **Recommended next step.**

### 9. (Low) A shot can be dropped on a transient publish failure

In `publishQueuedEvents()`, the shot is removed from the queue with
`xQueueReceive()` *before* publishing; on failure it `break`s and the dequeued
shot is lost. In practice a failure flips `mqttConnected` to false and the next
cycle discards the whole queue anyway (the firmware intentionally does **not**
buffer while offline), so this is consistent with the existing "display-only when
MQTT is down" design. Flagged for awareness; no behavioural change made.

### 10. (Low) Floating-point math and logging in the BLE callback path

`logShotData()` runs in the BLE callback (via `onShotDetected`) and does two
floating-point divisions plus a `vsnprintf` per shot at `INFO` level. `CLAUDE.md`
advises avoiding FP in BLE callbacks. It is functionally fine and only a minor
performance note; left as-is.

---

## Verification

- All changes are localized and compile-compatible (no signature/API changes
  beyond the removed unused member).
- **Native tests were not executed for this pass** (the host `g++` toolchain used
  by the `native-tests` PlatformIO environment was not available/was skipped).
  The existing suites — `test_protocol_parsing`, `test_ring_buffer`,
  `test_time_formatting` — are unaffected by these edits (no protocol-parsing or
  time-formatting logic changed). To validate locally:

  ```bash
  pio test -e native-tests
  ```

- The use-after-free fix (#1) and the dangling-pointer fix (#2) are best confirmed
  on hardware by forcing a BLE disconnect during an active session and verifying a
  clean rescan with no crash/reset.

## Suggested follow-ups (in priority order)

1. Move session-event MQTT publishing off the BLE callback (finding #8).
2. Consider having `DisplayManager` ignore a stale `deviceName` more defensively,
   or clear it on `DISCONNECTED` (now low-risk after fix #2).
3. Add a native test around `TimerApplication` connection-state transitions if the
   class can be decoupled from its hardware dependencies.
