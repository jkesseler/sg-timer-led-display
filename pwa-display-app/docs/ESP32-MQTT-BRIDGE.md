# ESP32 MQTT Bridge Implementation Guide

This document describes how to modify the ESP32 firmware to publish shot timer data to MQTT.

## Overview

The ESP32 will act as a bridge between BLE shot timers and MQTT clients:

```
Shot Timer (BLE) → ESP32 Bridge → MQTT Broker → PWA Display
                    (WiFi)         (WebSocket)
```

## Required Changes

### 1. Add WiFi and MQTT Libraries

**platformio.ini:**
```ini
[lib_deps_main]
adafruit/Adafruit GFX Library@^1.12.3
mrfaptastic/ESP32 HUB75 LED MATRIX PANEL DMA Display@^3.0.14
knolleary/PubSubClient@^2.8.0  ; MQTT client library
```

### 2. WiFi Configuration

**include/common.h:**
```cpp
// WiFi Configuration
#define WIFI_SSID "YourSSID"
#define WIFI_PASSWORD "YourPassword"

// MQTT Configuration
#define MQTT_BROKER "192.168.1.100"  // Your MQTT broker IP
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "esp32-timer-bridge"

// MQTT Topics
#define MQTT_TOPIC_CONNECTION "timer/connection/state"
#define MQTT_TOPIC_SESSION_START "timer/session/started"
#define MQTT_TOPIC_SESSION_STOP "timer/session/stopped"
#define MQTT_TOPIC_SHOT "timer/shot/detected"
#define MQTT_TOPIC_DEVICE_INFO "timer/device/info"
#define MQTT_TOPIC_COUNTDOWN "timer/countdown/complete"
```

### 3. Create MqttPublisher Class

**include/MqttPublisher.h:**
```cpp
#pragma once

#include <WiFi.h>
#include <PubSubClient.h>
#include "ITimerDevice.h"
#include "common.h"

class MqttPublisher {
private:
  WiFiClient wifiClient;
  PubSubClient mqttClient;
  bool wifiConnected;
  bool mqttConnected;
  unsigned long lastReconnectAttempt;

  void reconnectWiFi();
  void reconnectMQTT();
  String createJsonMessage(const char* payload);

public:
  MqttPublisher();

  bool initialize();
  void update();
  bool isConnected();

  // Publishing methods
  void publishConnectionState(DeviceConnectionState state, const char* deviceName = nullptr);
  void publishSessionStarted(const SessionData& sessionData);
  void publishSessionStopped(const SessionData& sessionData);
  void publishShotDetected(const NormalizedShotData& shotData);
  void publishCountdownComplete(const SessionData& sessionData);
  void publishDeviceInfo(const char* deviceModel, const char* deviceName);
};
```

