#include "MqttManager.h"
#include "WiFiConfig.h"
#include "DeviceId.h"
#include "common.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <algorithm>

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
    lastMqttCheck(0),
    reconnectBackoffMs(MQTT_FAST_CHECK_INTERVAL),
    taskHandle(nullptr),
    eventQueue(nullptr),
    totalEventsPublished(0),
    publishFailures(0) {
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
  if (taskHandle) {
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
  }
  if (eventQueue) {
    vQueueDelete(eventQueue);
    eventQueue = nullptr;
  }
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

  // PubSubClient's connect() busy-waits for CONNACK with no yield() call
  // (unlike its readByte() path, which does), pinned here on MqttTask at
  // priority 1 - a socketTimeout much above the ESP-IDF task watchdog's
  // ~5s idle-task window would risk a WDT reset on a slow/failed connect.
  // Leave it as-is; a burst of BLE notifications during loop()'s keepalive
  // wait (which does yield) is the actual failure mode, so widen that
  // instead of the connect() timeout.
  mqttClient.setSocketTimeout(4);  // 4 seconds

  // BLE and WiFi timeshare one radio - a burst of BLE notifications can
  // delay a PINGRESP by more than a second without the link actually being
  // down. 5s was manufacturing disconnects during active shot sessions.
  mqttClient.setKeepAlive(30);  // 30 seconds keep-alive

  LOG_SYSTEM("MQTT configured for %s:%d", mqttServer, mqttPort);
  LOG_SYSTEM("MQTT client ID: %s", mqttClientId);
  LOG_SYSTEM("Note: MQTT will connect when WiFi becomes available");

  // All MQTT socket I/O happens on this dedicated task from here on -
  // nothing else may call tryConnect()/mqttClient.loop()/publish*().
  eventQueue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(MqttEvent));
  if (!eventQueue) {
    LOG_ERROR("MQTT", "Failed to create MQTT event queue");
    return false;
  }
  startTask();

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
    reconnectBackoffMs = MQTT_FAST_CHECK_INTERVAL;  // Reset backoff on success
    LOG_INFO("MQTT", "MQTT connected successfully");
    // Announce presence. Retained so late-joining displays see "online" immediately.
    publishPresence(true);
    return true;
  }

  // Connection failed - back off before the next attempt, capped at the idle
  // interval, so a still-contended radio gets progressively more room.
  reconnectBackoffMs = std::min(reconnectBackoffMs * 2, MQTT_IDLE_CHECK_INTERVAL);

  // Log error (but don't spam)
  int state = mqttClient.state();
  // WiFiConfig::update() only logs WiFi state on a change it catches at its
  // own 5s check interval, so a drop-and-recover (or a silent re-associate
  // with a new/lost DHCP lease) between checks leaves no trace anywhere else.
  // Capture link state right here, at the point of failure, to tell a radio-
  // arbitration problem (status/RSSI/IP all fine, TCP just can't complete)
  // apart from an actual WiFi-side link problem.
  LOG_ERROR("MQTT", "Connection failed (state: %d, wifi status: %d, RSSI: %d dBm, IP: %s)",
            state, (int)WiFi.status(), WiFi.RSSI(), WiFi.localIP().toString().c_str());

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

void MqttManager::startTask() {
  if (taskHandle) return;  // Already running
  xTaskCreatePinnedToCore(taskEntry, "MqttTask", TASK_STACK_SIZE, this,
                           TASK_PRIORITY, &taskHandle, TASK_CORE);
}

void MqttManager::taskEntry(void* param) {
  static_cast<MqttManager*>(param)->taskLoop();
}

void MqttManager::taskLoop() {
  for (;;) {
    unsigned long now = millis();

    // Determine check interval based on activity. When disconnected, back
    // off exponentially instead of retrying at a fixed fast interval -
    // repeatedly reconnecting the same client ID into a still-contended
    // radio just delays the CONNACK further and keeps failing.
    unsigned long checkInterval = mqttConnected ? MQTT_IDLE_CHECK_INTERVAL : reconnectBackoffMs;

    if (now - lastMqttCheck >= checkInterval) {
      lastMqttCheck = now;
      tryConnect();
    }

    // CRITICAL: Always call loop() when connected to handle incoming messages
    // and maintain the connection (ping/pong). This must happen every iteration.
    if (mqttClient.connected()) {
      mqttClient.loop();
    }

    drainEventQueue();

    vTaskDelay(pdMS_TO_TICKS(TASK_LOOP_DELAY_MS));
  }
}

