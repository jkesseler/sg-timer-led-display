# BLE-LoRa Bridge — Architecture

## Overview

The bridge firmware is built around a **dual-role factory pattern**: the same binary runs on every board. At startup, `BridgeApplication` reads the configured role from NVS and instantiates a different component graph for each role.

```
NVS role = TRANSMITTER               NVS role = RECEIVER
────────────────────────             ────────────────────────────────────────
BLE device drivers                   LoRaReceiver
  └─► BridgeApplication              └─► BridgeApplication
        └─► LoRaTransmitter                 ├─► MqttManager  (MQTT mode)
                                            └─► SpecialPieBleServer (BLE mode)
```

Both roles share `BridgeOledDisplay` and `BridgeWiFiConfig`.

---

## Data flow

### Transmitter path (BLE → LoRa)

```
BLE Timer Device
  ↓  BLE notification callback
ITimerDevice subclass  (SGTimer, SpecialPieM1A2F, etc.)
  ↓  normalised NormalizedShotData / SessionData
BridgeApplication  (onShotDetected, onSessionStarted, …)
  ↓
LoRaTransmitter::sendXxx()
  ↓  LoRaProtocol::serializeXxx()
SX1276 radio  — transmit over 868 MHz
```

The BLE notification callbacks run on the BLE stack thread and must return quickly. `BridgeApplication` callbacks are invoked synchronously within `ITimerDevice::processTimerData()`, which is itself called from the notification callback. Keep them non-blocking.

### Receiver path (LoRa → MQTT or LoRa → BLE)

```
SX1276 radio  — incoming 868 MHz packet
  ↓  LoRaReceiver::update()  (polled every main-loop iteration)
  ↓  LoRaProtocol::deserialize()  — CRC-16 validation
BridgeApplication  (onLoRaShotDetected, onLoRaSessionStarted, …)
  ↓
Either:
  a) MqttManager::publishXxx()  →  MQTT broker
  b) SpecialPieBleServer::sendXxx()  →  BLE GATT notifications
```

---

## Main loop (`BridgeApplication::run`)

Runs every 10 ms (`MAIN_LOOP_DELAY`).

1. `wifiConfig.update()` — non-blocking WiFi portal management
2. Role-specific update:
   - Transmitter: `runTransmitter()` — BLE scan / connect / device update
   - Receiver: `runReceiver()` — poll `loraReceiver.update()`
3. `loraTx.update()` *(Transmitter only)* — send heartbeat if 30 s elapsed
4. `mqttManager->update()` *(Receiver / MQTT mode)* — MQTT keep-alive
5. `oledDisplay.update(bridgeStatus)` — redraw OLED if state changed
6. `vTaskDelay(MAIN_LOOP_DELAY)` — yield to FreeRTOS

---

## Classes

| Class | Header | Responsibility |
|---|---|---|
| `BridgeApplication` | `BridgeApplication.h` | Top-level coordinator; role-aware component factory; callback wiring |
| `LoRaTransmitter` | `LoRaTransmitter.h` | Serialises BLE events and transmits via SX1276; sends heartbeat |
| `LoRaReceiver` | `LoRaReceiver.h` | Polls SX1276, validates CRC, dispatches type-specific callbacks |
| `SpecialPieBleServer` | `SpecialPieBleServer.h` | GATT server emulating Special Pie M1A2+ BLE peripheral |
| `BridgeOledDisplay` | `BridgeOledDisplay.h` | Dirty-flag SSD1306 renderer; role-aware status views |
| `BridgeWiFiConfig` | `BridgeWiFiConfig.h` | Non-blocking WiFi portal; NVS read/write for role and MQTT settings |

### `BridgeApplication`

Key methods:
- `initialize()` — sets up serial, `Logger`, `DeviceId`, OLED, WiFi, and role-specific components
- `run()` — main loop (see above)
- `runTransmitter()` — BLE scan state machine; calls `timerDevice->update()`
- `runReceiver()` — calls `loraReceiver.update()`
- `setupBleCallbacks()` — registers `onShotDetected`, `onSessionStarted`, `onSessionStopped`, `onCountdownComplete`, `onSessionSuspended`, `onSessionResumed`, `onConnectionStateChanged` on the active `ITimerDevice`
- `setupLoRaCallbacks()` — registers the symmetric set of LoRa packet handlers on `LoRaReceiver`

### `LoRaTransmitter`