**src/MqttPublisher.cpp:**
```cpp
#include "MqttPublisher.h"
#include "Logger.h"
#include <Arduino_JSON.h>

MqttPublisher::MqttPublisher()
  : mqttClient(wifiClient),
    wifiConnected(false),
    mqttConnected(false),
    lastReconnectAttempt(0) {
}

bool MqttPublisher::initialize() {
  LOG_SYSTEM("Initializing WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    LOG_SYSTEM("WiFi connected: %s", WiFi.localIP().toString().c_str());

    // Configure MQTT
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setBufferSize(512); // Increase buffer for JSON messages

    // Connect to MQTT
    reconnectMQTT();

    return mqttConnected;
  } else {
    LOG_ERROR("MQTT", "WiFi connection failed");
    return false;
  }
}

void MqttPublisher::reconnectMQTT() {
  if (mqttClient.connect(MQTT_CLIENT_ID)) {
    mqttConnected = true;
    LOG_SYSTEM("MQTT connected to %s:%d", MQTT_BROKER, MQTT_PORT);
  } else {
    mqttConnected = false;
    LOG_ERROR("MQTT", "Connection failed, rc=%d", mqttClient.state());
  }
}

void MqttPublisher::update() {
  // Maintain WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    if (millis() - lastReconnectAttempt > 5000) {
      reconnectWiFi();
      lastReconnectAttempt = millis();
    }
    return;
  } else {
    wifiConnected = true;
  }

  // Maintain MQTT connection
  if (!mqttClient.connected()) {
    mqttConnected = false;
    if (millis() - lastReconnectAttempt > 5000) {
      reconnectMQTT();
      lastReconnectAttempt = millis();
    }
  } else {
    mqttConnected = true;
    mqttClient.loop();
  }
}

bool MqttPublisher::isConnected() {
  return wifiConnected && mqttConnected;
}

void MqttPublisher::publishConnectionState(DeviceConnectionState state, const char* deviceName) {
  if (!mqttConnected) return;

  JSONVar json;
  json["state"] = [state]() -> const char* {
    switch (state) {
      case DeviceConnectionState::CONNECTED: return "CONNECTED";
      case DeviceConnectionState::SCANNING: return "SCANNING";
      case DeviceConnectionState::CONNECTING: return "CONNECTING";
      case DeviceConnectionState::DISCONNECTED: return "DISCONNECTED";
      case DeviceConnectionState::ERROR: return "ERROR";
      default: return "UNKNOWN";
    }
  }();

  if (deviceName) {
    json["deviceName"] = deviceName;
  }
  json["timestamp"] = (long long)millis();

  String payload = JSON.stringify(json);
  mqttClient.publish(MQTT_TOPIC_CONNECTION, payload.c_str());
  LOG_SYSTEM("Published connection state: %s", payload.c_str());
}

void MqttPublisher::publishSessionStarted(const SessionData& sessionData) {
  if (!mqttConnected) return;

  JSONVar json;
  json["sessionId"] = (int)sessionData.sessionId;
  json["startDelaySeconds"] = sessionData.startDelaySeconds;
  json["timestamp"] = (long long)millis();

  String payload = JSON.stringify(json);
  mqttClient.publish(MQTT_TOPIC_SESSION_START, payload.c_str());
  LOG_SYSTEM("Published session started: %s", payload.c_str());
}

void MqttPublisher::publishSessionStopped(const SessionData& sessionData) {
  if (!mqttConnected) return;

  JSONVar json;
  json["sessionId"] = (int)sessionData.sessionId;
  json["totalShots"] = sessionData.totalShots;
  json["timestamp"] = (long long)millis();

  String payload = JSON.stringify(json);
  mqttClient.publish(MQTT_TOPIC_SESSION_STOP, payload.c_str());
  LOG_SYSTEM("Published session stopped: %s", payload.c_str());
}

void MqttPublisher::publishShotDetected(const NormalizedShotData& shotData) {
  if (!mqttConnected) return;

  JSONVar json;
  json["sessionId"] = (int)shotData.sessionId;
  json["shotNumber"] = shotData.shotNumber;
  json["absoluteTimeMs"] = (int)shotData.absoluteTimeMs;
  json["splitTimeMs"] = (int)shotData.splitTimeMs;
  json["deviceModel"] = shotData.deviceModel;
  json["isFirstShot"] = shotData.isFirstShot;
  json["timestamp"] = (long long)millis();

  String payload = JSON.stringify(json);
  mqttClient.publish(MQTT_TOPIC_SHOT, payload.c_str());
  LOG_TIMER("Published shot: %s", payload.c_str());
}

void MqttPublisher::publishCountdownComplete(const SessionData& sessionData) {
  if (!mqttConnected) return;

  JSONVar json;
  json["sessionId"] = (int)sessionData.sessionId;
  json["timestamp"] = (long long)millis();

  String payload = JSON.stringify(json);
  mqttClient.publish(MQTT_TOPIC_COUNTDOWN, payload.c_str());
  LOG_SYSTEM("Published countdown complete: %s", payload.c_str());
}

void MqttPublisher::publishDeviceInfo(const char* deviceModel, const char* deviceName) {
  if (!mqttConnected) return;

  JSONVar json;
  json["deviceModel"] = deviceModel;
  json["deviceName"] = deviceName;
  json["timestamp"] = (long long)millis();

  String payload = JSON.stringify(json);
  mqttClient.publish(MQTT_TOPIC_DEVICE_INFO, payload.c_str());
  LOG_SYSTEM("Published device info: %s", payload.c_str());
}

void MqttPublisher::reconnectWiFi() {
  LOG_SYSTEM("Reconnecting WiFi...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}
```

### 4. Integrate into TimerApplication

**include/TimerApplication.h:**
```cpp
#include "MqttPublisher.h"

class TimerApplication {
private:
  std::unique_ptr<MqttPublisher> mqttPublisher;
  // ... existing members

public:
  // ... existing methods
};
```

