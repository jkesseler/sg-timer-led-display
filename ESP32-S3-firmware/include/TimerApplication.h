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

  // Warn when MqttManager's event queue crosses this depth - see
  // MqttManager::EVENT_QUEUE_SIZE, which owns the queue itself.
  constexpr uint16_t QUEUE_DEPTH_WARN_THRESHOLD = 8;
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

  // Diagnostics for events handed to MqttManager's queue (the queue itself,
  // and the task that drains it, live in MqttManager - see its header).
  uint16_t maxQueueDepth;
  uint32_t totalShotsQueued;
  uint32_t shotEnqueueFailures;

  // Device scanning state
  unsigned long lastScanAttempt;
  bool isScanning;
  bool scanResultsReady;
  unsigned long startupTime;

  // Set when a disconnect is detected; the device is torn down from the main
  // loop (never from inside the device's own callback) to avoid deleting an
  // object whose method is still executing on the call stack.
  bool deviceResetPending;

  // Health monitoring
  unsigned long lastHealthCheck;
  unsigned long lastActivityTime;
  bool hadDeviceConnected;

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
