# ESP32-S3 Firmware — Timer Devices

This document covers the supported BLE timer devices, the `ITimerDevice` abstraction layer, and how to add a new device.

---

## Supported devices

| Device class | Discovery method | Service UUID | Event characteristic | Protocol | Time unit |
|---|---|---|---|---|---|
| `SGTimer` | Advertised service UUID | `7520FFFF-14D2-4CDA-8B6B-697C554C9311` | `75200001-14D2-4CDA-8B6B-697C554C9311` | Length-prefixed event packet | Native milliseconds |
| `SpecialPieM1A2F` | Name prefix `SP M1A2 Timer ` | `0000FFF0-0000-1000-8000-00805F9B34FB` | `0000FFF1-0000-1000-8000-00805F9B34FB` | Framed `F8 F9 … F9 F8` | Seconds + centiseconds |
| `SpecialPieM1A2Plus` | Advertised service UUID | `0000FFF0-0000-1000-8000-00805F9B34FB` | `0000FFF1-0000-1000-8000-00805F9B34FB` | Framed `F8 F9 … F9 F8` | Seconds + centiseconds |
| `ASNTracker` | Advertised service UUID | `E5A10001-F1A2-4B63-9F8C-D7B781E35E2A` | `E5A10002-F1A2-4B63-9F8C-D7B781E35E2A` | Framed `F8 F9 … F9 F8` | Seconds + centiseconds |

---

## Scan priority

`TimerApplication::processScanResults()` evaluates each discovered BLE device in this fixed order. The first match connects immediately; only one device is active at a time.

1. `SpecialPieM1A2F::matchesDevice()` — name-pattern check
2. `SGTimer::matchesDevice()` — UUID check
3. `SpecialPieM1A2Plus::matchesDevice()` — UUID check
4. `ASNTracker::matchesDevice()` — UUID check

---

## Event support matrix

| Event | SG Timer | SpecialPieM1A2F | SpecialPieM1A2Plus | ASNTracker |
|---|:---:|:---:|:---:|:---:|
| `onSessionStarted` | ✅ | ✅ | ✅ | ✅ |
| `onCountdownComplete` | ✅ (when delay > 0) | ❌ | ✅ (fired immediately after start) | ✅ (fired immediately after start) |
| `onShotDetected` | ✅ | ✅ | ✅ | ✅ |
| `onSessionStopped` | ✅ | ✅ | ✅ | ✅ |
| `onSessionSuspended` | ✅ | ❌ | ❌ | ❌ |
| `onSessionResumed` | ✅ | ❌ | ❌ | ❌ |
| Start delay (`startDelaySeconds`) | ✅ reported | 0.0 | 0.0 | 0.0 |
| Remote start/stop | Interface exists; all `false` by default | — | — | — |
| Shot list read | `75200004` characteristic | ❌ | ❌ | ❌ |

---

## Time normalisation

All devices output `NormalizedShotData` with times in **milliseconds**.

| Device | Native format | Normalisation in driver |
|---|---|---|
| SG Timer | Milliseconds | Pass through |
| Special Pie (both variants) | Seconds + centiseconds | `cs × 10` → ms; split calculated client-side |
| ASN Tracker | Seconds + centiseconds | Same as Special Pie |

Split time for Special Pie / ASN Tracker is calculated by tracking the previous shot's seconds and centiseconds and computing the delta — with centisecond borrowing when `deltaCentiseconds < 0`.

---

## Device-specific notes

### `SGTimer`

- Parses SG BLE event IDs `0x00`–`0x05` from the EVENT characteristic.
- Start delay (`startDelaySeconds`) is included in the `SESSION_STARTED` payload.
- Reads the `SHOT_LIST` characteristic (`75200004`) after `SESSION_STOPPED` to reconstruct the full shot log; results are printed to serial.
- Device model string is refined from the advertised name: `SG-SST4A…` → `SGTimer Sport`, `SG-SST4B…` → `SGTimer GO`.

### `SpecialPieM1A2F`

- Prioritised first in the scan order because name-pattern matching is more specific than shared UUID matching.
- Device name must match regex `^SP M1A2 Timer [A-Za-z0-9]{4}$`.
- Optionally reads firmware version from characteristic `09170002` of the device-info service.
- Uses MAC-address connection (stores the address from the scan result).

### `SpecialPieM1A2Plus`

- UUID-based fallback for Special Pie devices that do not match the name pattern.
- Shares service and characteristic UUIDs with `SpecialPieM1A2F`; identical wire protocol.
- `onCountdownComplete` is fired immediately upon `SESSION_STARTED` (no real countdown detection).

### `ASNTracker`

