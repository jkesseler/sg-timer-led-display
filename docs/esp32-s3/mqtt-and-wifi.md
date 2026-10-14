# ESP32-S3 Firmware — MQTT and WiFi

## Overview

MQTT and WiFi are **optional** in BLE mode. If no broker IP is configured, the firmware operates in display-only mode. WiFi starts at boot in non-blocking mode, so BLE scanning is never blocked by network delays.

---

## WiFi configuration portal

On first boot (or after NVS is erased), if no WiFi credentials are stored, the firmware opens a WiFi access point:

```
SSID:    J.K. PewPew Timer Bridge  (defined by BLE_DEVICE_NAME in common.h)
Portal:  http://192.168.4.1
```

Connect from a phone or laptop, open `http://192.168.4.1`, and fill in:

| Field | Description | Default |
|---|---|---|
| WiFi SSID / password | Network credentials | *(empty)* |
| MQTT server | Broker IP or hostname | *(empty — MQTT disabled if blank)* |
| MQTT port | Broker port | 1883 |
| MQTT username | Optional auth | *(empty)* |
| MQTT password | Optional auth | *(empty)* |
| Timer type | `1` = BLE: connect to the timer and publish its events. `2` = MQTT: show another bridge's timer (see [MQTT timer mode](#mqtt-timer-mode)) | `1` |
| Follow timer ID | MQTT mode only: the device ID of the bridge to show. `+` follows any bridge | `+` |
| Startup text | Marquee text shown on boot | `J.K. PewPew Timer Bridge` |

All settings are persisted to NVS (ESP32 flash) using the `Preferences` API under namespace `wifi-config`. They survive power cycles and firmware reflashes that do not erase NVS.

### Portal on the network address

When WiFi is connected to a network, the firmware closes the access point and serves the same portal pages on the IP address of the board. The startup screen shows this address (see `display.md`). Use it to change the settings without a reset to access-point mode. If you select **Exit** in the portal menu, the firmware starts the portal again at the next connection check.

> **Note:** The portal has no password. Any device on the same network can change the WiFi, MQTT and startup-text settings.

Implementation: `ESP32-S3-firmware/src/WiFiConfig.cpp`

---

## Initialisation order

```
TimerApplication::initialize()
  1. WiFiConfig::initialize()   — loads NVS settings, starts non-blocking WiFiManager (autoConnect)
  2. DisplayManager::init()
  3. MqttManager::initialize()  — only if MQTT server is non-empty; connects once WiFi is up
  4. BLE client init            — BLE mode only
```

BLE scanning works immediately, even in environments with no WiFi.

---

## MQTT topic structure

All topics are prefixed with `timer/<deviceId>/`, where `deviceId` is the unique 6-character flash-backed ID from `DeviceId`.

| Topic | Retained | Payload | Trigger |
|---|---|---|---|
| `timer/<id>/presence` | ✅ | `"online"` / `"offline"` | Connect / LWT disconnect |
| `timer/<id>/connection/state` | ✅ | `"connected"` / `"disconnected"` | BLE state change |
| `timer/<id>/device/info` | ✅ | JSON: model, firmware, deviceId | On BLE connect |
| `timer/<id>/session/started` | ❌ | JSON: sessionId, startDelay | `SESSION_STARTED` |
| `timer/<id>/session/stopped` | ❌ | JSON: sessionId, totalShots | `SESSION_STOPPED` |
| `timer/<id>/session/suspended` | ❌ | JSON: sessionId | `SESSION_SUSPENDED` |
| `timer/<id>/session/resumed` | ❌ | JSON: sessionId | `SESSION_RESUMED` |
| `timer/<id>/shot/<n>` | ❌ | JSON: shot number, absoluteTimeMs, splitTimeMs | `SHOT_DETECTED` |
| `timer/<id>/countdown/complete` | ❌ | JSON: sessionId | `COUNTDOWN_COMPLETE` |

### Subscribed topics

| Topic | Published by | Payload | Effect |
|---|---|---|---|
| `timer/<id>/session/up-next` | score-keeping-app timekeeper, not retained | JSON: `sessionId`, `next`, optional `onDeck` | The session-end screen alternates with a NEXT / ON DECK screen every 10 s |

The firmware ignores an up-next message when its `sessionId` is not the session that just ended. The timekeeper publishes it only after a timed turn stops and a next shooter exists.

### Example payloads

**Shot detected:**
```json
{
  "sessionId": 1698012345,
  "shotNumber": 3,
  "absoluteTimeMs": 4820,
  "splitTimeMs": 1234,
  "isFirstShot": false,
  "deviceModel": "SGTimer Sport"
}
```

**Session started:**
```json
{
  "sessionId": 1698012345,
  "startDelaySeconds": 3.0
}
```

---

## MQTT timer mode

With timer type `2`, the board does not use BLE. It shows the events of another bridge, such as `ble-nodejs-bridge` on the score-keeping laptop, so the board and the laptop can use one timer together. Restart the board after you change the timer type.

- The board subscribes to `timer/<follow timer ID>/#` and shows `session/*`, `shot/detected`, `countdown/complete`, `connection/state` and `session/up-next`, the same as from its own timer.
- It shows one bridge at a time: the first whose `presence` is `online`. While that presence is `offline`, the board shows the timer as disconnected and ignores the bridge's retained `connection/state`, which a crashed bridge leaves at `CONNECTED`. When the bridge goes offline, the next bridge to come online is shown.
- The board publishes nothing. It sets no last will and clears its own retained `presence`, `connection/state` and `device/info`, so the score-keeping app and `/display` never take the board for a timer.
- With `+`, the board follows whichever bridge it sees online first. Set the bridge's device ID if more than one bridge is online.
- MQTT is required in this mode. Until the bridge reports a connected timer, the board shows the timer as disconnected.

To test without a timer, start `mqtt-simulator` against the same broker.

---

## Last Will Testament (LWT)

`MqttManager` registers an LWT at connect time:

- Topic: `timer/<deviceId>/presence`
- Payload: `"offline"`
- Retain: `true`
- QoS: 1

This ensures downstream subscribers see `"offline"` automatically if the device loses power or network connectivity.

---

## Shot event queueing

Shot events are detected on the BLE stack thread. Publishing MQTT directly from that thread is not safe. Instead:

```
BLE notification callback
  → onShotDetected()
  → xQueueSend(shotEventQueue)        (non-blocking, runs on BLE thread)

Main loop (every 10 ms):
  → publishQueuedEvents()
  → drains up to 8 shots per iteration via MqttManager::publishShot()
```

The queue holds up to 32 `NormalizedShotData` entries. `maxQueueDepth` and `publishFailures` are logged by `performHealthCheck()` every 30 s.

---

## MQTT reconnect behaviour

`MqttManager::update()` is called every main loop tick. If the broker connection drops:

1. `MqttManager` attempts reconnect on the next tick.
2. On success, it re-publishes retained topics (`presence`, `connection/state`, `device/info`).
3. On failure, it backs off and tries again next tick (no fixed interval — relies on the 10 ms main loop delay as a natural rate-limit).

Shots queued during a connection outage remain in the FreeRTOS queue and are published once the connection is restored, unless the queue fills (oldest events are dropped).

---

## Disabling MQTT

Leave the MQTT server field blank in the WiFi portal. `MqttManager` is not initialised, and the firmware runs in display-only mode with no network activity. All BLE and display functionality is unaffected.
