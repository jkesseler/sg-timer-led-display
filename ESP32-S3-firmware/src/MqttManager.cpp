#include "MqttManager.h"
#include "WiFiConfig.h"
#include "DeviceId.h"
#include "common.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Global static for PubSubClient (required for callback)
static WiFiClient espClient;
static PubSubClient mqttClient(espClient);

// Connection state strings for MQTT - static to avoid repeated string construction
// (Topic strings are built per-device in buildTopics())

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
  // Zero-initialise all topic buffers
  memset(topicPresence, 0, sizeof(topicPresence));
  memset(topicConnectionState, 0, sizeof(topicConnectionState));
  memset(topicDeviceInfo, 0, sizeof(topicDeviceInfo));
  memset(topicSessionStarted, 0, sizeof(topicSessionStarted));
  memset(topicSessionStopped, 0, sizeof(topicSessionStopped));
  memset(topicSessionSuspended, 0, sizeof(topicSessionSuspended));
  memset(topicSessionResumed, 0, sizeof(topicSessionResumed));
  memset(topicShotDetected, 0, sizeof(topicShotDetected));
  memset(topicCountdownComplete, 0, sizeof(topicCountdownComplete));
  memset(mqttClientId, 0, sizeof(mqttClientId));
}

void MqttManager::buildTopics(const char* devId) {
  // All event topics are scoped under timer/<deviceId>/
  // Retained topics (presence, connection/state, device/info) allow late-joining
  // displays to receive the current state immediately upon subscription.
  snprintf(topicPresence,        TOPIC_BUFFER_SIZE, "timer/%s/presence",          devId);
  snprintf(topicConnectionState, TOPIC_BUFFER_SIZE, "timer/%s/connection/state",  devId);
  snprintf(topicDeviceInfo,      TOPIC_BUFFER_SIZE, "timer/%s/device/info",       devId);
  snprintf(topicSessionStarted,  TOPIC_BUFFER_SIZE, "timer/%s/session/started",   devId);
  snprintf(topicSessionStopped,  TOPIC_BUFFER_SIZE, "timer/%s/session/stopped",   devId);
  snprintf(topicSessionSuspended,TOPIC_BUFFER_SIZE, "timer/%s/session/suspended", devId);
  snprintf(topicSessionResumed,  TOPIC_BUFFER_SIZE, "timer/%s/session/resumed",   devId);
  snprintf(topicShotDetected,    TOPIC_BUFFER_SIZE, "timer/%s/shot/detected",     devId);
  snprintf(topicCountdownComplete,TOPIC_BUFFER_SIZE,"timer/%s/countdown/complete",devId);
  // Unique per-device client ID prevents broker from dropping duplicate connections
  snprintf(mqttClientId, CLIENT_ID_BUFFER_SIZE, "pewpew-%s", devId);
  LOG_DEBUG("MQTT", "Topics built for device: %s", devId);
  LOG_DEBUG("MQTT", "Presence topic: %s", topicPresence);
}

void MqttManager::publishPresence(bool online) {
  // Retained + QoS 1 so the broker stores the last value.
  // Any display that subscribes later immediately receives the current state.
  const char* payload = online ? "online" : "offline";
  mqttClient.publish(topicPresence, (const uint8_t*)payload, strlen(payload), /*retain=*/true);
  LOG_INFO("MQTT", "Presence: %s", payload);
}

MqttManager::~MqttManager() {
  if (mqttConnected) {
    disconnectMqtt();
  }
}