void MqttManager::drainEventQueue() {
  if (!eventQueue || uxQueueMessagesWaiting(eventQueue) == 0) {
    return;
  }

  // Not connected - leave events queued rather than discard them. They
  // publish in order once the connection is restored; EVENT_QUEUE_SIZE
  // bounds how much can build up during an outage.
  if (!mqttClient.connected()) {
    return;
  }

  uint16_t processed = 0;
  MqttEvent event;
  while (processed < MAX_EVENTS_PER_DRAIN_CYCLE &&
         xQueueReceive(eventQueue, &event, 0) == pdTRUE) {
    bool published = dispatchEvent(event);
    processed++;

    if (!published) {
      // The write that just failed already paid WiFiClient::write()'s full
      // internal retry budget (WIFI_CLIENT_MAX_WRITE_RETRY x
      // WIFI_CLIENT_SELECT_TIMEOUT_US - up to ~10s, and NOT affected by our
      // setSocketTimeout()) while mqttClient.connected() kept reporting
      // true the whole time, because the socket only looks dead once the OS
      // notices independently. Don't let every other already-queued event
      // pay that same ~10s tax one at a time - close the socket directly
      // (not via mqttClient.disconnect(), which would attempt one more
      // write - the MQTT DISCONNECT packet - and risk the same stall again)
      // and force an immediate reconnect. The rest of the backlog stays
      // queued for the next drain once that succeeds.
      espClient.stop();
      mqttConnected = false;
      lastMqttCheck = 0;
      reconnectBackoffMs = MQTT_FAST_CHECK_INTERVAL;
      break;
    }
  }
}

bool MqttManager::dispatchEvent(const MqttEvent& event) {
  switch (event.type) {
    case MqttEventType::SHOT_DETECTED:
      if (doPublishShotDetected(event.shot)) {
        totalEventsPublished++;
        return true;
      }
      publishFailures++;
      LOG_WARN("MQTT", "Failed to publish shot #%u", event.shot.shotNumber);
      return false;

    case MqttEventType::SESSION_STARTED:
      return doPublishSessionStarted(event.sessionStarted.sessionId, event.sessionStarted.startDelaySeconds);

    case MqttEventType::SESSION_STOPPED:
      return doPublishSessionStopped(event.sessionStopped.sessionId, event.sessionStopped.totalShots,
                                      event.sessionStopped.lastShotTimeMs);

    case MqttEventType::SESSION_SUSPENDED:
      return doPublishSessionSuspended(event.sessionSimple.sessionId);

    case MqttEventType::SESSION_RESUMED:
      return doPublishSessionResumed(event.sessionSimple.sessionId);

    case MqttEventType::COUNTDOWN_COMPLETE:
      return doPublishCountdownComplete(event.sessionSimple.sessionId);

    case MqttEventType::CONNECTION_STATE:
      return doPublishConnectionState(event.connectionState.state,
                                       event.connectionState.hasDeviceName ? event.connectionState.deviceName : nullptr,
                                       event.connectionState.hasDeviceModel ? event.connectionState.deviceModel : nullptr);

    case MqttEventType::DEVICE_INFO:
      return doPublishDeviceInfo(event.deviceInfo.hasDeviceName ? event.deviceInfo.deviceName : nullptr,
                                  event.deviceInfo.hasDeviceModel ? event.deviceInfo.deviceModel : nullptr,
                                  event.deviceInfo.hasFirmwareVersion ? event.deviceInfo.firmwareVersion : nullptr);
  }
  return true;
}

bool MqttManager::enqueueShot(const NormalizedShotData& shotData) {
  if (!eventQueue) return false;
  MqttEvent event;
  event.type = MqttEventType::SHOT_DETECTED;
  event.shot = shotData;
  return xQueueSend(eventQueue, &event, 0) == pdTRUE;
}

bool MqttManager::enqueueSessionStarted(uint32_t sessionId, float startDelaySeconds) {
  if (!eventQueue) return false;
  MqttEvent event;
  event.type = MqttEventType::SESSION_STARTED;
  event.sessionStarted.sessionId = sessionId;
  event.sessionStarted.startDelaySeconds = startDelaySeconds;
  return xQueueSend(eventQueue, &event, 0) == pdTRUE;
}

bool MqttManager::enqueueSessionStopped(uint32_t sessionId, uint16_t totalShots, uint32_t lastShotTimeMs) {
  if (!eventQueue) return false;
  MqttEvent event;
  event.type = MqttEventType::SESSION_STOPPED;
  event.sessionStopped.sessionId = sessionId;
  event.sessionStopped.totalShots = totalShots;
  event.sessionStopped.lastShotTimeMs = lastShotTimeMs;
  return xQueueSend(eventQueue, &event, 0) == pdTRUE;
}

bool MqttManager::enqueueSessionSuspended(uint32_t sessionId) {
  if (!eventQueue) return false;
  MqttEvent event;
  event.type = MqttEventType::SESSION_SUSPENDED;
  event.sessionSimple.sessionId = sessionId;
  return xQueueSend(eventQueue, &event, 0) == pdTRUE;
}

bool MqttManager::enqueueSessionResumed(uint32_t sessionId) {
  if (!eventQueue) return false;
  MqttEvent event;
  event.type = MqttEventType::SESSION_RESUMED;
  event.sessionSimple.sessionId = sessionId;
  return xQueueSend(eventQueue, &event, 0) == pdTRUE;
}

