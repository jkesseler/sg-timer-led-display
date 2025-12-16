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
  LOG_SYSTEM("Note: MQTT will attempt connection after WiFi is available");

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
    LOG_DEBUG("MQTT", "Local IP: %s, Gateway: %s", WiFiConfig::getLocalIP(), WiFi.gatewayIP().toString().c_str());

    // Set socket timeout to prevent long hangs
    mqttClient.setSocketTimeout(MQTT_SOCKET_TIMEOUT / 1000);  // Convert to seconds

    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      isConnected = true;
      LOG_BLE("MQTT connected successfully");

      // Publish online status
      const char* payload = R"({"state":"CONNECTED","timestamp":)";
      // Note: Full status will be published on next event
    } else {
      isConnected = false;
      int state = mqttClient.state();
      LOG_ERROR("MQTT", "MQTT connection failed, state: %d", state);

      // Diagnostic logging for common error states
      switch (state) {
        case -4:
          LOG_ERROR("MQTT", "Connection timeout (state -4)");
          LOG_ERROR("MQTT", "Broker unreachable at %s:%d", MQTT_BROKER_IP, MQTT_BROKER_PORT);
          LOG_ERROR("MQTT", "Troubleshooting:");
          LOG_ERROR("MQTT", "  - Verify broker IP and port are correct in common.h");
          LOG_ERROR("MQTT", "  - Check ESP32 is on same WiFi network as broker");
          LOG_ERROR("MQTT", "  - Ensure broker (Mosquitto) is running");
          LOG_ERROR("MQTT", "  - Check firewall on broker machine (port 1883)");
          LOG_ERROR("MQTT", "  - Try: mosquitto -v (to see if broker is listening)");
          break;
        case -2:
          LOG_ERROR("MQTT", "Connection failed - TCP socket timeout (state -2)");
          LOG_ERROR("MQTT", "Possible causes:");
          LOG_ERROR("MQTT", "  1. Broker IP %s is incorrect or not reachable", MQTT_BROKER_IP);
          LOG_ERROR("MQTT", "  2. Network routing issue - check with: ping %s", MQTT_BROKER_IP);
          LOG_ERROR("MQTT", "  3. Broker not listening on port 1883");
          LOG_ERROR("MQTT", "  4. Firewall blocking TCP port 1883");
          LOG_ERROR("MQTT", "  5. WiFi network isolation - ESP32 can't reach broker machine");
          LOG_ERROR("MQTT", "Test on broker: mosquitto_sub -h %s -t 'test' -p %d", MQTT_BROKER_IP, MQTT_BROKER_PORT);
          break;
        case -3:
          LOG_ERROR("MQTT", "Connection lost");
          break;
        case 1:
          LOG_ERROR("MQTT", "Bad protocol version");
          break;
        case 2:
          LOG_ERROR("MQTT", "Bad client ID - try changing MQTT_CLIENT_ID in common.h");
          break;
        case 3:
          LOG_ERROR("MQTT", "Broker unavailable");
          break;
        case 4:
          LOG_ERROR("MQTT", "Bad credentials - check MQTT_USER and MQTT_PASSWORD in common.h");
          break;
        case 5:
          LOG_ERROR("MQTT", "Not authorized - verify MQTT credentials");
          break;
        default:
          LOG_ERROR("MQTT", "Unknown error state: %d - check broker IP/port in common.h", state);
      }
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

void MqttManager::publishSessionStopped(uint32_t sessionId, uint16_t totalShots, uint32_t lastShotTimeMs) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["totalShots"] = totalShots;
  if (lastShotTimeMs > 0) {
    doc["lastShotTimeMs"] = lastShotTimeMs;
  }
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
