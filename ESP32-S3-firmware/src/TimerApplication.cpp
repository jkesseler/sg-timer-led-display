#include "TimerApplication.h"
#include "SGTimerDevice.h"
#include "SpecialPieTimerDevice.h"
#include "common.h"
#include <BLEDevice.h>

TimerApplication::TimerApplication()
  : sessionActive(false),
    lastShotNumber(0),
    lastShotTime(0),
    lastScanAttempt(0),
    isScanning(false),
    lastHealthCheck(0),
    lastActivityTime(0) {
}

TimerApplication::~TimerApplication() {
  // Smart pointers will handle cleanup automatically
}

bool TimerApplication::initialize() {
  LOG_SYSTEM("=== SG Shot Timer BLE Bridge ===");
  LOG_SYSTEM("ESP32-S3 DevKit-C Starting...");

  // Initialize display manager
  displayManager = std::unique_ptr<DisplayManager>(new DisplayManager());
  if (!displayManager->initialize()) {
    LOG_ERROR("SYSTEM", "Failed to initialize display manager");
    return false;
  }


  // Initialize BLE
  BLEDevice::init(BLE_DEVICE_NAME);
  LOG_BLE("ESP32-S3 BLE Client initialized");

  // Note: Device will be created dynamically when found during scanning
  LOG_SYSTEM("Ready to scan for timer devices (SG Timer or Special Pie Timer)");

  LOG_SYSTEM("Application initialized successfully");
  lastActivityTime = millis();
  return true;
}

void TimerApplication::run() {
  // If no device is connected, scan for available devices
  if (!timerDevice || !timerDevice->isConnected()) {
    scanForDevices();
  }

  // Update all components
  if (timerDevice) {
    timerDevice->update();
  }

  if (displayManager) {
    displayManager->update();
  }

  // Perform periodic health checks
  performHealthCheck();

  // delay(MAIN_LOOP_DELAY);
}

void TimerApplication::setupCallbacks() {
  ITimerDevice* device = timerDevice.get();

  if (!device) return;

  device->onShotDetected([this](const NormalizedShotData& shotData) {
    onShotDetected(shotData);
  });

  device->onSessionStarted([this](const SessionData& sessionData) {
    onSessionStarted(sessionData);
  });

  device->onSessionStopped([this](const SessionData& sessionData) {
    onSessionStopped(sessionData);
  });

  device->onSessionSuspended([this](const SessionData& sessionData) {
    onSessionSuspended(sessionData);
  });

  device->onSessionResumed([this](const SessionData& sessionData) {
    onSessionResumed(sessionData);
  });

  device->onConnectionStateChanged([this](DeviceConnectionState state) {
    onConnectionStateChanged(state);
  });
}

void TimerApplication::onShotDetected(const NormalizedShotData& shotData) {
  // Update application state
  lastShotNumber = shotData.shotNumber;
  lastShotTime = shotData.absoluteTimeMs;
  updateActivityTime();

  // Update display if session is active
  if (sessionActive && displayManager) {
    displayManager->showShotData(shotData);
  }

  logShotData(shotData);
}

void TimerApplication::onSessionStarted(const SessionData& sessionData) {
  LOG_TIMER("Session started: ID %u", sessionData.sessionId);

  sessionActive = true;
  lastShotNumber = 0;
  lastShotTime = 0;

  if (displayManager) {
    displayManager->showWaitingForShots(sessionData);
  }
}

void TimerApplication::onSessionStopped(const SessionData& sessionData) {
  LOG_TIMER("Session stopped: ID %u, Total shots: %d",
            sessionData.sessionId, sessionData.totalShots);

  sessionActive = false;

  if (displayManager) {
    displayManager->showSessionEnd(sessionData, lastShotNumber);
  }
}

void TimerApplication::onSessionSuspended(const SessionData& sessionData) {
  LOG_TIMER("Session suspended: ID %u, Total shots: %d",
            sessionData.sessionId, sessionData.totalShots);
  // Keep sessionActive true for suspended sessions
}

void TimerApplication::onSessionResumed(const SessionData& sessionData) {
  LOG_TIMER("Session resumed: ID %u, Total shots: %d",
            sessionData.sessionId, sessionData.totalShots);

  sessionActive = true;

  if (displayManager) {
    displayManager->showWaitingForShots(sessionData);
  }
}

void TimerApplication::onConnectionStateChanged(DeviceConnectionState state) {
  LOG_BLE("Connection state changed: %d", (int)state);
  updateActivityTime();

  // Update display based on connection state
  if (state == DeviceConnectionState::DISCONNECTED && sessionActive) {
    sessionActive = false;
  }

  const char* deviceName = nullptr;
  if (timerDevice) {
    deviceName = timerDevice->getDeviceName();
  }

  if (displayManager) {
    displayManager->showConnectionState(state, deviceName);
  }
}

