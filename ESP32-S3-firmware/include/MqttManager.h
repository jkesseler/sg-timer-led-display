#pragma once

#include "ITimerDevice.h"
#include "Logger.h"
#include <memory>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Events that can reach the MQTT broker. Every event is enqueued from
// whichever task detects it (the BLE notification task or the main loop)
// and is only ever dispatched by MqttManager's own background task -
// PubSubClient is not thread-safe, so nothing outside that task may call
// into it directly.
enum class MqttEventType : uint8_t {
  SHOT_DETECTED,
  SESSION_STARTED,
  SESSION_STOPPED,
  SESSION_SUSPENDED,
  SESSION_RESUMED,
  COUNTDOWN_COMPLETE,
  CONNECTION_STATE,
  DEVICE_INFO,
};

struct MqttEvent {
  MqttEventType type;

  union {
    NormalizedShotData shot;

    struct {
      uint32_t sessionId;
      float startDelaySeconds;
    } sessionStarted;

    struct {
      uint32_t sessionId;
      uint16_t totalShots;
      uint32_t lastShotTimeMs;
    } sessionStopped;

    // Reused for SESSION_SUSPENDED / SESSION_RESUMED / COUNTDOWN_COMPLETE,
    // which all carry only a session ID.
    struct {
      uint32_t sessionId;
    } sessionSimple;

    struct {
      DeviceConnectionState state;
      bool hasDeviceName;
      bool hasDeviceModel;
      char deviceName[64];
      char deviceModel[32];
    } connectionState;

    struct {
      bool hasDeviceName;
      bool hasDeviceModel;
      bool hasFirmwareVersion;
      char deviceName[64];
      char deviceModel[32];
      char firmwareVersion[16];
    } deviceInfo;
  };

  MqttEvent() : type(MqttEventType::SHOT_DETECTED) {}
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
  uint32_t totalEventsPublished;
  uint32_t publishFailures;

  static constexpr uint16_t EVENT_QUEUE_SIZE = 32;
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

  // Helper methods - uses pre-allocated buffer
  // retain=true → broker stores the last value for late-joining subscribers
  bool publishJson(const char* topic, const char* jsonPayload, bool retain = false);

  // Actual publishers - touch PubSubClient/jsonBuffer directly, so these may
  // only run on the background task. dispatchEvent() is their sole caller.
  void doPublishConnectionState(DeviceConnectionState state, const char* deviceName, const char* deviceModel);
  void doPublishDeviceInfo(const char* deviceName, const char* deviceModel, const char* firmwareVersion);
  void doPublishSessionStarted(uint32_t sessionId, float startDelaySeconds);
  void doPublishSessionStopped(uint32_t sessionId, uint16_t totalShots, uint32_t lastShotTimeMs);
  void doPublishSessionSuspended(uint32_t sessionId);
  void doPublishSessionResumed(uint32_t sessionId);
  void doPublishCountdownComplete(uint32_t sessionId);
  bool doPublishShotDetected(const NormalizedShotData& shotData);

  // Background task - the only code allowed to call the doPublish*() methods
  void startTask();
  static void taskEntry(void* param);
  void taskLoop();
  void drainEventQueue();
  void dispatchEvent(const MqttEvent& event);

public:
  MqttManager();
  ~MqttManager();

  // Lifecycle. On success, creates the event queue and starts the
  // background MQTT task; nothing else needs to be called from the main loop.
  bool initialize();

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

  // Settings/status
  // NOTE: touches PubSubClient directly - only safe to call from the
  // background task. Currently unused; wire it through the event queue
  // (a new MqttEventType) before calling it from anywhere else.
  void reconnect();
  const char* getMqttClientId() const;
};
