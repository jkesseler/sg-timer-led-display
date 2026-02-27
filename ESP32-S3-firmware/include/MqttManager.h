#pragma once

#include "ITimerDevice.h"
#include "Logger.h"
#include "WiFiConfig.h"
#include <memory>

/**
 * @brief Manages Wi-Fi connectivity and MQTT publishing
 *
 * Bridges BLE timer events to MQTT topics for PWA display consumption.
 * WiFi connectivity is managed by WiFiConfig class (non-blocking).
 * MQTT connection is established on-demand when WiFi is available.
 *
 * Optimizations:
 * - Pre-allocated JSON buffer to avoid heap fragmentation
 * - Fast path for shot publishing (most common operation)
 * - Cached WiFi connection status to reduce redundant checks
 */
class MqttManager {
private:
  // Connection state
  bool mqttConnected;
  bool wifiWasConnected;  // Cache to detect WiFi state changes
  unsigned long lastMqttCheck;

  // Pre-allocated buffer for JSON serialization (reduces heap fragmentation)
  static constexpr size_t JSON_BUFFER_SIZE = 256;
  char jsonBuffer[JSON_BUFFER_SIZE];

  // Per-device MQTT topics (built at initialize() time using the device ID)
  // Format: timer/<deviceId>/<event>
  static constexpr size_t TOPIC_BUFFER_SIZE = 64;
  char topicPresence[TOPIC_BUFFER_SIZE];          // retained + LWT
  char topicConnectionState[TOPIC_BUFFER_SIZE];   // retained
  char topicDeviceInfo[TOPIC_BUFFER_SIZE];        // retained
  char topicSessionStarted[TOPIC_BUFFER_SIZE];
  char topicSessionStopped[TOPIC_BUFFER_SIZE];
  char topicSessionSuspended[TOPIC_BUFFER_SIZE];
  char topicSessionResumed[TOPIC_BUFFER_SIZE];
  char topicShotDetected[TOPIC_BUFFER_SIZE];
  char topicCountdownComplete[TOPIC_BUFFER_SIZE];

  // Unique MQTT client ID (includes device ID to avoid broker conflicts)
  static constexpr size_t CLIENT_ID_BUFFER_SIZE = 32;
  char mqttClientId[CLIENT_ID_BUFFER_SIZE];

  // Configuration constants
  static constexpr unsigned long MQTT_FAST_CHECK_INTERVAL = 500;   // Check more frequently when publishing
  static constexpr unsigned long MQTT_IDLE_CHECK_INTERVAL = 5000;  // Less frequent when idle

  // Connection management
  bool tryConnect();
  void disconnectMqtt();

  // Builds all device-specific topic strings from the device ID
  void buildTopics(const char* devId);

  // Publishes retained "online"/"offline" to the presence topic
  void publishPresence(bool online);

  // Helper methods - uses pre-allocated buffer
  // retain=true → broker stores the last value for late-joining subscribers
  bool publishJson(const char* topic, const char* jsonPayload, bool retain = false);

public:
  MqttManager();
  ~MqttManager();

  // Lifecycle
  bool initialize();
  void update();  // Called in main loop - handles MQTT loop() and reconnection

  // Connection status - inlined for performance in hot path
  inline bool isHealthy() const {
    return mqttConnected && wifiWasConnected;
  }

  inline bool canPublish() const {
    return mqttConnected;  // Fast check without WiFi re-query
  }

  // Event publishers - called by TimerApplication
  void publishConnectionState(DeviceConnectionState state, const char* deviceName, const char* deviceModel);
  void publishDeviceInfo(const char* deviceName, const char* deviceModel, const char* firmwareVersion);
  void publishSessionStarted(uint32_t sessionId, float startDelaySeconds);
  void publishSessionStopped(uint32_t sessionId, uint16_t totalShots, uint32_t lastShotTimeMs = 0);
  void publishSessionSuspended(uint32_t sessionId);
  void publishSessionResumed(uint32_t sessionId);
  void publishCountdownComplete(uint32_t sessionId);

  // Fast shot publishing - optimized for high-frequency BLE events
  // Returns true if published successfully
  bool publishShotDetected(const NormalizedShotData& shotData);

  // Settings/status
  void reconnect();
  const char* getMqttClientId() const;
};