Owns the SX1276 radio in TX direction. Parameters set at `initialize()`: SF7, BW 500 kHz, 868 MHz, 14 dBm, sync word 0x77, preamble 8.

Key methods: `sendShotDetected(NormalizedShotData)`, `sendSessionStarted/Stopped/Suspended/Resumed()`, `sendCountdownComplete()`, `sendHeartbeat()`, `update()` (heartbeat timer).

### `LoRaReceiver`

Polls `LoRa.parsePacket()` on every call to `update()`. Validates the application-layer CRC-16, then fires the registered `std::function` callback for the matched type. Exposes metrics: `getLastRssi()`, `getPacketsReceived()`, `getCrcErrors()`.

### `SpecialPieBleServer`

Advertises as `Special Pie M1A2+` with service UUID `0000FFF0-0000-1000-8000-00805F9B34FB`. On shot events it converts milliseconds to seconds + centiseconds and frames an `F8 F9` notification (see [lora-protocol.md](lora-protocol.md)). Restarts advertising after client disconnect.

### `BridgeOledDisplay`

Uses the dirty-flag pattern: `update(BridgeStatus)` compares the new status struct against `lastStatus` and skips the redraw if nothing changed. The 128 × 64 display shows role-specific layouts:

- **Transmitter view**: role label | BLE connection status | shots-transmitted count
- **Receiver view**: role + output mode | last shot time | RX count + RSSI | MQTT / BLE client status
- **Footer** (both): WiFi SSID + uptime + CRC error count

### `BridgeWiFiConfig`

Wraps `WiFiManager`. On first boot (or after NVS clear), it opens an AP named `J.K. PewPew Long Range Bridge AP [deviceId]` at `192.168.4.1` and presents a configuration portal. Settings are stored under the NVS namespace `bridge-cfg`. Implements a `WiFiConfig` shim class so shared `ESP32-S3-firmware` code that calls `WiFiConfig::getMqttXxx()` continues to work without changes.

---

## Shared code from `ESP32-S3-firmware`

The bridge reuses several components from the main firmware's source tree. They are pulled in via the `lora-bridge-shared-sources` build filter in `platformio.ini`:

| Component | Source file | Used by |
|---|---|---|
| `Logger` | `ESP32-S3-firmware/src/Logger.cpp` | All components |
| `DeviceId` | `ESP32-S3-firmware/src/DeviceId.cpp` | `BridgeApplication`, `LoRaTransmitter` |
| `MqttManager` | `ESP32-S3-firmware/src/MqttManager.cpp` | Receiver / MQTT mode |
| `SGTimer` | `ESP32-S3-firmware/src/SGTimer.cpp` | Transmitter BLE device discovery |
| `SpecialPieM1A2F` | `ESP32-S3-firmware/src/SpecialPieM1A2F.cpp` | Transmitter BLE device discovery |
| `SpecialPieM1A2Plus` | `ESP32-S3-firmware/src/SpecialPieM1A2Plus.cpp` | Transmitter BLE device discovery |

`NormalizedShotData` and `SessionData` structs (defined in `ITimerDevice.h`) are the lingua franca between BLE device drivers and `BridgeApplication`.

---

## Key design patterns

### Dual-role factory

`BridgeApplication::initialize()` branches on `BridgeRole`: it instantiates either (`ITimerDevice` subclass + `LoRaTransmitter`) or (`LoRaReceiver` + `MqttManager` / `SpecialPieBleServer`). The main loop then calls `runTransmitter()` or `runReceiver()` accordingly.

### Dirty-flag OLED rendering

`BridgeOledDisplay` stores the previous `BridgeStatus` value. `update()` exits early if no field changed, eliminating unnecessary I²C traffic and display flicker.

### Non-blocking main loop

Every subsystem call inside `run()` must return within the 10 ms budget. WiFi portal, BLE scan, and MQTT reconnect are all asynchronous. Rate-limiting guards (`BLE_RECONNECT_INTERVAL`, `BLE_SCAN_RETRY_INTERVAL_MS`) prevent busy-spin reconnection.

### Callback-driven event dispatch

All cross-component communication uses `std::function` callbacks registered at startup. `BridgeApplication` never calls into BLE or LoRa internals directly; it only receives normalised events. This decouples the transport layer from the application logic.

### NVS-backed configuration

Role and MQTT settings survive power cycles. A web portal allows reconfiguration without a firmware rebuild.
