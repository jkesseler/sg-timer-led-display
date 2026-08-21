#include "TimerApplication.h"
#include "SGTimer.h"
#include "SpecialPieM1A2Plus.h"
#include "SpecialPieM1A2F.h"
#include "ASNTracker.h"
#include "WiFiConfig.h"
#include "common.h"
#include <BLEDevice.h>
#include <esp_coexist.h>

namespace {
TimerApplication* gTimerApplicationInstance = nullptr;
volatile bool gBleScanResultsReady = false;

void onBleScanComplete(BLEScanResults /*scanResults*/) {
  (void)gTimerApplicationInstance;
  gBleScanResultsReady = true;
}
}

TimerApplication::TimerApplication()
  : sessionActive(false),
    lastShotNumber(0),
    lastShotTime(0),
    maxQueueDepth(0),
    totalShotsQueued(0),
    shotEnqueueFailures(0),
    lastScanAttempt(0),
    isScanning(false),
    scanResultsReady(false),
    startupTime(0),
    deviceResetPending(false),
    lastHealthCheck(0),
    lastActivityTime(0),
    hadDeviceConnected(false) {
  gTimerApplicationInstance = this;
}

TimerApplication::~TimerApplication() {
  if (gTimerApplicationInstance == this) {
    gTimerApplicationInstance = nullptr;
  }
  // Smart pointers will handle cleanup automatically
}

