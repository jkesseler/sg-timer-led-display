#include "TimerApplication.h"
#include "SGTimerDevice.h"
#include "SpecialPieTimerDevice.h"
#include "SpecialPieMacTimerDevice.h"
#include "ASNTrackerDevice.h"
#include "WiFiConfig.h"
#include "common.h"
#include <BLEDevice.h>

TimerApplication::TimerApplication()
  : sessionActive(false),
    lastShotNumber(0),
    lastShotTime(0),
    shotEventQueue(nullptr),
    maxQueueDepth(0),
    totalShotsQueued(0),
    totalShotsPublished(0),
    publishFailures(0),
    lastScanAttempt(0),
    isScanning(false),
    startupTime(0),
    lastHealthCheck(0),
    lastActivityTime(0),
    hadDeviceConnected(false),
    lastMqttWarningTime(0) {
}

TimerApplication::~TimerApplication() {
  if (shotEventQueue) {
    vQueueDelete(shotEventQueue);
    shotEventQueue = nullptr;
  }
  // Smart pointers will handle cleanup automatically
}

bool TimerApplication::initialize() {
  LOG_SYSTEM("=== SG Shot Timer BLE Bridge ===");
  LOG_SYSTEM("ESP32-S3 DevKit-C Starting...");

  // Create FreeRTOS queue for shot events
  shotEventQueue = xQueueCreate(AppConfig::EVENT_QUEUE_SIZE, sizeof(NormalizedShotData));
  if (!shotEventQueue) {
    LOG_ERROR("SYSTEM", "Failed to create shot event queue");
    return false;
  }

  // Initialize display manager
  displayManager = std::unique_ptr<DisplayManager>(new DisplayManager());
  if (!displayManager->initialize()) {
    LOG_ERROR("SYSTEM", "Failed to initialize display manager");
    return false;
  }

  // Initialize MQTT manager in BLE mode.
  // Runtime MQTT enable/disable is controlled by WiFiConfig::getMqttServer().
  if (TIMER_TYPE == TIMER_TYPE_BLE) {
    // Initialize MQTT manager
    mqttManager = std::unique_ptr<MqttManager>(new MqttManager());
    if (!mqttManager->initialize()) {
      LOG_SYSTEM("MQTT disabled - server not configured");
      // Non-fatal - continue without MQTT
    }
  } else {
    LOG_SYSTEM("MQTT disabled (TIMER_TYPE=%d)", TIMER_TYPE);
  }

  // Initialize BLE only if timer type is BLE
  if (TIMER_TYPE == TIMER_TYPE_BLE) {
    BLEDevice::init(BLE_DEVICE_NAME);
    LOG_BLE("ESP32-S3 BLE Client initialized");
    LOG_SYSTEM("Ready to scan for timer devices (SG Timer or Special Pie Timer)");
  } else {
    // MQTT client not implemented yet
    LOG_SYSTEM("BLE disabled - Timer Type: MQTT");
  }

  LOG_SYSTEM("Application initialized successfully");

  startupTime = millis();
  lastActivityTime = millis();
  return true;
}