void TimerApplication::logShotData(const NormalizedShotData& shotData) {
  LOG_TIMER("Shot detected: #%d, Time: %.3fs, Split: %.3fs",
            shotData.shotNumber,
            shotData.absoluteTimeMs / 1000.0,
            shotData.splitTimeMs / 1000.0);
}

void TimerApplication::performHealthCheck() {
  unsigned long currentTime = millis();

  if (currentTime - lastHealthCheck >= AppConfig::HEALTH_CHECK_INTERVAL_MS) {
    // Check component health
    bool displayHealthy = displayManager && displayManager->isInitialized();
    bool timerHealthy = timerDevice != nullptr;

    if (!displayHealthy) {
      LOG_ERROR("HEALTH", "Display manager is not healthy");
    }

    if (!timerHealthy) {
      LOG_ERROR("HEALTH", "Timer device is not healthy");
    }

    // Check for activity timeout (no BLE activity for too long)
    if (currentTime - lastActivityTime > AppConfig::WATCHDOG_TIMEOUT_MS) {
      LOG_WARN("HEALTH", "No BLE activity for %lu ms", currentTime - lastActivityTime);
    }

    LOG_DEBUG("HEALTH", "System uptime: %lu ms, Free heap: %u bytes",
              getUptimeMs(), ESP.getFreeHeap());

    lastHealthCheck = currentTime;
  }
}

void TimerApplication::updateActivityTime() {
  lastActivityTime = millis();
}

bool TimerApplication::isHealthy() const {
  bool displayHealthy = displayManager && displayManager->isInitialized();
  bool timerHealthy = timerDevice != nullptr;
  bool activityHealthy = (millis() - lastActivityTime) < AppConfig::WATCHDOG_TIMEOUT_MS;

  return displayHealthy && timerHealthy && activityHealthy;
}

unsigned long TimerApplication::getUptimeMs() const {
  return millis();
}

bool TimerApplication::isInitialized() const {
  bool displayHealthy = displayManager && displayManager->isInitialized();
  bool timerHealthy = timerDevice != nullptr;

  return displayHealthy && timerHealthy;
}


void TimerApplication::scanForDevices() {
  unsigned long now = millis();

  // Throttle scan attempts
  if (isScanning || (now - lastScanAttempt < 5000)) {
    return;
  }

  lastScanAttempt = now;
  isScanning = true;

  LOG_SYSTEM("Scanning for timer devices...");

  BLEScan* pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);

  BLEScanResults foundDevices = pScan->start(10, false);

  // Check for SG Timer
  BLEUUID sgServiceUuid(SGTimerDevice::SERVICE_UUID);
  // Check for Special Pie Timer
  BLEUUID specialPieServiceUuid(SpecialPieTimerDevice::SERVICE_UUID);

  bool deviceFound = false;

  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);

    // Check for SG Timer (by address)
    if (device.haveServiceUUID() && device.isAdvertisingService(sgServiceUuid))
    {
      LOG_SYSTEM("SG Timer found! Connecting...");

      // Create SG Timer device
      SGTimerDevice* sgDevice = new SGTimerDevice();
      timerDevice = std::unique_ptr<ITimerDevice>(sgDevice);
      setupCallbacks();

      if (timerDevice->initialize()) {
        // Attempt connection using SG Timer's connection logic
        sgDevice->attemptConnection();
        // Connection status will be checked in next update cycle
        deviceFound = true;
        break;
      } else {
        LOG_ERROR("TIMER", "Failed to initialize SG Timer");
        timerDevice.reset();
      }
    }
    // Check for Special Pie Timer (by service UUID)
    else if (device.haveServiceUUID() && device.isAdvertisingService(specialPieServiceUuid)) {
      LOG_SYSTEM("Special Pie Timer found! Connecting...");

      // Create Special Pie Timer device
      SpecialPieTimerDevice* specialPieDevice = new SpecialPieTimerDevice();
      timerDevice = std::unique_ptr<ITimerDevice>(specialPieDevice);
      setupCallbacks();

      if (timerDevice->initialize()) {
        // Attempt connection
        if (specialPieDevice->attemptConnection(&device)) {
          LOG_SYSTEM("Successfully connected to Special Pie Timer");
          deviceFound = true;
          break;
        } else {
          LOG_ERROR("TIMER", "Failed to connect to Special Pie Timer");
          timerDevice.reset();
        }
      } else {
        LOG_ERROR("TIMER", "Failed to initialize Special Pie Timer");
        timerDevice.reset();
      }
    }
  }

  pScan->clearResults();

  if (!deviceFound) {
    LOG_SYSTEM("No compatible timer devices found. Retrying...");
  }

  isScanning = false;
}