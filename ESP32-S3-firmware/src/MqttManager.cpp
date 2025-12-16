#include "MqttManager.h"
#include "common.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Global static for PubSubClient (required for callback)
static WiFiClient espClient;
static PubSubClient mqttClient(espClient);

// Static instance for callback handling
static MqttManager* mqttManagerInstance = nullptr;

// MQTT Topic definitions
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

// Connection state strings for MQTT
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
  : isConnected(false),
    lastMqttCheck(0) {
  mqttManagerInstance = this;
}

MqttManager::~MqttManager() {
  if (isConnected) {
    disconnectMqtt();
  }
  mqttManagerInstance = nullptr;
}

bool MqttManager::initialize() {
  LOG_SYSTEM("Initializing MQTT Manager");

  // Set MQTT broker details
  mqttClient.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT);

  LOG_SYSTEM("MQTT configured for %s:%d", MQTT_BROKER_IP, MQTT_BROKER_PORT);

  // WiFi is managed by WiFiConfig (non-blocking)
  // MQTT will connect on-demand when WiFi becomes available
  lastMqttCheck = 0;  // Allow immediate connection attempt
  ensureMqttConnected();

  return true;  // Manager initializes successfully even if MQTT connection fails initially
}

void MqttManager::ensureMqttConnected() {
  unsigned long now = millis();

  // Check MQTT connection periodically
  if (now - lastMqttCheck < MQTT_CHECK_INTERVAL) {
    return;
  }

  lastMqttCheck = now;

  // If Wi-Fi is not available, we can't maintain MQTT connection
  if (!WiFiConfig::isConnected()) {
    if (isConnected) {
      LOG_SYSTEM("WiFi disconnected - MQTT will reconnect when WiFi available");
      isConnected = false;
    }
    return;
  }

  // Try to connect if not already connected
  if (!mqttClient.connected()) {
    if (isConnected) {
      LOG_SYSTEM("MQTT disconnected - attempting to reconnect");
    }

    LOG_BLE("Attempting MQTT connection to %s:%d", MQTT_BROKER_IP, MQTT_BROKER_PORT);

    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      isConnected = true;
      LOG_BLE("MQTT connected successfully");

      // Publish online status
      const char* payload = R"({"state":"CONNECTED","timestamp":)";
      // Note: Full status will be published on next event
    } else {
      isConnected = false;
      LOG_ERROR("MQTT", "MQTT connection failed, error: %d", mqttClient.state());
    }
  } else {
    if (!isConnected) {
      isConnected = true;
      LOG_BLE("MQTT connection restored");
    }

    // Keep alive - process MQTT messages
    mqttClient.loop();
  }
}

void MqttManager::disconnectMqtt() {
  if (mqttClient.connected()) {
    mqttClient.disconnect();
  }
  isConnected = false;
}

void MqttManager::update() {
  // WiFi is managed by WiFiConfig - just ensure MQTT stays connected
  ensureMqttConnected();
}

void MqttManager::publishJson(const char* topic, const char* jsonPayload) {
  // Only publish if MQTT is connected and Wi-Fi is available
  if (!isConnected || !WiFiConfig::isConnected()) {
    LOG_DEBUG("MQTT", "MQTT not ready, skipping publish to %s", topic);
    return;
  }

  if (mqttClient.connected()) {
    if (mqttClient.publish(topic, jsonPayload)) {
      LOG_DEBUG("MQTT", "Published to %s", topic);
    } else {
      LOG_ERROR("MQTT", "Failed to publish to %s", topic);
    }
  } else {
    LOG_ERROR("MQTT", "MQTT not connected, cannot publish to %s", topic);
  }
}

void MqttManager::publishConnectionState(DeviceConnectionState state, const char* deviceName, const char* deviceModel) {
  // Create JSON payload
  JsonDocument doc;
  doc["state"] = connectionStateToString(state);
  if (deviceName) {
    doc["deviceName"] = deviceName;
  }
  if (deviceModel) {
    doc["deviceModel"] = deviceModel;
  }
  doc["timestamp"] = millis();

  char buffer[256];
  serializeJson(doc, buffer, sizeof(buffer));

  publishJson(Topics::CONNECTION_STATE, buffer);
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

  char buffer[256];
  serializeJson(doc, buffer, sizeof(buffer));

  publishJson(Topics::DEVICE_INFO, buffer);
}

void MqttManager::publishSessionStarted(uint32_t sessionId, uint16_t startDelaySeconds) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["startDelaySeconds"] = startDelaySeconds;
  doc["timestamp"] = millis();

  char buffer[128];
  serializeJson(doc, buffer, sizeof(buffer));

  publishJson(Topics::SESSION_STARTED, buffer);
}

void MqttManager::publishSessionStopped(uint32_t sessionId, uint16_t totalShots) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["totalShots"] = totalShots;
  doc["timestamp"] = millis();

  char buffer[128];
  serializeJson(doc, buffer, sizeof(buffer));

  publishJson(Topics::SESSION_STOPPED, buffer);
}

void MqttManager::publishSessionSuspended(uint32_t sessionId) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["timestamp"] = millis();

  char buffer[96];
  serializeJson(doc, buffer, sizeof(buffer));

  publishJson(Topics::SESSION_SUSPENDED, buffer);
}

void MqttManager::publishSessionResumed(uint32_t sessionId) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["timestamp"] = millis();

  char buffer[96];
  serializeJson(doc, buffer, sizeof(buffer));

  publishJson(Topics::SESSION_RESUMED, buffer);
}

void MqttManager::publishShotDetected(const NormalizedShotData& shotData) {
  JsonDocument doc;
  doc["sessionId"] = shotData.sessionId;
  doc["shotNumber"] = shotData.shotNumber;
  doc["absoluteTimeMs"] = shotData.absoluteTimeMs;
  doc["splitTimeMs"] = shotData.splitTimeMs;
  doc["deviceModel"] = shotData.deviceModel;
  doc["isFirstShot"] = shotData.isFirstShot;
  doc["timestamp"] = millis();

  char buffer[256];
  serializeJson(doc, buffer, sizeof(buffer));

  publishJson(Topics::SHOT_DETECTED, buffer);
}

void MqttManager::publishCountdownComplete(uint32_t sessionId) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["timestamp"] = millis();

  char buffer[96];
  serializeJson(doc, buffer, sizeof(buffer));

  publishJson(Topics::COUNTDOWN_COMPLETE, buffer);
}

void MqttManager::reconnect() {
  LOG_SYSTEM("Manually triggering MQTT reconnection");
  disconnectMqtt();
  lastMqttCheck = 0;  // Force immediate reconnection attempt
  ensureMqttConnected();
}

const char* MqttManager::getMqttClientId() const {
  return MQTT_CLIENT_ID;
}