void TimerApplication::run() {
  // ============================================================
  // PHASE 1: WiFi Background Management (Non-blocking)
  // ============================================================
  WiFiConfig::update();

  // ============================================================
  // PHASE 2: BLE Device Management (only if TIMER_TYPE == TIMER_TYPE_BLE)
  // ============================================================
  if (TIMER_TYPE == TIMER_TYPE_BLE) {
    if (!timerDevice) {
      scanForDevices();
    }

    // Process BLE events - this may trigger callbacks that enqueue shots
    if (timerDevice) {
      timerDevice->update();
    }
  }

  // ============================================================
  // PHASE 3: MQTT Queue Processing (Batch publish)
  // ============================================================
  // Process queued events AFTER BLE update to minimize latency
  publishQueuedEvents();

  // MQTT connection maintenance
  if (mqttManager) {
    mqttManager->update();
  }

  // ============================================================
  // PHASE 4: Display Update
  // ============================================================
  if (displayManager) {
    displayManager->update();
  }

  // ============================================================
  // PHASE 5: Health Monitoring
  // ============================================================
  performHealthCheck();

  // Yield to FreeRTOS scheduler
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

  // ============================================================
  // CRITICAL: Enqueue for async MQTT publishing using ring buffer
  // This is called from BLE callback context - must be fast!
  // Only queue if MQTT is available - don't buffer when unavailable
  // ============================================================
  // Only publish to MQTT if TIMER_TYPE == TIMER_TYPE_BLE and connected to MQTT broker
  bool shouldPublishMqtt = (TIMER_TYPE == TIMER_TYPE_BLE) && mqttManager && mqttManager->canPublish();

  if (shouldPublishMqtt) {
    if (xQueueSend(shotEventQueue, &shotData, 0) == pdTRUE) {
      totalShotsQueued++;

      // Track max queue depth for diagnostics
      uint16_t depth = (uint16_t)uxQueueMessagesWaiting(shotEventQueue);
      if (depth > maxQueueDepth) {
        maxQueueDepth = depth;
        if (maxQueueDepth > AppConfig::QUEUE_DEPTH_WARN_THRESHOLD) {
          LOG_WARN("QUEUE", "Peak queue depth: %u/%u", maxQueueDepth, AppConfig::EVENT_QUEUE_SIZE);
        }
      }
    } else {
      // Queue full - this indicates MQTT can't keep up
      publishFailures++;
      LOG_ERROR("QUEUE", "Buffer full! Shot #%u dropped (failures: %lu)",
                shotData.shotNumber, (unsigned long)publishFailures);
    }
  }

  // Update display immediately (regardless of MQTT queue)
  if (sessionActive && displayManager) {
    displayManager->showShotData(shotData);
  }
}

void TimerApplication::onSessionStarted(const SessionData& sessionData) {
  LOG_TIMER("Session started: ID %u, Countdown: %.1fs",
            sessionData.sessionId, sessionData.startDelaySeconds);

  sessionActive = true;
  lastShotNumber = 0;
  lastShotTime = 0;

  // Publish directly (session events are infrequent)
  if (mqttManager && mqttManager->canPublish()) {
    mqttManager->publishSessionStarted(sessionData.sessionId, sessionData.startDelaySeconds);
  }

  if (displayManager) {
    if (sessionData.startDelaySeconds > 0.0f) {
      displayManager->showCountdown(sessionData);
    } else {
      displayManager->showWaitingForShots(sessionData);
    }
  }
}

