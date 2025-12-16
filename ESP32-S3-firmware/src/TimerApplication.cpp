#include "TimerApplication.h"
#include "SGTimerDevice.h"
#include "SpecialPieTimerDevice.h"
#include "WiFiConfig.h"
#include "common.h"
#include <BLEDevice.h>

TimerApplication::TimerApplication()
  : sessionActive(false),
    lastShotNumber(0),
    lastShotTime(0),
    lastScanAttempt(0),
    isScanning(false),
    startupTime(0),
    lastHealthCheck(0),
    lastActivityTime(0),
    hadDeviceConnected(false),
    maxQueueDepth(0),
    lastBleWindow(0),
    lastMqttWarningTime(0) {
  coexistenceMetrics.shots_queued = 0;
  coexistenceMetrics.shots_published = 0;
  coexistenceMetrics.publish_failures = 0;
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

  // Initialize MQTT manager
  mqttManager = std::unique_ptr<MqttManager>(new MqttManager());
  if (!mqttManager->initialize()) {
    LOG_ERROR("SYSTEM", "Failed to initialize MQTT manager");
    // Non-fatal - continue without MQTT
  }

  // Initialize BLE
  BLEDevice::init(BLE_DEVICE_NAME);
  LOG_BLE("ESP32-S3 BLE Client initialized");

  // Note: Device will be created dynamically when found during scanning
  LOG_SYSTEM("Ready to scan for timer devices (SG Timer or Special Pie Timer)");

  LOG_SYSTEM("Application initialized successfully");
  startupTime = millis();
  lastActivityTime = millis();
  return true;
}