bool MqttManager::initialize() {
  LOG_SYSTEM("Initializing MQTT Manager");

  // Build device-specific topic strings using the unique device ID.
  // Must be called after deviceId.initialize().
  buildTopics(deviceId.get().c_str());

  // Get MQTT configuration from WiFiConfig (will use defaults if not configured)
  const char* mqttServer = WiFiConfig::getMqttServer();
  int mqttPort = WiFiConfig::getMqttPort();

  // Check if MQTT server is configured
  if (!mqttServer || mqttServer[0] == '\0') {
    LOG_SYSTEM("MQTT server not configured - MQTT disabled");
    return false;
  }

  // Set MQTT broker details
  mqttClient.setServer(mqttServer, mqttPort);

  // Set a reasonable buffer size for our messages
  mqttClient.setBufferSize(512);

  // Shorter socket timeout for faster failure detection
  mqttClient.setSocketTimeout(3);  // 3 seconds

  // Keep-alive interval
  mqttClient.setKeepAlive(60);  // 60 seconds keep-alive

  LOG_SYSTEM("MQTT configured for %s:%d", mqttServer, mqttPort);
  LOG_SYSTEM("MQTT client ID: %s", mqttClientId);
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
      LOG_INFO("MQTT", "MQTT connection restored");
    }
    return true;
  }

  // Need to connect
  if (mqttConnected) {
    LOG_SYSTEM("MQTT disconnected - reconnecting...");
    mqttConnected = false;
  }

  // Get MQTT configuration
  const char* mqttServer = WiFiConfig::getMqttServer();
  int mqttPort = WiFiConfig::getMqttPort();
  const char* mqttUser = WiFiConfig::getMqttUser();
  const char* mqttPassword = WiFiConfig::getMqttPassword();

  LOG_DEBUG("MQTT", "Attempting connection to %s:%d", mqttServer, mqttPort);

  // Connect with or without authentication.
  // The Last Will & Testament (LWT) ensures the broker automatically publishes
  // "offline" to the presence topic if this device disconnects unexpectedly
  // (power loss, WiFi drop, etc.). Retained=true means displays that subscribe
  // later will immediately see the "offline" state.
  const uint8_t willQos = 0;
  const bool willRetain = true;
  const char* willMessage = "offline";

  bool connected = false;
  bool useAuth = (mqttUser && mqttUser[0] != '\0' && mqttPassword && mqttPassword[0] != '\0');

  if (useAuth) {
    LOG_DEBUG("MQTT", "Using authentication (user: %s)", mqttUser);
    connected = mqttClient.connect(mqttClientId, mqttUser, mqttPassword,
                                   topicPresence, willQos, willRetain, willMessage);
  } else {
    LOG_DEBUG("MQTT", "Connecting without authentication");
    connected = mqttClient.connect(mqttClientId,
                                   topicPresence, willQos, willRetain, willMessage);
  }

  if (connected) {
    mqttConnected = true;
    LOG_INFO("MQTT", "MQTT connected successfully");
    // Announce presence. Retained so late-joining displays see "online" immediately.
    publishPresence(true);
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
        LOG_ERROR("MQTT", "Timeout - broker unreachable at %s:%d", mqttServer, mqttPort);
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

bool MqttManager::publishJson(const char* topic, const char* jsonPayload, bool retain) {
  // Fast path - check connection status (already cached)
  if (!mqttConnected) {
    return false;
  }

  // Double-check actual connection (handles edge cases)
  if (!mqttClient.connected()) {
    mqttConnected = false;
    return false;
  }

  // Publish the message.
  // retain=true → broker stores the last value and delivers it immediately
  // to any new subscriber ("late joiners"), enabling displays that power-on
  // after the device to see the current state without any re-publish.
  if (mqttClient.publish(topic, jsonPayload, retain)) {
    LOG_DEBUG("MQTT", "Published to %s (retain=%s)", topic, retain ? "y" : "n");
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
  // Retained: displays that connect later see the current BLE connection state.
  publishJson(topicConnectionState, jsonBuffer, /*retain=*/true);
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
  doc["deviceId"] = deviceId.get().c_str();  // Embed deviceId so displays can identify the source
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  // Retained: late-joining displays learn device identity without a re-announce.
  publishJson(topicDeviceInfo, jsonBuffer, /*retain=*/true);
}

void MqttManager::publishSessionStarted(uint32_t sessionId, float startDelaySeconds) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["startDelaySeconds"] = startDelaySeconds;
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  publishJson(topicSessionStarted, jsonBuffer);  // ephemeral event - not retained
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
  publishJson(topicSessionStopped, jsonBuffer);  // ephemeral event - not retained
}

void MqttManager::publishSessionSuspended(uint32_t sessionId) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  publishJson(topicSessionSuspended, jsonBuffer);
}

void MqttManager::publishSessionResumed(uint32_t sessionId) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  publishJson(topicSessionResumed, jsonBuffer);
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
  if (mqttClient.publish(topicShotDetected, jsonBuffer)) {
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
  publishJson(topicCountdownComplete, jsonBuffer);
}

void MqttManager::reconnect() {
  LOG_SYSTEM("Manually triggering MQTT reconnection");
  disconnectMqtt();
  lastMqttCheck = 0;  // Force immediate reconnection attempt
  tryConnect();
}

const char* MqttManager::getMqttClientId() const {
  return mqttClientId;
}
