# ESP32-S3 Firmware — Architecture

## Data flow

```
BLE Timer Device  (SGTimer / SpecialPieM1A2F / SpecialPieM1A2Plus / ASNTracker)
  ↓  BLE notification callback
ITimerDevice subclass  — parses raw bytes, converts to NormalizedShotData
  ↓  std::function callbacks
TimerApplication  — coordinates all subsystems
  ├─► FreeRTOS queue  ──►  MqttManager  ──►  MQTT broker
  └─► DisplayManager  ──►  HUB75 128×32 panel
```

**Critical constraint:** BLE notification callbacks run on the BLE stack thread. They must execute quickly and never block. `onShotDetected()` enqueues shot data into a FreeRTOS queue via `xQueueSend()`; the main loop drains it batch-wise in `publishQueuedEvents()`.

---

## Main loop (`TimerApplication::run`)

Runs every 10 ms (`MAIN_LOOP_DELAY`).

1. `wifiConfig.update()` — non-blocking WiFi portal background management
2. BLE device management — scan / process scan results / `timerDevice->update()`
3. `publishQueuedEvents()` — drain the FreeRTOS shot queue into MQTT (up to 8 shots per cycle)
4. `mqttManager->update()` — MQTT keep-alive and reconnect
5. `displayManager->update()` — dirty-flag-driven display render
6. `performHealthCheck()` — periodic uptime and health logging
7. `vTaskDelay(MAIN_LOOP_DELAY)` — yield to FreeRTOS

---

## Classes

| Class | Header | Responsibility |
|---|---|---|
| `TimerApplication` | `TimerApplication.h` | Top-level coordinator; owns all other components; main loop |
| `DisplayManager` | `DisplayManager.h` | HUB75 panel rendering; state machine; dirty-flag redraws |
| `MqttManager` | `MqttManager.h` | MQTT connection, reconnect, publish; last will testament |
| `WiFiConfig` | `WiFiConfig.h` | Non-blocking WiFi portal; NVS persistence of MQTT settings |
| `ITimerDevice` | `ITimerDevice.h` | Abstract interface for all timer device drivers |
| `BaseTimerDevice` | `BaseTimerDevice.h` | Shared BLE lifecycle, callback storage, heartbeat |
| `SGTimer` | `SGTimer.h` | SG Timer Sport / GO BLE driver |
| `SpecialPieM1A2F` | `SpecialPieM1A2F.h` | Special Pie M1A2 (name-pattern discovery) BLE driver |
| `SpecialPieM1A2Plus` | `SpecialPieM1A2Plus.h` | Special Pie M1A2+ (UUID discovery) BLE driver |
| `ASNTracker` | `ASNTracker.h` | ASN Tracker BLE driver |
| `DeviceId` | `DeviceId.h` | Flash-backed unique device identifier |
| `Logger` | `Logger.h` | Tagged log macros with level filtering |

### `TimerApplication`

Key methods:
- `initialize()` — sets up display, MQTT, WiFi, BLE in correct dependency order
- `run()` — main loop (see above)
- `processScanResults()` — evaluates each scanned BLE device against all four matchers in priority order
- `setupCallbacks()` — registers `onShotDetected`, `onSessionStarted`, `onCountdownComplete`, `onSessionStopped`, `onSessionSuspended`, `onSessionResumed`, `onConnectionStateChanged` on the active device
- `publishQueuedEvents()` — drains the FreeRTOS queue, publishes to MQTT
- `performHealthCheck()` — logs uptime and queue depth every 30 s

Key state:
- `shotEventQueue` — FreeRTOS queue (32 × `NormalizedShotData`), lock-free ring buffer for BLE→MQTT
- `sessionActive`, `lastShotNumber`, `lastShotTimeMs`

### `DisplayManager`

Uses the dirty-flag pattern: callers invoke `showXxx()` methods that update internal state and set `displayDirty = true`. `update()` redraws only when `displayDirty` is set. The display renders at 128×32 in RGB565.

`formatTime()` and `formatSplitTime()` are replicated in `test/test_time_formatting/` — keep them in sync if you change the production implementations.

### `ITimerDevice` / `BaseTimerDevice`

`ITimerDevice` defines the interface all device drivers implement:

```cpp
virtual bool matchesDevice(BLEAdvertisedDevice*) = 0;
virtual bool attemptConnection(BLEAdvertisedDevice*) = 0;
virtual void processTimerData(uint8_t* data, size_t len) = 0;
virtual void update() = 0;
virtual void disconnect() = 0;

// Callback registration
void onShotDetected(std::function<void(const NormalizedShotData&)>);
void onSessionStarted(std::function<void(const SessionData&)>);
void onCountdownComplete(std::function<void(const SessionData&)>);
void onSessionStopped(std::function<void(const SessionData&)>);
void onSessionSuspended(std::function<void(const SessionData&)>);
void onSessionResumed(std::function<void(const SessionData&)>);
void onConnectionStateChanged(std::function<void(DeviceConnectionState)>);
```

`BaseTimerDevice` provides shared BLE connection management (connect, disconnect, reconnect delay, `DeviceConnectionState` state machine, heartbeat logging). Concrete classes only need to implement `matchesDevice()`, `attemptConnection()`, and `processTimerData()`.

---

## Key data structures

### `NormalizedShotData`

All time values are in **milliseconds**. Devices reporting centiseconds (Special Pie, ASN) multiply by 10 inside their driver before setting these fields.

```cpp
struct NormalizedShotData {
  uint32_t sessionId;
  uint16_t shotNumber;       // 1-based
  uint32_t absoluteTimeMs;   // ms since session start
  uint32_t splitTimeMs;      // ms since previous shot (0 for first)
  uint64_t timestampMs;      // system clock when shot was detected
  char     deviceModel[32];  // null-terminated, e.g. "SGTimer", "SP M1A2 Timer"
  bool     isFirstShot;
};
```

### `SessionData`

```cpp
struct SessionData {
  uint32_t sessionId;
  bool     isActive;
  uint16_t totalShots;
  uint32_t startTimestamp;
  float    startDelaySeconds;  // 0.0 for Special Pie / ASN
};
```

### `DeviceConnectionState`

```
DISCONNECTED → SCANNING → CONNECTING → CONNECTED → (disconnect) → DISCONNECTED
                                                 → ERROR → DISCONNECTED
```

---

## Key design patterns

### Callback-driven event dispatch

All cross-component communication uses `std::function` callbacks registered in `TimerApplication::setupCallbacks()`. The device driver fires normalised events; `TimerApplication` never calls BLE internals directly.

**Static instance for C-style BLE notification callbacks:**

```cpp
class MyDevice : public BaseTimerDevice {
  static MyDevice* instance;

  static void notifyCallback(BLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    if (instance) instance->processTimerData(data, len);
  }
};
```

### Dirty-flag display rendering

`DisplayManager` stores the last rendered state. `update()` skips the redraw if nothing changed, avoiding unnecessary DMA writes and flicker.

### Non-blocking main loop

Every call in `run()` must return within the 10 ms budget. Long-running operations (WiFi portal, BLE scan, MQTT reconnect, shot-list read) are always asynchronous. Rate-limiting guards (`BLE_RECONNECT_INTERVAL`, 5 s) prevent busy-loop reconnection.

### FreeRTOS queue for BLE → MQTT handoff

Shot events are detected on the BLE stack thread. Queuing via `xQueueSend` is the only way to safely hand them to the main loop without allocating on the stack or holding the BLE thread.

### Smart pointer component ownership

```cpp
std::unique_ptr<DisplayManager>  displayManager;
std::unique_ptr<MqttManager>     mqttManager;
std::unique_ptr<ITimerDevice>    timerDevice;
```

`TimerApplication` is the sole owner; components are destroyed in reverse-construction order when `TimerApplication` goes out of scope.

---

## Compile-time configuration (`common.h`)

| Macro | Value | Effect |
|---|---|---|
| `TIMER_TYPE` | `TIMER_TYPE_BLE` (1) | Activates BLE input path |
| `PANEL_WIDTH / HEIGHT / CHAIN` | 64 / 32 / 2 | 128×32 total display |
| `DEFAULT_BRIGHTNESS` | 128 | Initial panel brightness (0–255) |
| `MAIN_LOOP_DELAY` | 10 ms | FreeRTOS yield interval |
| `BLE_SCAN_DURATION` | 10 s | Seconds per scan window |
| `BLE_RECONNECT_INTERVAL` | 5 000 ms | Minimum delay between reconnect attempts |
| `STARTUP_MESSAGE_DELAY` | 5 000 ms (1 000 ms in `DEBUG_BUILD`) | Marquee duration at startup |
| `MAX_SHOTS_PER_SESSION` | 100 | Shot-list read ceiling |

All runtime-overridable values (MQTT server, port, credentials) are stored in NVS and managed by `WiFiConfig`. See [mqtt-and-wifi.md](mqtt-and-wifi.md).
