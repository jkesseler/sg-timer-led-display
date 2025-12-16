#pragma once

#include "ITimerDevice.h"
#include "DisplayManager.h"
#include "MqttManager.h"
#include "Logger.h"
#include <memory>
#include <queue>

// Application configuration
namespace AppConfig {
  constexpr uint32_t WATCHDOG_TIMEOUT_MS = 10000;  // 10 seconds
  constexpr uint32_t HEALTH_CHECK_INTERVAL_MS = 5000;  // 5 seconds
  constexpr uint16_t EVENT_QUEUE_MAX_SIZE = 30;  // Max queued events before processing
  constexpr uint32_t COEXISTENCE_BLE_WINDOW_MS = 5;  // BLE-only window after event
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

  // Event queue for radio coexistence (BLE/WiFi)
  std::queue<NormalizedShotData> shotEventQueue;
  uint16_t maxQueueDepth;  // Monitor max queue depth for diagnostics
  unsigned long lastBleWindow;  // Track BLE-only window timing

  // Device scanning state
  unsigned long lastScanAttempt;
  bool isScanning;
  unsigned long startupTime;  // Track when app started for startup delay

  // Health monitoring
  unsigned long lastHealthCheck;
  unsigned long lastActivityTime;
  bool hadDeviceConnected;  // Track if we ever had a device to distinguish "no device" from "lost device"

  // Coexistence metrics
  struct {
    uint32_t shots_queued;
    uint32_t shots_published;
    uint32_t publish_failures;
  } coexistenceMetrics;

  // MQTT availability tracking
  unsigned long lastMqttWarningTime;  // Throttle logging of MQTT unavailable state

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
  void publishQueuedEvents();  // Process event queue asynchronously

public:
  TimerApplication();
  ~TimerApplication();

  bool initialize();
  void run(); // Main application loop

  // Getters for debugging/monitoring
  bool isSessionActive() const { return sessionActive; }
  DisplayManager* getDisplayManager() const { return displayManager.get(); }
  MqttManager* getMqttManager() const { return mqttManager.get(); }

  // System health
  bool isHealthy() const;
  bool isInitialized() const;
  unsigned long getUptimeMs() const;
};