void TimerApplication::onCountdownComplete(const SessionData& sessionData) {
  LOG_TIMER("Countdown complete - ready for shots");

  if (mqttManager && mqttManager->canPublish()) {
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

  // Clear any remaining queued shots on session end
  // Stale shots arriving after session end should be discarded
  UBaseType_t remaining = uxQueueMessagesWaiting(shotEventQueue);
  if (remaining > 0) {
    LOG_WARN("QUEUE", "Discarding %u queued shots on session end", (unsigned)remaining);
    xQueueReset(shotEventQueue);
  }

  // Publish session ended directly (not queued)
  if (mqttManager && mqttManager->canPublish()) {
    mqttManager->publishSessionStopped(sessionData.sessionId, sessionData.totalShots, lastShotTime);
  }

  if (displayManager) {
    displayManager->showSessionEnd(sessionData, lastShotNumber);
  }
}

void TimerApplication::onSessionSuspended(const SessionData& sessionData) {
  LOG_TIMER("Session suspended: ID %u, Total shots: %d",
            sessionData.sessionId, sessionData.totalShots);

  if (mqttManager && mqttManager->canPublish()) {
    mqttManager->publishSessionSuspended(sessionData.sessionId);
  }
}

void TimerApplication::onSessionResumed(const SessionData& sessionData) {
  LOG_TIMER("Session resumed: ID %u, Total shots: %d",
            sessionData.sessionId, sessionData.totalShots);

  sessionActive = true;

  if (mqttManager && mqttManager->canPublish()) {
    mqttManager->publishSessionResumed(sessionData.sessionId);
  }

  if (displayManager) {
    displayManager->showWaitingForShots(sessionData);
  }
}

void TimerApplication::onConnectionStateChanged(DeviceConnectionState state) {
  LOG_BLE("Connection state changed: %d", (int)state);
  updateActivityTime();

  if (state == DeviceConnectionState::CONNECTED) {
    hadDeviceConnected = true;

    // Initialize WiFi only after first BLE device connection
    if (!WiFiConfig::isInitialized()) {
      LOG_SYSTEM("BLE device connected - initializing WiFi now");
      WiFiConfig::initialize();
    }
  }

  // Get device info for MQTT publish
  const char* deviceName = nullptr;
  const char* deviceModel = nullptr;
  if (timerDevice) {
    deviceName = timerDevice->getDeviceName();
    deviceModel = timerDevice->getDeviceModel();
  }

  if (mqttManager && mqttManager->canPublish()) {
    mqttManager->publishConnectionState(state, deviceName, deviceModel);
  }

  // Handle disconnection
  if (state == DeviceConnectionState::DISCONNECTED) {
    if (sessionActive) {
      sessionActive = false;
    }
    timerDevice.reset();  // Clean up so we can scan again
  }

  if (displayManager) {
    displayManager->showConnectionState(state, deviceName);
  }
}

void TimerApplication::logShotData(const NormalizedShotData& shotData) {
  LOG_TIMER("Shot #%d: %.3fs (split: %.3fs)",
            shotData.shotNumber,
            shotData.absoluteTimeMs / 1000.0,
            shotData.splitTimeMs / 1000.0);
}

void TimerApplication::performHealthCheck() {
  unsigned long currentTime = millis();

  if (currentTime - lastHealthCheck < AppConfig::HEALTH_CHECK_INTERVAL_MS) {
    return;
  }
  lastHealthCheck = currentTime;

  // Check component health
  bool displayHealthy = displayManager && displayManager->isInitialized();
  bool timerHealthy = timerDevice != nullptr;

  if (!displayHealthy) {
    LOG_ERROR("HEALTH", "Display manager not healthy");
  }

  if (!timerHealthy && hadDeviceConnected) {
    LOG_ERROR("HEALTH", "Timer device lost connection");
  }

  // Activity timeout warning
  if (currentTime - lastActivityTime > AppConfig::WATCHDOG_TIMEOUT_MS) {
    LOG_WARN("HEALTH", "No BLE activity for %lu ms", currentTime - lastActivityTime);
  }

  // Queue metrics (only if there's been activity)
  if (totalShotsQueued > 0) {
    LOG_DEBUG("HEALTH", "Queue: %u/%u, Published: %lu/%lu, Failures: %lu, Peak: %u",
              (unsigned)uxQueueMessagesWaiting(shotEventQueue), AppConfig::EVENT_QUEUE_SIZE,
              (unsigned long)totalShotsPublished,
              (unsigned long)totalShotsQueued,
              (unsigned long)publishFailures,
              maxQueueDepth);
  }

  LOG_DEBUG("HEALTH", "Uptime: %lu ms, Free heap: %u bytes",
            getUptimeMs(), ESP.getFreeHeap());
}

void TimerApplication::updateActivityTime() {
  lastActivityTime = millis();
}

void TimerApplication::publishQueuedEvents() {
  // Fast path - nothing to publish
  if (!shotEventQueue || uxQueueMessagesWaiting(shotEventQueue) == 0) {
    return;
  }

  // Check MQTT availability
  bool mqttReady = mqttManager && mqttManager->canPublish();

  if (!mqttReady) {
    // MQTT not available - discard buffered events (don't queue when offline)
    UBaseType_t discarded = uxQueueMessagesWaiting(shotEventQueue);
    if (discarded > 0) {
      xQueueReset(shotEventQueue);
      LOG_DEBUG("QUEUE", "MQTT unavailable - discarded %u queued shots", (unsigned)discarded);
    }
    return;
  }

  // ============================================================
  // BATCH PUBLISH: Process multiple shots per loop iteration
  // This is the key optimization for fast BLE events
  // ============================================================
  uint16_t processed = 0;
  NormalizedShotData shot;
  while (xQueueReceive(shotEventQueue, &shot, 0) == pdTRUE &&
         processed < AppConfig::MAX_SHOTS_PER_PUBLISH_CYCLE) {
    // Attempt to publish
    if (mqttManager->publishShotDetected(shot)) {
      totalShotsPublished++;
      LOG_DEBUG("QUEUE", "Published shot #%u", shot.shotNumber);
    } else {
      // Publish failed - MQTT might have disconnected
      publishFailures++;
      LOG_WARN("QUEUE", "Failed to publish shot #%u", shot.shotNumber);
      // Stop processing this cycle - let MQTT reconnect
      break;
    }

    processed++;
  }

  // Log batch processing stats
  if (processed > 1) {
    LOG_DEBUG("QUEUE", "Batch published %u shots", processed);
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

bool TimerApplication::isRuntimeReady() const {
  bool displayHealthy = displayManager && displayManager->isInitialized();
  bool timerHealthy = timerDevice != nullptr;

  return displayHealthy && timerHealthy;
}

void TimerApplication::scanForDevices() {
  unsigned long now = millis();

  // Don't scan during startup message display
  if (now - startupTime < STARTUP_MESSAGE_DELAY) {
    return;
  }

  // Throttle scan attempts - wait 5 seconds between full scan cycles
  if (isScanning || (now - lastScanAttempt < 5000)) {
    return;
  }

  lastScanAttempt = now;
  isScanning = true;

  if (displayManager) {
    displayManager->showConnectionState(DeviceConnectionState::SCANNING, nullptr);
  }

  LOG_SYSTEM("Scanning for compatible timer devices...");

  // Unified BLE scan for all device types (MAC-based and UUID-based)
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(BLE_SCAN_INTERVAL);
  pScan->setWindow(BLE_SCAN_WINDOW);

  BLEScanResults foundDevices = pScan->start(BLE_SCAN_DURATION, false);
  LOG_SYSTEM("Scan complete - found %d devices", foundDevices.getCount());

  bool deviceFound = false;

  // Check each discovered device against all known device types
  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);

    // Try MAC-based Special Pie Timer first (highest priority)
    if (SpecialPieMacTimerDevice::matchesDevice(&device)) {
      LOG_SYSTEM("Found MAC-based Special Pie Timer (MAC: %s)",
                 device.getAddress().toString().c_str());

      SpecialPieMacTimerDevice* macDevice = new SpecialPieMacTimerDevice();
      timerDevice = std::unique_ptr<ITimerDevice>(macDevice);
      setupCallbacks();

      if (timerDevice->initialize() && macDevice->attemptConnection(&device)) {
        LOG_SYSTEM("Successfully connected to Special Pie Timer (MAC-based)");
        deviceFound = true;
        break;
      } else {
        LOG_ERROR("TIMER", "Failed to connect to MAC-based Special Pie Timer");
        timerDevice.reset();
      }
    }
    // Try SG Timer
    else if (SGTimerDevice::matchesDevice(&device)) {
      LOG_SYSTEM("Found SG Timer (UUID-based)");

      SGTimerDevice* sgDevice = new SGTimerDevice();
      timerDevice = std::unique_ptr<ITimerDevice>(sgDevice);
      setupCallbacks();

      if (timerDevice->initialize() && sgDevice->attemptConnection(&device)) {
        LOG_SYSTEM("Successfully connected to SG Timer");
        deviceFound = true;
        break;
      } else {
        LOG_ERROR("TIMER", "Failed to connect to SG Timer");
        timerDevice.reset();
      }
    }
    // Try UUID-based Special Pie Timer
    else if (SpecialPieTimerDevice::matchesDevice(&device)) {
      LOG_SYSTEM("Found UUID-based Special Pie Timer");

      SpecialPieTimerDevice* specialPieDevice = new SpecialPieTimerDevice();
      timerDevice = std::unique_ptr<ITimerDevice>(specialPieDevice);
      setupCallbacks();

      if (timerDevice->initialize() && specialPieDevice->attemptConnection(&device)) {
        LOG_SYSTEM("Successfully connected to Special Pie Timer");
        deviceFound = true;
        break;
      } else {
        LOG_ERROR("TIMER", "Failed to connect to Special Pie Timer");
        timerDevice.reset();
      }
    }
    // Try ASN Tracker
    else if (ASNTrackerDevice::matchesDevice(&device)) {
      LOG_SYSTEM("Found ASN Tracker");

      ASNTrackerDevice* asnDevice = new ASNTrackerDevice();
      timerDevice = std::unique_ptr<ITimerDevice>(asnDevice);
      setupCallbacks();

      if (timerDevice->initialize() && asnDevice->attemptConnection(&device)) {
        LOG_SYSTEM("Successfully connected to ASN Tracker");
        deviceFound = true;
        break;
      } else {
        LOG_ERROR("TIMER", "Failed to connect to ASN Tracker");
        timerDevice.reset();
      }
    }
  }

  pScan->clearResults();

  if (!deviceFound) {
    LOG_SYSTEM("No compatible timer devices found. Will retry in 5 seconds...");
  }

  isScanning = false;
}
