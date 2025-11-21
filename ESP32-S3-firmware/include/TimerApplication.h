#pragma once

#include "ITimerDevice.h"
#include "DisplayManager.h"
#include "Logger.h"
#include <memory>

// Application configuration
namespace AppConfig {
  constexpr uint32_t WATCHDOG_TIMEOUT_MS = 10000;  // 10 seconds
  constexpr uint32_t HEALTH_CHECK_INTERVAL_MS = 5000;  // 5 seconds
}

class TimerApplication {
private:
  std::unique_ptr<ITimerDevice> timerDevice;
  std::unique_ptr<DisplayManager> displayManager;

  // Application state
  bool sessionActive;
  uint16_t lastShotNumber;
  uint32_t lastShotTime;

  // Device scanning state
  unsigned long lastScanAttempt;
  bool isScanning;
  unsigned long startupTime;  // Track when app started for startup delay

  // Health monitoring
  unsigned long lastHealthCheck;
  unsigned long lastActivityTime;

  // Event handlers
  void onShotDetected(const NormalizedShotData& shotData);
  void onSessionStarted(const SessionData& sessionData);
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

public:
  TimerApplication();
  ~TimerApplication();

  bool initialize();
  void run(); // Main application loop

  // Getters for debugging/monitoring
  bool isSessionActive() const { return sessionActive; }
  DisplayManager* getDisplayManager() const { return displayManager.get(); }

  // System health
  bool isHealthy() const;
  bool isInitialized() const;
  unsigned long getUptimeMs() const;
};