#include "MqttManager.h"
#include "common.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Global static for PubSubClient (required for callback)
static WiFiClient espClient;
static PubSubClient mqttClient(espClient);

// MQTT Topic definitions - use PROGMEM-style storage for efficiency
namespace Topics {
  const char* CONNECTION_STATE = "timer/connection/state";
  const char* SESSION_STARTED = "timer/session/started";
  const char* SESSION_STOPPED = "timer/session/stopped";
  const char* SESSION_SUSPENDED = "timer/session/suspended";
  const char* SESSION_RESUMED = "timer/session/resumed";
  const char* SHOT_DETECTED = "timer/shot/detected";
  const char* COUNTDOWN_COMPLETE = "timer/countdown/complete";
  const char* DEVICE_INFO = "timer/device/info";
}

// Connection state strings for MQTT - static to avoid repeated string construction
namespace ConnectionStates {
  const char* DISCONNECTED = "DISCONNECTED";
  const char* SCANNING = "SCANNING";
  const char* CONNECTING = "CONNECTING";
  const char* CONNECTED = "CONNECTED";
  const char* ERROR = "ERROR";
}

static const char* connectionStateToString(DeviceConnectionState state) {
  switch (state) {
    case DeviceConnectionState::DISCONNECTED:
      return ConnectionStates::DISCONNECTED;
    case DeviceConnectionState::SCANNING:
      return ConnectionStates::SCANNING;
    case DeviceConnectionState::CONNECTING:
      return ConnectionStates::CONNECTING;
    case DeviceConnectionState::CONNECTED:
      return ConnectionStates::CONNECTED;
    case DeviceConnectionState::ERROR:
      return ConnectionStates::ERROR;
    default:
      return ConnectionStates::ERROR;
  }
}

MqttManager::MqttManager()
  : mqttConnected(false),
    wifiWasConnected(false),
    lastMqttCheck(0) {
}

MqttManager::~MqttManager() {
  if (mqttConnected) {
    disconnectMqtt();
  }
}

bool MqttManager::initialize() {
  LOG_SYSTEM("Initializing MQTT Manager");

  // Set MQTT broker details
  mqttClient.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT);

  // Set a reasonable buffer size for our messages
  mqttClient.setBufferSize(512);

  // Shorter socket timeout for faster failure detection
  mqttClient.setSocketTimeout(3);  // 3 seconds

  // Keep-alive interval
  mqttClient.setKeepAlive(60);  // 60 seconds keep-alive

  LOG_SYSTEM("MQTT configured for %s:%d", MQTT_BROKER_IP, MQTT_BROKER_PORT);
  LOG_SYSTEM("Note: MQTT will connect when WiFi becomes available");

  return true;
}

bool MqttManager::tryConnect() {
  // Check WiFi first - fast path exit
  if (!WiFiConfig::isConnected()) {
    if (wifiWasConnected) {
      LOG_SYSTEM("WiFi disconnected - MQTT unavailable");
      wifiWasConnected = false;
      mqttConnected = false;
    }
    return false;
  }

  // Update WiFi state cache
  if (!wifiWasConnected) {
    wifiWasConnected = true;
    LOG_SYSTEM("WiFi connected - MQTT can now connect");
  }

  // Already connected?
  if (mqttClient.connected()) {
    if (!mqttConnected) {
      mqttConnected = true;
      LOG_BLE("MQTT connection restored");
    }
    return true;
  }

  // Need to connect
  if (mqttConnected) {
    LOG_SYSTEM("MQTT disconnected - reconnecting...");
    mqttConnected = false;
  }

  LOG_DEBUG("MQTT", "Attempting connection to %s:%d", MQTT_BROKER_IP, MQTT_BROKER_PORT);

  if (mqttClient.connect(MQTT_CLIENT_ID)) {
    mqttConnected = true;
    LOG_BLE("MQTT connected successfully");
    return true;
  }

  // Connection failed - log error (but don't spam)
  int state = mqttClient.state();
  LOG_ERROR("MQTT", "Connection failed (state: %d)", state);

  // Only log detailed diagnostics occasionally
  static unsigned long lastDiagnosticLog = 0;
  if (millis() - lastDiagnosticLog > 30000) {  // Every 30 seconds max
    lastDiagnosticLog = millis();
    switch (state) {
      case -4:
        LOG_ERROR("MQTT", "Timeout - broker unreachable at %s:%d", MQTT_BROKER_IP, MQTT_BROKER_PORT);
        break;
      case -2:
        LOG_ERROR("MQTT", "Network unreachable");
        break;
      case 2:
        LOG_ERROR("MQTT", "Bad client ID");
        break;
      case 4:
        LOG_ERROR("MQTT", "Bad credentials");
        break;
      case 5:
        LOG_ERROR("MQTT", "Not authorized");
        break;
    }
  }

  return false;
}

void MqttManager::disconnectMqtt() {
  if (mqttClient.connected()) {
    mqttClient.disconnect();
  }
  mqttConnected = false;
}