- Uses its own unique UUID pair; otherwise implements the same framed protocol as Special Pie.
- Shot numbers from the device are zero-indexed; the driver converts to 1-based before storing in `NormalizedShotData.shotNumber` (`totalShots = shotNumber + 1`).

---

## The `ITimerDevice` interface

All device classes implement `ITimerDevice`:

```cpp
class ITimerDevice {
public:
  virtual bool matchesDevice(BLEAdvertisedDevice* device) = 0;
  virtual bool attemptConnection(BLEAdvertisedDevice* device) = 0;
  virtual void processTimerData(uint8_t* data, size_t len) = 0;
  virtual void update() = 0;          // called every main loop tick
  virtual void disconnect() = 0;

  // Callback registration
  void onShotDetected(std::function<void(const NormalizedShotData&)> cb);
  void onSessionStarted(std::function<void(const SessionData&)> cb);
  void onCountdownComplete(std::function<void(const SessionData&)> cb);
  void onSessionStopped(std::function<void(const SessionData&)> cb);
  void onSessionSuspended(std::function<void(const SessionData&)> cb);
  void onSessionResumed(std::function<void(const SessionData&)> cb);
  void onConnectionStateChanged(std::function<void(DeviceConnectionState)> cb);
};
```

`BaseTimerDevice` provides default implementations for connection lifecycle, callback storage, heartbeat logging, and `DeviceConnectionState` management. Concrete classes only implement `matchesDevice()`, `attemptConnection()`, and `processTimerData()`.

---

## How to add a new timer device

1. **Create header** `ESP32-S3-firmware/include/YourDevice.h`
   - Extend `BaseTimerDevice`
   - Declare `static const char* SERVICE_UUID` and `CHARACTERISTIC_UUID`
   - Declare `static YourDevice* instance` (needed for C-style BLE callback)
   - Declare `static void notifyCallback(BLERemoteCharacteristic*, uint8_t*, size_t, bool)`

2. **Create implementation** `ESP32-S3-firmware/src/YourDevice.cpp`
   - `matchesDevice()` — return `true` if the advertised device matches (UUID or name)
   - `attemptConnection()` — connect, get service, get characteristic, register `notifyCallback`
   - `processTimerData()` — parse raw bytes; convert all times to **milliseconds**; fire callbacks
   - Null-check every BLE object (`pClient`, `pService`, `pChar`) before use
   - No heap allocation, no `delay()`, no display or MQTT calls inside the callback

3. **Register in scan priority** — add to `TimerApplication::processScanResults()`:
   ```cpp
   if (YourDevice::matchesDevice(&advertisedDevice)) {
     timerDevice = std::make_unique<YourDevice>();
     timerDevice->attemptConnection(&advertisedDevice);
     return;
   }
   ```

4. **Add protocol tests** in `ESP32-S3-firmware/test/test_protocol_parsing/`, following the `ProtocolTestBase` pattern used by the existing four device tests. Use `#define private public` to access `processTimerData()`.

5. **Run native tests**: `pio test -e native-tests`

Use `SGTimer` as the reference for UUID-based devices; `SpecialPieM1A2F` for name-pattern devices.

---

## MQTT republish interaction

When a broker is configured, shot events from any device go through the same path:

```
onShotDetected() callback
  → xQueueSend(shotEventQueue)       (non-blocking; runs on BLE stack thread)
  → publishQueuedEvents()            (called in main loop; up to 8 shots per cycle)
  → MqttManager::publishShot()
```

See [mqtt-and-wifi.md](mqtt-and-wifi.md) for MQTT topic structure.

---

## Relevant source files

| File | Contains |
|---|---|
| `ESP32-S3-firmware/include/ITimerDevice.h` | `ITimerDevice`, `NormalizedShotData`, `SessionData`, `DeviceConnectionState` |
| `ESP32-S3-firmware/include/BaseTimerDevice.h` | `BaseTimerDevice` |
| `ESP32-S3-firmware/src/SGTimer.cpp` | SG Timer driver |
| `ESP32-S3-firmware/src/SpecialPieM1A2F.cpp` | Special Pie name-variant driver |
| `ESP32-S3-firmware/src/SpecialPieM1A2Plus.cpp` | Special Pie UUID-variant driver |
| `ESP32-S3-firmware/src/ASNTracker.cpp` | ASN Tracker driver |
| `ESP32-S3-firmware/src/TimerApplication.cpp` | `processScanResults()`, callback wiring |

---

## Wire protocol references

Full byte-level protocol documentation for each device type:

- [../timer-protocols/special-pie-ble.md](../timer-protocols/special-pie-ble.md) — Special Pie M1A2+ BLE protocol
- [../timer-protocols/sg-timer-ble-api.md](../timer-protocols/sg-timer-ble-api.md) — SG Timer BLE API v3.2 specification
