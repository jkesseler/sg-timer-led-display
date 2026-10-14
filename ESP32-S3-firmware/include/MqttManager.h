#pragma once

#include "ITimerDevice.h"
#include "MqttEvent.h"
#include "FollowedTimer.h"
#include "Logger.h"
#include <memory>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Next / on-deck shooters, published by the score-keeping app after a
// session stops. An empty onDeck means there is no one on deck.
struct UpNextInfo {
  uint32_t sessionId;
  char next[48];
  char onDeck[48];
};

/**
 * @brief Manages Wi-Fi connectivity and MQTT publishing
 *
 * Bridges BLE timer events to MQTT topics for PWA display consumption.
 * WiFi connectivity is managed by WiFiConfig class (non-blocking).
 * MQTT connection is established on-demand when WiFi is available.
 *
 * All MQTT socket I/O (connect, loop(), publish) runs on a single
 * dedicated FreeRTOS task pinned to core 0, created in initialize().
 * Callers on any other task (BLE notification callbacks, the main loop)
 * only ever call the enqueue*() methods, which are safe to call from
 * anywhere. This keeps PubSubClient and the JSON scratch buffer under
 * single-task ownership - see MqttEvent above.
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
  unsigned long reconnectBackoffMs;  // Grows on repeated failure so a contended
                                      // radio doesn't get hammered every 500ms

  // Background task that owns all MQTT socket I/O (core 0)
  TaskHandle_t taskHandle;
  QueueHandle_t eventQueue;
  QueueHandle_t upNextQueue;  // length 1, overwritten: only the latest matters
  QueueHandle_t followedEventQueue;  // FIFO: every shot must reach the display
  uint32_t totalEventsPublished;
  uint32_t publishFailures;

  static constexpr uint16_t EVENT_QUEUE_SIZE = 32;
  static constexpr uint16_t FOLLOWED_EVENT_QUEUE_SIZE = 32;
  static constexpr uint16_t MAX_EVENTS_PER_DRAIN_CYCLE = 8;
  static constexpr uint32_t TASK_LOOP_DELAY_MS = 20;
  static constexpr uint32_t TASK_STACK_SIZE = 4096;
  static constexpr UBaseType_t TASK_PRIORITY = 1;
  static constexpr BaseType_t TASK_CORE = 0;

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
  char topicSessionUpNext[TOPIC_BUFFER_SIZE];

  // Set when the board shows another bridge's timer instead of relaying its own.
  bool isFollower;
  char topicFollowedTimer[TOPIC_BUFFER_SIZE];  // timer/<followed id>/#
  FollowedTimer followedTimer;  // MQTT task only

  // Unique MQTT client ID (includes device ID to avoid broker conflicts)
  static constexpr size_t CLIENT_ID_BUFFER_SIZE = 32;
  char mqttClientId[CLIENT_ID_BUFFER_SIZE];

  // Configuration constants
  static constexpr unsigned long MQTT_FAST_CHECK_INTERVAL = 500;   // Check more frequently when publishing
  static constexpr unsigned long MQTT_IDLE_CHECK_INTERVAL = 5000;  // Less frequent when idle

  // Connection management - called only from the background task (taskLoop)
  bool tryConnect();
  void disconnectMqtt();

  // Builds all device-specific topic strings from the device ID
  void buildTopics(const char* devId);

  // Publishes retained "online"/"offline" to the presence topic
  void publishPresence(bool online);

  void handleMessage(const char* topic, const uint8_t* payload, unsigned int length);
  void handleUpNext(const uint8_t* payload, unsigned int length);
  void handleFollowedMessage(const char* topic, const uint8_t* payload, unsigned int length);
  void clearOwnRetainedTopics();

  // Helper methods - uses pre-allocated buffer
  // retain=true → broker stores the last value for late-joining subscribers
  bool publishJson(const char* topic, const char* jsonPayload, bool retain = false);

  // Actual publishers - touch PubSubClient/jsonBuffer directly, so these may
  // only run on the background task. dispatchEvent() is their sole caller.
  // All return the underlying write's success so a stalled/dead connection
  // (see drainEventQueue()) can be detected regardless of event type.
  bool doPublishConnectionState(DeviceConnectionState state, const char* deviceName, const char* deviceModel);
  bool doPublishDeviceInfo(const char* deviceName, const char* deviceModel, const char* firmwareVersion);
  bool doPublishSessionStarted(uint32_t sessionId, float startDelaySeconds);
  bool doPublishSessionStopped(uint32_t sessionId, uint16_t totalShots, uint32_t lastShotTimeMs);
  bool doPublishSessionSuspended(uint32_t sessionId);
  bool doPublishSessionResumed(uint32_t sessionId);
  bool doPublishCountdownComplete(uint32_t sessionId);
  bool doPublishShotDetected(const NormalizedShotData& shotData);

  // Background task - the only code allowed to call the doPublish*() methods
  void startTask();
  static void taskEntry(void* param);
  void taskLoop();
  void drainEventQueue();
  bool dispatchEvent(const MqttEvent& event);

public:
  MqttManager();
  ~MqttManager();

  // Lifecycle. On success, creates the event queue and starts the
  // background MQTT task; nothing else needs to be called from the main loop.
  // followedDeviceId ("+" = any): show that bridge's events; publish no presence, so displays never pick this board.
  bool initialize(const char* followedDeviceId = nullptr);

  // Connection status - inlined for performance in hot path
  inline bool isHealthy() const {
    return mqttConnected && wifiWasConnected;
  }

  inline bool canPublish() const {
    return mqttConnected;  // Fast check without WiFi re-query
  }

  // Thread-safe event submission - safe to call from any task (BLE
  // notification callbacks or the main loop). Returns false if the queue is
  // full or MQTT was never configured; callers should gate on canPublish()
  // first to avoid buffering events nobody can drain.
  bool enqueueShot(const NormalizedShotData& shotData);
  bool enqueueSessionStarted(uint32_t sessionId, float startDelaySeconds);
  bool enqueueSessionStopped(uint32_t sessionId, uint16_t totalShots, uint32_t lastShotTimeMs = 0);
  bool enqueueSessionSuspended(uint32_t sessionId);
  bool enqueueSessionResumed(uint32_t sessionId);
  bool enqueueCountdownComplete(uint32_t sessionId);
  bool enqueueConnectionState(DeviceConnectionState state, const char* deviceName, const char* deviceModel);
  bool enqueueDeviceInfo(const char* deviceName, const char* deviceModel, const char* firmwareVersion);

  // Diagnostics
  inline UBaseType_t getQueueDepth() const {
    return eventQueue ? uxQueueMessagesWaiting(eventQueue) : 0;
  }
  inline uint32_t getTotalPublished() const { return totalEventsPublished; }
  inline uint32_t getPublishFailures() const { return publishFailures; }

  // Main-loop side of the up-next handoff from the MQTT task.
  bool receiveUpNext(UpNextInfo& out);

  // Main-loop side of the followed timer's events, in arrival order.
  bool receiveFollowedEvent(MqttEvent& out);

  // Settings/status
  // NOTE: touches PubSubClient directly - only safe to call from the
  // background task. Currently unused; wire it through the event queue
  // (a new MqttEventType) before calling it from anywhere else.
  void reconnect();
  const char* getMqttClientId() const;
};