void MqttManager::update() {
  unsigned long now = millis();

  // Determine check interval based on activity
  // When disconnected, check more frequently to reconnect faster
  unsigned long checkInterval = mqttConnected ? MQTT_IDLE_CHECK_INTERVAL : MQTT_FAST_CHECK_INTERVAL;

  if (now - lastMqttCheck >= checkInterval) {
    lastMqttCheck = now;
    tryConnect();
  }

  // CRITICAL: Always call loop() when connected to handle incoming messages
  // and maintain the connection (ping/pong). This must happen every iteration.
  if (mqttClient.connected()) {
    mqttClient.loop();
  }
}

bool MqttManager::publishJson(const char* topic, const char* jsonPayload) {
  // Fast path - check connection status (already cached)
  if (!mqttConnected) {
    return false;
  }

  // Double-check actual connection (handles edge cases)
  if (!mqttClient.connected()) {
    mqttConnected = false;
    return false;
  }

  // Publish the message
  if (mqttClient.publish(topic, jsonPayload)) {
    LOG_DEBUG("MQTT", "Published to %s", topic);
    return true;
  }

  LOG_ERROR("MQTT", "Failed to publish to %s", topic);
  return false;
}

void MqttManager::publishConnectionState(DeviceConnectionState state, const char* deviceName, const char* deviceModel) {
  JsonDocument doc;
  doc["state"] = connectionStateToString(state);
  if (deviceName) {
    doc["deviceName"] = deviceName;
  }
  if (deviceModel) {
    doc["deviceModel"] = deviceModel;
  }
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  publishJson(Topics::CONNECTION_STATE, jsonBuffer);
}

void MqttManager::publishDeviceInfo(const char* deviceName, const char* deviceModel, const char* firmwareVersion) {
  JsonDocument doc;
  if (deviceName) {
    doc["deviceName"] = deviceName;
  }
  if (deviceModel) {
    doc["deviceModel"] = deviceModel;
  }
  if (firmwareVersion) {
    doc["firmwareVersion"] = firmwareVersion;
  }
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  publishJson(Topics::DEVICE_INFO, jsonBuffer);
}

void MqttManager::publishSessionStarted(uint32_t sessionId, uint16_t startDelaySeconds) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["startDelaySeconds"] = startDelaySeconds;
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  publishJson(Topics::SESSION_STARTED, jsonBuffer);
}

void MqttManager::publishSessionStopped(uint32_t sessionId, uint16_t totalShots, uint32_t lastShotTimeMs) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["totalShots"] = totalShots;
  if (lastShotTimeMs > 0) {
    doc["lastShotTimeMs"] = lastShotTimeMs;
  }
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  publishJson(Topics::SESSION_STOPPED, jsonBuffer);
}

void MqttManager::publishSessionSuspended(uint32_t sessionId) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  publishJson(Topics::SESSION_SUSPENDED, jsonBuffer);
}

void MqttManager::publishSessionResumed(uint32_t sessionId) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  publishJson(Topics::SESSION_RESUMED, jsonBuffer);
}

bool MqttManager::publishShotDetected(const NormalizedShotData& shotData) {
  // OPTIMIZED: This is the hot path for fast BLE events
  // Uses pre-allocated buffer and minimal overhead

  // Fast fail if not connected
  if (!mqttConnected || !mqttClient.connected()) {
    mqttConnected = false;
    return false;
  }

  // Build JSON directly using snprintf - avoids JsonDocument heap allocation
  // Format: {"sessionId":N,"shotNumber":N,"absoluteTimeMs":N,"splitTimeMs":N,"deviceModel":"X","isFirstShot":B,"timestamp":N}
  int len = snprintf(jsonBuffer, JSON_BUFFER_SIZE,
    "{\"sessionId\":%lu,\"shotNumber\":%u,\"absoluteTimeMs\":%lu,\"splitTimeMs\":%lu,\"deviceModel\":\"%s\",\"isFirstShot\":%s,\"timestamp\":%lu}",
    (unsigned long)shotData.sessionId,
    shotData.shotNumber,
    (unsigned long)shotData.absoluteTimeMs,
    (unsigned long)shotData.splitTimeMs,
    shotData.deviceModel ? shotData.deviceModel : "unknown",
    shotData.isFirstShot ? "true" : "false",
    (unsigned long)millis()
  );

  if (len < 0 || len >= (int)JSON_BUFFER_SIZE) {
    LOG_ERROR("MQTT", "Shot JSON buffer overflow");
    return false;
  }

  // Publish with minimal overhead
  if (mqttClient.publish(Topics::SHOT_DETECTED, jsonBuffer)) {
    LOG_DEBUG("MQTT", "Shot #%u published", shotData.shotNumber);
    return true;
  }

  LOG_ERROR("MQTT", "Failed to publish shot #%u", shotData.shotNumber);
  return false;
}

void MqttManager::publishCountdownComplete(uint32_t sessionId) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  publishJson(Topics::COUNTDOWN_COMPLETE, jsonBuffer);
}

void MqttManager::reconnect() {
  LOG_SYSTEM("Manually triggering MQTT reconnection");
  disconnectMqtt();
  lastMqttCheck = 0;  // Force immediate reconnection attempt
  tryConnect();
}

const char* MqttManager::getMqttClientId() const {
  return MQTT_CLIENT_ID;
}
