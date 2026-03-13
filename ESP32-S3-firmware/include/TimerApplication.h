#pragma once

#include "ITimerDevice.h"
#include "DisplayManager.h"
#include "MqttManager.h"
#include "Logger.h"
#include <memory>
#include "freertos/queue.h"

// Application configuration
namespace AppConfig {
  constexpr uint32_t WATCHDOG_TIMEOUT_MS = 10000;  // 10 seconds
  constexpr uint32_t HEALTH_CHECK_INTERVAL_MS = 5000;  // 5 seconds

  // FreeRTOS queue configuration for shot events
  constexpr uint16_t EVENT_QUEUE_SIZE = 32;
  constexpr uint16_t QUEUE_DEPTH_WARN_THRESHOLD = EVENT_QUEUE_SIZE / 4;  // Warn at ~25% capacity

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

  // FreeRTOS queue for shot events (written by BLE callback, read by main loop)
  QueueHandle_t shotEventQueue;

  // Diagnostics
  uint16_t maxQueueDepth;
  uint32_t totalShotsQueued;
  uint32_t totalShotsPublished;
  uint32_t publishFailures;

  // Device scanning state
  unsigned long lastScanAttempt;
  bool isScanning;
  bool scanResultsReady;
  unsigned long startupTime;

  // Health monitoring
  unsigned long lastHealthCheck;
  unsigned long lastActivityTime;
  bool hadDeviceConnected;

  // MQTT warning throttle
  unsigned long lastMqttWarningTime;

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
  void processScanResults();
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
  bool isRuntimeReady() const;
  unsigned long getUptimeMs() const;
};
