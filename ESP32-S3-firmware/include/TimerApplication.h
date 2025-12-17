#pragma once

#include "ITimerDevice.h"
#include "DisplayManager.h"
#include "MqttManager.h"
#include "Logger.h"
#include <memory>

// Application configuration
namespace AppConfig {
  constexpr uint32_t WATCHDOG_TIMEOUT_MS = 10000;  // 10 seconds
  constexpr uint32_t HEALTH_CHECK_INTERVAL_MS = 5000;  // 5 seconds

  // Ring buffer configuration for shot events
  // Using power-of-2 size enables fast modulo via bitwise AND
  constexpr uint16_t EVENT_QUEUE_SIZE = 32;  // Must be power of 2
  constexpr uint16_t EVENT_QUEUE_MASK = EVENT_QUEUE_SIZE - 1;
  constexpr uint16_t QUEUE_DEPTH_WARN_THRESHOLD = EVENT_QUEUE_SIZE / 8;  // Warn at ~12.5% capacity

  // Batch processing configuration
  constexpr uint16_t MAX_SHOTS_PER_PUBLISH_CYCLE = 8;  // Max shots to publish per loop iteration
}

class TimerApplication {
private:
  std::unique_ptr<ITimerDevice> timerDevice;
  std::unique_ptr<DisplayManager> displayManager;
  std::unique_ptr<MqttManager> mqttManager;

  // Application state
  bool sessionActive;
  uint16_t lastShotNumber;
  uint32_t lastShotTime;

  // Lock-free ring buffer for shot events (faster than std::queue)
  // Allows BLE callback to enqueue without blocking MQTT publishing
  NormalizedShotData shotEventBuffer[AppConfig::EVENT_QUEUE_SIZE];
  volatile uint16_t queueHead;  // Write position (BLE callback writes here)
  volatile uint16_t queueTail;  // Read position (main loop reads here)

  // Diagnostics
  uint16_t maxQueueDepth;
  uint32_t totalShotsQueued;
  uint32_t totalShotsPublished;
  uint32_t publishFailures;

  // Device scanning state
  unsigned long lastScanAttempt;
  bool isScanning;
  unsigned long startupTime;

  // Health monitoring
  unsigned long lastHealthCheck;
  unsigned long lastActivityTime;
  bool hadDeviceConnected;

  // MQTT warning throttle
  unsigned long lastMqttWarningTime;

  // Ring buffer helpers - inline for performance
  inline uint16_t queueSize() const {
    return (queueHead - queueTail) & AppConfig::EVENT_QUEUE_MASK;
  }

  inline bool queueEmpty() const {
    return queueHead == queueTail;
  }

  inline bool queueFull() const {
    return queueSize() == (AppConfig::EVENT_QUEUE_SIZE - 1);
  }

  // Event handlers
  void onShotDetected(const NormalizedShotData& shotData);
  void onSessionStarted(const SessionData& sessionData);
  void onCountdownComplete(const SessionData& sessionData);
  void onSessionStopped(const SessionData& sessionData);
  void onSessionSuspended(const SessionData& sessionData);
  void onSessionResumed(const SessionData& sessionData);
  void onConnectionStateChanged(DeviceConnectionState state);

  // Helper methods
  void setupCallbacks();
  void logShotData(const NormalizedShotData& shotData);
  void performHealthCheck();
  void updateActivityTime();
  void scanForDevices();
  void publishQueuedEvents();

public:
  TimerApplication();
  ~TimerApplication();

  bool initialize();
  void run();

  // Getters for debugging/monitoring
  bool isSessionActive() const { return sessionActive; }
  DisplayManager* getDisplayManager() const { return displayManager.get(); }
  MqttManager* getMqttManager() const { return mqttManager.get(); }

  // System health
  bool isHealthy() const;
  bool isInitialized() const;
  unsigned long getUptimeMs() const;
};