bool MqttManager::enqueueCountdownComplete(uint32_t sessionId) {
  if (!eventQueue) return false;
  MqttEvent event;
  event.type = MqttEventType::COUNTDOWN_COMPLETE;
  event.sessionSimple.sessionId = sessionId;
  return xQueueSend(eventQueue, &event, 0) == pdTRUE;
}

bool MqttManager::enqueueConnectionState(DeviceConnectionState state, const char* deviceName, const char* deviceModel) {
  if (!eventQueue) return false;
  MqttEvent event;
  event.type = MqttEventType::CONNECTION_STATE;
  event.connectionState.state = state;
  event.connectionState.hasDeviceName = (deviceName != nullptr);
  event.connectionState.hasDeviceModel = (deviceModel != nullptr);
  if (deviceName) {
    strncpy(event.connectionState.deviceName, deviceName, sizeof(event.connectionState.deviceName) - 1);
    event.connectionState.deviceName[sizeof(event.connectionState.deviceName) - 1] = '\0';
  }
  if (deviceModel) {
    strncpy(event.connectionState.deviceModel, deviceModel, sizeof(event.connectionState.deviceModel) - 1);
    event.connectionState.deviceModel[sizeof(event.connectionState.deviceModel) - 1] = '\0';
  }
  return xQueueSend(eventQueue, &event, 0) == pdTRUE;
}

bool MqttManager::enqueueDeviceInfo(const char* deviceName, const char* deviceModel, const char* firmwareVersion) {
  if (!eventQueue) return false;
  MqttEvent event;
  event.type = MqttEventType::DEVICE_INFO;
  event.deviceInfo.hasDeviceName = (deviceName != nullptr);
  event.deviceInfo.hasDeviceModel = (deviceModel != nullptr);
  event.deviceInfo.hasFirmwareVersion = (firmwareVersion != nullptr);
  if (deviceName) {
    strncpy(event.deviceInfo.deviceName, deviceName, sizeof(event.deviceInfo.deviceName) - 1);
    event.deviceInfo.deviceName[sizeof(event.deviceInfo.deviceName) - 1] = '\0';
  }
  if (deviceModel) {
    strncpy(event.deviceInfo.deviceModel, deviceModel, sizeof(event.deviceInfo.deviceModel) - 1);
    event.deviceInfo.deviceModel[sizeof(event.deviceInfo.deviceModel) - 1] = '\0';
  }
  if (firmwareVersion) {
    strncpy(event.deviceInfo.firmwareVersion, firmwareVersion, sizeof(event.deviceInfo.firmwareVersion) - 1);
    event.deviceInfo.firmwareVersion[sizeof(event.deviceInfo.firmwareVersion) - 1] = '\0';
  }
  return xQueueSend(eventQueue, &event, 0) == pdTRUE;
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

bool MqttManager::doPublishConnectionState(DeviceConnectionState state, const char* deviceName, const char* deviceModel) {
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
  return publishJson(topicConnectionState, jsonBuffer, /*retain=*/true);
}

bool MqttManager::doPublishDeviceInfo(const char* deviceName, const char* deviceModel, const char* firmwareVersion) {
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
  return publishJson(topicDeviceInfo, jsonBuffer, /*retain=*/true);
}

bool MqttManager::doPublishSessionStarted(uint32_t sessionId, float startDelaySeconds) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["startDelaySeconds"] = startDelaySeconds;
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  return publishJson(topicSessionStarted, jsonBuffer);  // ephemeral event - not retained
}

bool MqttManager::doPublishSessionStopped(uint32_t sessionId, uint16_t totalShots, uint32_t lastShotTimeMs) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["totalShots"] = totalShots;
  if (lastShotTimeMs > 0) {
    doc["lastShotTimeMs"] = lastShotTimeMs;
  }
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  return publishJson(topicSessionStopped, jsonBuffer);  // ephemeral event - not retained
}

bool MqttManager::doPublishSessionSuspended(uint32_t sessionId) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  return publishJson(topicSessionSuspended, jsonBuffer);
}

bool MqttManager::doPublishSessionResumed(uint32_t sessionId) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  return publishJson(topicSessionResumed, jsonBuffer);
}

bool MqttManager::doPublishShotDetected(const NormalizedShotData& shotData) {
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
    shotData.deviceModel,  // fixed-size char[] member, always a valid C-string
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

bool MqttManager::doPublishCountdownComplete(uint32_t sessionId) {
  JsonDocument doc;
  doc["sessionId"] = sessionId;
  doc["timestamp"] = millis();

  serializeJson(doc, jsonBuffer, JSON_BUFFER_SIZE);
  return publishJson(topicCountdownComplete, jsonBuffer);
}

void MqttManager::reconnect() {
  LOG_SYSTEM("Manually triggering MQTT reconnection");
  disconnectMqtt();
  lastMqttCheck = 0;  // Force immediate reconnection attempt
  reconnectBackoffMs = MQTT_FAST_CHECK_INTERVAL;
  tryConnect();
}

const char* MqttManager::getMqttClientId() const {
  return mqttClientId;
}