**src/TimerApplication.cpp:**
```cpp
TimerApplication::TimerApplication() {
  // ... existing initialization
  mqttPublisher = std::make_unique<MqttPublisher>();
}

bool TimerApplication::initialize() {
  // ... existing initialization

  // Initialize MQTT publisher
  if (!mqttPublisher->initialize()) {
    LOG_ERROR("SYSTEM", "MQTT initialization failed - continuing without MQTT");
    // Don't fail completely, just log and continue
  }

  // ... rest of initialization
}

void TimerApplication::run() {
  // Update MQTT connection
  if (mqttPublisher) {
    mqttPublisher->update();
  }

  // ... existing loop code
}

void TimerApplication::onConnectionStateChanged(DeviceConnectionState state) {
  // ... existing code

  // Publish to MQTT
  if (mqttPublisher && mqttPublisher->isConnected()) {
    const char* deviceName = timerDevice ? timerDevice->getDeviceName() : nullptr;
    mqttPublisher->publishConnectionState(state, deviceName);
  }
}

void TimerApplication::onSessionStarted(const SessionData& sessionData) {
  // ... existing code

  // Publish to MQTT
  if (mqttPublisher && mqttPublisher->isConnected()) {
    mqttPublisher->publishSessionStarted(sessionData);
  }
}

void TimerApplication::onShotDetected(const NormalizedShotData& shotData) {
  // ... existing code

  // Publish to MQTT
  if (mqttPublisher && mqttPublisher->isConnected()) {
    mqttPublisher->publishShotDetected(shotData);
  }
}

void TimerApplication::onSessionStopped(const SessionData& sessionData) {
  // ... existing code

  // Publish to MQTT
  if (mqttPublisher && mqttPublisher->isConnected()) {
    mqttPublisher->publishSessionStopped(sessionData);
  }
}

void TimerApplication::onCountdownComplete(const SessionData& sessionData) {
  // ... existing code

  // Publish to MQTT
  if (mqttPublisher && mqttPublisher->isConnected()) {
    mqttPublisher->publishCountdownComplete(sessionData);
  }
}
```

### 5. Add Arduino_JSON Library

**platformio.ini:**
```ini
[lib_deps_main]
adafruit/Adafruit GFX Library@^1.12.3
mrfaptastic/ESP32 HUB75 LED MATRIX PANEL DMA Display@^3.0.14
knolleary/PubSubClient@^2.8.0
arduino-libraries/Arduino_JSON@^0.2.0
```

## Testing

1. **Set up MQTT broker** (Mosquitto recommended)
2. **Configure WiFi credentials** in `common.h`
3. **Set MQTT broker IP** in `common.h`
4. **Build and upload** firmware
5. **Monitor serial output** for connection status
6. **Subscribe to topics** using MQTT Explorer or mosquitto_sub:
   ```bash
   mosquitto_sub -h localhost -t 'timer/#' -v
   ```
7. **Trigger shots** on timer device
8. **Verify messages** appear in MQTT client

## Network Architecture

```
┌─────────────┐         ┌──────────────┐         ┌──────────────┐
│ Shot Timer  │   BLE   │   ESP32-S3   │  WiFi   │ MQTT Broker  │
│  (SG/Pie)   ├────────>│    Bridge    ├────────>│ (Mosquitto)  │
└─────────────┘         └──────┬───────┘         └──────┬───────┘
                               │                         │
                               │ (optional)              │ WebSocket
                               v                         v
                        ┌──────────────┐         ┌──────────────┐
                        │  HUB75 LED   │         │  PWA Display │
                        │   Display    │         │ (Browser)    │
                        └──────────────┘         └──────────────┘
```

## Security Considerations

1. **WiFi Credentials:** Store in separate config file (not in git)
2. **MQTT Authentication:** Add username/password to MQTT broker
3. **TLS/SSL:** Use `WiFiClientSecure` for encrypted connection
4. **Network Isolation:** Consider separate VLAN for IoT devices

## Performance Notes

- MQTT publishing is non-blocking
- WiFi/MQTT reconnection every 5 seconds if disconnected
- JSON buffer size: 512 bytes (configurable)
- No impact on BLE connection or display rendering
- Dual-core: WiFi on one core, BLE/Display on another

## Troubleshooting

**WiFi won't connect:**
- Verify SSID and password
- Check 2.4GHz vs 5GHz (ESP32 only supports 2.4GHz)
- Monitor serial output for error messages

**MQTT connection fails:**
- Verify broker IP and port
- Check firewall rules
- Test with MQTT Explorer from PC first

**Messages not appearing:**
- Check topic names match exactly
- Verify JSON format in serial monitor
- Increase buffer size if messages truncated

## Alternative: HTTP/REST API

If MQTT is not suitable, consider HTTP POST to a REST API:

```cpp
#include <HTTPClient.h>

void postShotData(const NormalizedShotData& shotData) {
  HTTPClient http;
  http.begin("http://server/api/shot");
  http.addHeader("Content-Type", "application/json");

  String payload = createJsonPayload(shotData);
  int httpCode = http.POST(payload);

  http.end();
}
```

This is simpler but less efficient for real-time updates.
