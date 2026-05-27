# ESP32-S3 Firmware — MQTT and WiFi

## Overview

MQTT and WiFi are **optional** in this firmware. If no broker IP is configured, the firmware operates in display-only mode. WiFi initialises lazily after the first BLE connection so BLE scanning is never blocked by network delays.

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
| Startup text | Marquee text shown on boot | `J.K. PewPew Timer Bridge` |

All settings are persisted to NVS (ESP32 flash) using the `Preferences` API under namespace `wifi-config`. They survive power cycles and firmware reflashes that do not erase NVS.

Implementation: `ESP32-S3-firmware/src/WiFiConfig.cpp`

---

## Initialisation order

```
TimerApplication::initialize()
  1. WiFiConfig::initialize()   — loads NVS settings, starts non-blocking WiFiManager
  2. DisplayManager::init()
  3. BLE client init
  4. (first BLE connection established)
     → WiFiConfig: WiFi stack starts, connects to saved SSID
     → MqttManager::initialize()  — only if MQTT server is non-empty
```

This ensures BLE scanning works immediately, even in environments with no WiFi.

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