void TimerApplication::run() {
  // PHASE 0: WiFi Management (Non-blocking background)
  // ==================================================
  // WiFi connection is maintained in the background
  WiFiConfig::update();

  // If no device exists, scan for available devices
  // Don't scan if we have a device that's connecting/connected
  if (!timerDevice) {
    scanForDevices();
  }

  // PHASE 1: BLE Processing (CRITICAL - must not be blocked by WiFi)
  // ================================================================
  if (timerDevice) {
    timerDevice->update();
  }

  // Let BLE finish its processing/reception window
  // This prevents WiFi from interfering with BLE radio window
  lastBleWindow = millis();
  delay(AppConfig::COEXISTENCE_BLE_WINDOW_MS);

  // PHASE 2: WiFi & MQTT Operations (Safe - BLE window complete)
  // ==============================================================
  // Publish any queued events asynchronously
  publishQueuedEvents();

  if (mqttManager) {
    mqttManager->update();
  }

  // PHASE 3: Display & Monitoring
  // =============================
  if (displayManager) {
    displayManager->update();
  }

  // Perform periodic health checks
  performHealthCheck();

  // Yield to FreeRTOS scheduler, while allowing BLE and DMA tasks to run efficiently
  vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY));
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

  device->onCountdownComplete([this](const SessionData& sessionData) {
    onCountdownComplete(sessionData);
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
  logShotData(shotData);

  // CRITICAL: Queue event for async MQTT publishing
  // This prevents WiFi operations from blocking BLE reception
  if (shotEventQueue.size() < AppConfig::EVENT_QUEUE_MAX_SIZE) {
    shotEventQueue.push(shotData);
    coexistenceMetrics.shots_queued++;

    // Track max queue depth for diagnostics
    if (shotEventQueue.size() > maxQueueDepth) {
      maxQueueDepth = shotEventQueue.size();
      if (maxQueueDepth > 3) {
        LOG_WARN("COEX", "Event queue depth: %d (potential radio conflicts)", maxQueueDepth);
      }
    }
  } else {
    // Queue is full - event will be lost if not processed
    LOG_ERROR("COEX", "Event queue full! Shot %d may not be published", shotData.shotNumber);
  }

  // Update display if session is active
  // This happens immediately regardless of MQTT queue status
  if (sessionActive && displayManager) {
    displayManager->showShotData(shotData);
  }
}

void TimerApplication::onSessionStarted(const SessionData& sessionData) {
  LOG_TIMER("Session started: ID %u, Countdown: %.1fs", sessionData.sessionId, sessionData.startDelaySeconds);

  sessionActive = true;
  lastShotNumber = 0;
  lastShotTime = 0;

  // Publish to MQTT
  if (mqttManager) {
    mqttManager->publishSessionStarted(sessionData.sessionId, sessionData.startDelaySeconds);
  }

  if (displayManager) {
    // Show countdown if there's a delay, otherwise go straight to waiting
    if (sessionData.startDelaySeconds > 0.0f) {
      displayManager->showCountdown(sessionData);
    } else {
      displayManager->showWaitingForShots(sessionData);
    }
  }
}

void TimerApplication::onCountdownComplete(const SessionData& sessionData) {
  LOG_TIMER("Countdown complete - ready for shots");

  // Publish to MQTT
  if (mqttManager) {
    mqttManager->publishCountdownComplete(sessionData.sessionId);
  }

  if (displayManager) {
    displayManager->showWaitingForShots(sessionData);
  }
}

void TimerApplication::onSessionStopped(const SessionData& sessionData) {
  LOG_TIMER("Session stopped: ID %u, Total shots: %d",
            sessionData.sessionId, sessionData.totalShots);

  sessionActive = false;

  // CRITICAL: Clear any buffered shots when session ends
  // We don't want stale shot events arriving after session completion
  if (shotEventQueue.size() > 0) {
    LOG_WARN("COEX", "Clearing %d queued shots on session end", shotEventQueue.size());
    while (!shotEventQueue.empty()) {
      shotEventQueue.pop();
    }
  }

  // Publish session ended event immediately (not queued)
  // Include last shot time so PWA display shows final result
  if (mqttManager) {
    mqttManager->publishSessionStopped(sessionData.sessionId, sessionData.totalShots, lastShotTime);
  }

  if (displayManager) {
    displayManager->showSessionEnd(sessionData, lastShotNumber);
  }
}

void TimerApplication::onSessionSuspended(const SessionData& sessionData) {
  LOG_TIMER("Session suspended: ID %u, Total shots: %d",
            sessionData.sessionId, sessionData.totalShots);

  // Publish to MQTT
  if (mqttManager) {
    mqttManager->publishSessionSuspended(sessionData.sessionId);
  }

  // Keep sessionActive true for suspended sessions
}

void TimerApplication::onSessionResumed(const SessionData& sessionData) {
  LOG_TIMER("Session resumed: ID %u, Total shots: %d",
            sessionData.sessionId, sessionData.totalShots);

  sessionActive = true;

  // Publish to MQTT
  if (mqttManager) {
    mqttManager->publishSessionResumed(sessionData.sessionId);
  }

  if (displayManager) {
    displayManager->showWaitingForShots(sessionData);
  }
}

void TimerApplication::onConnectionStateChanged(DeviceConnectionState state) {
  LOG_BLE("Connection state changed: %d", (int)state);
  updateActivityTime();

  // Track successful connections and initialize WiFi on first connection
  if (state == DeviceConnectionState::CONNECTED) {
    hadDeviceConnected = true;

    // Initialize WiFi only after first BLE device connection
    if (!WiFiConfig::isInitialized()) {
      LOG_SYSTEM("BLE device connected - initializing WiFi now");
      WiFiConfig::initialize();
    }
  }

  // Publish connection state to MQTT
  const char* deviceName = nullptr;
  const char* deviceModel = nullptr;
  if (timerDevice) {
    deviceName = timerDevice->getDeviceName();
    deviceModel = timerDevice->getDeviceModel();
  }

  if (mqttManager) {
    mqttManager->publishConnectionState(state, deviceName, deviceModel);
  }

  // Update display based on connection state
  if (state == DeviceConnectionState::DISCONNECTED) {
    if (sessionActive) {
      sessionActive = false;
    }
    // Clean up device so we can scan again
    timerDevice.reset();
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

    // Only log timer error if we had a device and lost it
    // Don't log when we're just scanning for the first time
    if (!timerHealthy && hadDeviceConnected) {
      LOG_ERROR("HEALTH", "Timer device lost connection");
    }

    // Check for activity timeout (no BLE activity for too long)
    if (currentTime - lastActivityTime > AppConfig::WATCHDOG_TIMEOUT_MS) {
      LOG_WARN("HEALTH", "No BLE activity for %lu ms", currentTime - lastActivityTime);
    }

    // Log radio coexistence metrics
    if (shotEventQueue.size() > 0) {
      LOG_DEBUG("COEX", "Queue depth: %d, Published: %lu, Failures: %lu, Max depth: %u",
                shotEventQueue.size(),
                coexistenceMetrics.shots_published,
                coexistenceMetrics.publish_failures,
                maxQueueDepth);
    }

    LOG_DEBUG("HEALTH", "System uptime: %lu ms, Free heap: %u bytes, Queue: %d events",
              getUptimeMs(), ESP.getFreeHeap(), shotEventQueue.size());

    lastHealthCheck = currentTime;
  }
}

void TimerApplication::updateActivityTime() {
  lastActivityTime = millis();
}

void TimerApplication::publishQueuedEvents() {
  // Process queued shot events asynchronously
  // This runs in the main loop, outside the critical BLE window

  if (shotEventQueue.empty()) {
    return;
  }

  bool mqttHealthy = mqttManager && mqttManager->isHealthy();

  // If queue is getting too full (>75% capacity), drain it regardless of MQTT status
  // This prevents the queue from locking up when MQTT is temporarily unavailable
  bool queueCritical = shotEventQueue.size() > (AppConfig::EVENT_QUEUE_MAX_SIZE * 3 / 4);

  if (!mqttHealthy && !queueCritical) {
    // MQTT unavailable and queue not critical - keep events buffered
    // Throttle logging to avoid spam (only log every 2 seconds)
    unsigned long now = millis();
    if (now - lastMqttWarningTime >= 2000) {
      LOG_WARN("COEX", "Event queue: %d waiting (MQTT unavailable, buffering)",
               shotEventQueue.size());
      lastMqttWarningTime = now;
    }
    return;
  }

  // Publish all queued events
  // Process aggressively to prevent queue backup
  unsigned int processed = 0;
  while (!shotEventQueue.empty()) {
    // If MQTT becomes unavailable during processing, stop
    if (!mqttHealthy && processed > 0) {
      break;
    }

    NormalizedShotData shot = shotEventQueue.front();

    // Attempt to publish the event
    if (mqttManager) {
      mqttManager->publishShotDetected(shot);
      LOG_DEBUG("COEX", "Published queued shot #%d", shot.shotNumber);
    } else {
      // No MQTT manager - discard old events if queue too full
      if (queueCritical) {
        LOG_DEBUG("COEX", "Discarding queued shot #%d (MQTT unavailable, queue full)", shot.shotNumber);
      }
    }

    shotEventQueue.pop();
    coexistenceMetrics.shots_published++;
    processed++;
  }

  // Log if we had to process multiple events (indicates congestion)
  if (processed > 3) {
    LOG_INFO("COEX", "Processed %d queued shots", processed);
  }
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

  // Don't scan during startup message display period
  if (now - startupTime < STARTUP_MESSAGE_DELAY) {
    return;
  }

  // Throttle scan attempts
  if (isScanning || (now - lastScanAttempt < BLE_SCAN_RETRY_INTERVAL_MS)) {
    return;
  }

  lastScanAttempt = now;
  isScanning = true;

  if (displayManager) {
    displayManager->showConnectionState(DeviceConnectionState::SCANNING, nullptr);
  }

  LOG_SYSTEM("Scanning for timer devices...");

  BLEScan* pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(BLE_SCAN_INTERVAL);
  pScan->setWindow(BLE_SCAN_WINDOW);

  // Perform BLE scan - results contain copies of advertised device data
  BLEScanResults foundDevices = pScan->start(BLE_SCAN_DURATION, false);

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
        // Device will update state via callbacks (CONNECTING -> CONNECTED)
        if (sgDevice->attemptConnection(&device)) {
          LOG_SYSTEM("Successfully connected to SG Timer");
          deviceFound = true;
          break;
        } else {
          LOG_ERROR("TIMER", "Failed to connect to SG Timer");
          timerDevice.reset();
        }
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
        // Device will update state via callbacks (CONNECTING -> CONNECTED)
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

  // BLEScanResults contains copies of advertised device data
  // clearResults() properly frees the internal storage after device loop completes
  pScan->clearResults();

  if (!deviceFound) {
    LOG_SYSTEM("No compatible timer devices found. Retrying...");
  }

  isScanning = false;
}