bool TimerApplication::initialize() {
  LOG_SYSTEM("=== SG Shot Timer BLE Bridge ===");
  LOG_SYSTEM("ESP32-S3 DevKit-C Starting...");

  // Initialize WiFi manager first so runtime configuration is available
  // before BLE setup and device scanning begin.
  // Non-blocking mode keeps startup responsive even without WiFi credentials.
  WiFiConfig::initialize();

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

    // BT and WiFi share one radio on the ESP32-S3; ask the coexistence
    // arbiter for a balanced time-share instead of the SDK's WiFi-preferred
    // default, since this firmware needs both to work at once.
    esp_err_t coexResult = esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);
    if (coexResult != ESP_OK) {
      LOG_WARN("BLE", "Failed to set coexistence preference (err %d)", (int)coexResult);
    }

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
    if (gBleScanResultsReady) {
      gBleScanResultsReady = false;
      scanResultsReady = true;
    }

    if (isScanning && scanResultsReady) {
      processScanResults();
    }

    if (!timerDevice) {
      scanForDevices();
    }

    // Process BLE events - this may trigger callbacks that enqueue shots
    if (timerDevice) {
      timerDevice->update();
    }

    // Tear down a disconnected device here, in the main loop, rather than
    // inside the device's own connection-state callback. Destroying it from
    // within its callback would free the object whose method is still on the
    // call stack (use-after-free). See onConnectionStateChanged().
    if (deviceResetPending) {
      deviceResetPending = false;
      timerDevice.reset();
      LOG_BLE("Timer device released - ready to rescan");
    }
  }

  // ============================================================
  // PHASE 3: MQTT
  // ============================================================
  // MqttManager owns its own background task (core 0) for all socket I/O
  // and queue draining - nothing to pump from this loop.

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
  // CRITICAL: Enqueue for async MQTT publishing.
  // This is called from BLE callback context - must be fast! enqueueShot()
  // only touches a FreeRTOS queue; the actual publish happens later on
  // MqttManager's own background task.
  // Only queue if MQTT is available - don't buffer when unavailable
  // ============================================================
  // Only publish to MQTT if TIMER_TYPE == TIMER_TYPE_BLE and connected to MQTT broker
  bool shouldPublishMqtt = (TIMER_TYPE == TIMER_TYPE_BLE) && mqttManager && mqttManager->canPublish();

  if (shouldPublishMqtt) {
    if (mqttManager->enqueueShot(shotData)) {
      totalShotsQueued++;

      // Track max queue depth for diagnostics
      uint16_t depth = (uint16_t)mqttManager->getQueueDepth();
      if (depth > maxQueueDepth) {
        maxQueueDepth = depth;
        if (maxQueueDepth > AppConfig::QUEUE_DEPTH_WARN_THRESHOLD) {
          LOG_WARN("QUEUE", "Peak queue depth: %u", maxQueueDepth);
        }
      }
    } else {
      // Queue full - this indicates MQTT can't keep up
      shotEnqueueFailures++;
      LOG_ERROR("QUEUE", "Buffer full! Shot #%u dropped (failures: %lu)",
                shotData.shotNumber, (unsigned long)shotEnqueueFailures);
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

  // Enqueue for the MQTT task to publish (this callback runs on the BLE task)
  if (mqttManager && mqttManager->canPublish()) {
    mqttManager->enqueueSessionStarted(sessionData.sessionId, sessionData.startDelaySeconds);
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
    mqttManager->enqueueCountdownComplete(sessionData.sessionId);
  }

  if (displayManager) {
    displayManager->showWaitingForShots(sessionData);
  }
}

void TimerApplication::onSessionStopped(const SessionData& sessionData) {
  LOG_TIMER("Session stopped: ID %u, Total shots: %d",
            sessionData.sessionId, sessionData.totalShots);

  sessionActive = false;

  // Enqueue session-stopped behind whatever shots are already queued: they
  // were detected before this event fired, so the single ordered queue
  // publishes them first and this event after - no separate discard needed.
  if (mqttManager && mqttManager->canPublish()) {
    mqttManager->enqueueSessionStopped(sessionData.sessionId, sessionData.totalShots, lastShotTime);
  }

  if (displayManager) {
    displayManager->showSessionEnd(sessionData, lastShotNumber);
  }
}

void TimerApplication::onSessionSuspended(const SessionData& sessionData) {
  LOG_TIMER("Session suspended: ID %u, Total shots: %d",
            sessionData.sessionId, sessionData.totalShots);

  if (mqttManager && mqttManager->canPublish()) {
    mqttManager->enqueueSessionSuspended(sessionData.sessionId);
  }
}

void TimerApplication::onSessionResumed(const SessionData& sessionData) {
  LOG_TIMER("Session resumed: ID %u, Total shots: %d",
            sessionData.sessionId, sessionData.totalShots);

  sessionActive = true;

  if (mqttManager && mqttManager->canPublish()) {
    mqttManager->enqueueSessionResumed(sessionData.sessionId);
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
  }

  // Get device info for MQTT publish
  const char* deviceName = nullptr;
  const char* deviceModel = nullptr;
  if (timerDevice) {
    deviceName = timerDevice->getDeviceName();
    deviceModel = timerDevice->getDeviceModel();
  }

  if (mqttManager && mqttManager->canPublish()) {
    mqttManager->enqueueConnectionState(state, deviceName, deviceModel);
  }

  // Handle disconnection
  if (state == DeviceConnectionState::DISCONNECTED) {
    if (sessionActive) {
      sessionActive = false;
    }
    // Defer the actual teardown to run(): this callback may be invoked from
    // within the device's own update()/handleConnectionLost(), so deleting it
    // here would destroy the object mid-method (use-after-free).
    deviceResetPending = true;
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

  // Queue metrics (only if there's been activity). The queue itself and its
  // draining now live in MqttManager, running on its own background task.
  if (totalShotsQueued > 0 && mqttManager) {
    LOG_DEBUG("HEALTH", "Queue depth: %u, Published: %lu, EnqueueFailures: %lu, PublishFailures: %lu, Peak: %u",
              (unsigned)mqttManager->getQueueDepth(),
              (unsigned long)mqttManager->getTotalPublished(),
              (unsigned long)shotEnqueueFailures,
              (unsigned long)mqttManager->getPublishFailures(),
              maxQueueDepth);
  }

  LOG_DEBUG("HEALTH", "Uptime: %lu ms, Free heap: %u bytes",
            getUptimeMs(), ESP.getFreeHeap());
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

  // Throttle scan attempts - wait between full scan cycles
  if (isScanning || (now - lastScanAttempt < BLE_SCAN_RETRY_INTERVAL_MS)) {
    return;
  }

  lastScanAttempt = now;
  isScanning = true;
  scanResultsReady = false;

  if (displayManager) {
    displayManager->showConnectionState(DeviceConnectionState::SCANNING, nullptr);
  }

  LOG_SYSTEM("Scanning for compatible timer devices...");

  // Unified BLE scan for all device types (MAC-based and UUID-based)
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(BLE_SCAN_INTERVAL);
  pScan->setWindow(BLE_SCAN_WINDOW);

  bool scanStarted = pScan->start(BLE_SCAN_DURATION, onBleScanComplete, false);
  if (!scanStarted) {
    LOG_ERROR("BLE", "Failed to start BLE scan");
    isScanning = false;
    scanResultsReady = false;
    return;
  }

  LOG_SYSTEM("BLE scan started (non-blocking)");
}

void TimerApplication::processScanResults() {
  scanResultsReady = false;

  BLEScan* pScan = BLEDevice::getScan();
  BLEScanResults foundDevices = pScan->getResults();
  LOG_SYSTEM("Scan complete - found %d devices", foundDevices.getCount());

  bool deviceFound = false;

  // Check each discovered device against all known device types
  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);

    // Try MAC-based Special Pie Timer first (highest priority)
    if (SpecialPieM1A2F::matchesDevice(&device)) {
      LOG_SYSTEM("Found MAC-based Special Pie Timer (MAC: %s)",
                 device.getAddress().toString().c_str());

      SpecialPieM1A2F* macDevice = new SpecialPieM1A2F();
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
    else if (SGTimer::matchesDevice(&device)) {
      LOG_SYSTEM("Found SG Timer (UUID-based)");

      SGTimer* sgDevice = new SGTimer();
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
    else if (SpecialPieM1A2Plus::matchesDevice(&device)) {
      LOG_SYSTEM("Found UUID-based Special Pie Timer");

      SpecialPieM1A2Plus* specialPieDevice = new SpecialPieM1A2Plus();
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
    else if (ASNTracker::matchesDevice(&device)) {
      LOG_SYSTEM("Found ASN Tracker");

      ASNTracker* asnDevice = new ASNTracker();
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
