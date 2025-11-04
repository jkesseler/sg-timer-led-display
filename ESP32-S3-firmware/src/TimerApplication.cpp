#include "TimerApplication.h"
#include "SGTimerDevice.h"
#include "SGTimerSimulator.h"
#include "SimulatorCommands.h"
#include "SystemStateMachine.h"
#include "ButtonHandler.h"
#include "common.h"
#include <BLEDevice.h>

TimerApplication::TimerApplication()
  : sessionActive(false),
    lastShotNumber(0),
    lastShotTime(0),
    showSessionEnd(false),
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

  // Initialize brightness controller
  brightnessController = std::unique_ptr<BrightnessController>(new BrightnessController());
  if (!brightnessController->initialize()) {
    LOG_ERROR("SYSTEM", "Failed to initialize brightness controller");
    return false;
  }

  // Connect brightness controller to display manager
  brightnessController->setBrightnessCallback([this](uint8_t brightness) {
    displayManager->setBrightness(brightness);
  });

  // Initialize state machine
  stateMachine = std::unique_ptr<SystemStateMachine>(new SystemStateMachine(this));
  if (!stateMachine) {
    LOG_ERROR("SYSTEM", "Failed to create state machine");
    return false;
  }

  // Initialize button handler
  buttonHandler = std::unique_ptr<ButtonHandler>(new ButtonHandler());
  if (!buttonHandler || !buttonHandler->initialize()) {
    LOG_ERROR("SYSTEM", "Failed to initialize button handler");
    return false;
  }

  // Initialize BLE (only if not using simulator)
#ifndef USE_SIMULATOR
  BLEDevice::init(BLE_DEVICE_NAME);
  LOG_BLE("ESP32-S3 BLE Client initialized");
#endif

  // Create timer device (SG Timer implementation or simulator)
#ifdef USE_SIMULATOR
  LOG_SYSTEM("Using SG Timer Simulator");
  simulator = std::unique_ptr<SGTimerSimulator>(new SGTimerSimulator(SimulationMode::AUTO_SHOTS));
  // Note: We can't assign simulator.get() directly to unique_ptr,
  // so we'll use simulator directly and cast when needed
  simCommands = std::unique_ptr<SimulatorCommands>(new SimulatorCommands(simulator.get()));
#else
  LOG_SYSTEM("Using Real SG Timer Device");
  timerDevice = std::unique_ptr<SGTimerDevice>(new SGTimerDevice());
#endif

  // Set up callbacks
  setupCallbacks();

  // Initialize the timer device
#ifdef USE_SIMULATOR
  ITimerDevice* device = simulator.get();
#else
  ITimerDevice* device = timerDevice.get();
#endif

  if (!device || !device->initialize()) {
    LOG_ERROR("TIMER", "Failed to initialize timer device");
    return false;
  }

  LOG_TIMER("Timer device initialized successfully");
  device->startScanning();

  // Initialize state machine
  stateMachine->initialize();

  LOG_SYSTEM("Application initialized successfully");
  lastActivityTime = millis();
  return true;
}

void TimerApplication::run() {
  // Check for button presses
  if (buttonHandler && buttonHandler->checkButtonPress()) {
    handleButtonPress();
  }

  // Update state machine first
  if (stateMachine) {
    stateMachine->update();
  }

  // Update all components
#ifdef USE_SIMULATOR
  if (simulator) {
    simulator->update();
  }
#else
  if (timerDevice) {
    timerDevice->update();
  }
#endif

#ifdef USE_SIMULATOR
  // Update simulator command interface
  if (simCommands) {
    simCommands->update();
  }
#endif

  if (brightnessController) {
    brightnessController->update();
  }

  if (displayManager) {
    displayManager->update();
  }

  // Perform periodic health checks
  performHealthCheck();

  delay(MAIN_LOOP_DELAY);
}

void TimerApplication::setupCallbacks() {
#ifdef USE_SIMULATOR
  ITimerDevice* device = simulator.get();
#else
  ITimerDevice* device = timerDevice.get();
#endif

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

  // Notify state machine
  if (stateMachine) {
    stateMachine->onSessionEvent("shot");
  }

  // Update display if session is active
  if (sessionActive && displayManager) {
    displayManager->showShotData(shotData);
  }

  logShotData(shotData);
}

void TimerApplication::onSessionStarted(const SessionData& sessionData) {
  LOG_TIMER("Session started: ID %u", sessionData.sessionId);

  sessionActive = true;
  showSessionEnd = false;
  lastShotNumber = 0;
  lastShotTime = 0;

  // Notify state machine
  if (stateMachine) {
    stateMachine->onSessionEvent("started");
  }

  if (displayManager) {
    displayManager->showWaitingForShots(sessionData);
  }
}

void TimerApplication::onSessionStopped(const SessionData& sessionData) {
  LOG_TIMER("Session stopped: ID %u, Total shots: %d",
            sessionData.sessionId, sessionData.totalShots);

  sessionActive = false;
  showSessionEnd = true;

  // Notify state machine
  if (stateMachine) {
    stateMachine->onSessionEvent("ended");
  }

  if (displayManager) {
    displayManager->showSessionEnd(sessionData, lastShotNumber);
  }
}

void TimerApplication::onSessionSuspended(const SessionData& sessionData) {
  LOG_TIMER("Session suspended: ID %u, Total shots: %d",
            sessionData.sessionId, sessionData.totalShots);

  // Keep sessionActive true for suspended sessions
  // Notify state machine
  if (stateMachine) {
    stateMachine->onSessionEvent("suspended");
  }

  // Display manager will handle the visual state
}

void TimerApplication::onSessionResumed(const SessionData& sessionData) {
  LOG_TIMER("Session resumed: ID %u, Total shots: %d",
            sessionData.sessionId, sessionData.totalShots);

  sessionActive = true;
  showSessionEnd = false;

  // Notify state machine
  if (stateMachine) {
    stateMachine->onSessionEvent("resumed");
  }

  if (displayManager) {
    displayManager->showWaitingForShots(sessionData);
  }
}

void TimerApplication::onConnectionStateChanged(DeviceConnectionState state) {
  LOG_BLE("Connection state changed: %d", (int)state);
  updateActivityTime();

  // Notify state machine
  if (stateMachine) {
    stateMachine->onConnectionStateChanged(state);
  }

  // Update display based on connection state
  if (state == DeviceConnectionState::DISCONNECTED && sessionActive) {
    sessionActive = false;
    showSessionEnd = false;
  }

  const char* deviceName = nullptr;
#ifdef USE_SIMULATOR
  if (simulator) {
    deviceName = simulator->getDeviceName();
  }
#else
  if (timerDevice) {
    deviceName = timerDevice->getDeviceName();
  }
#endif

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
#ifdef USE_SIMULATOR
    bool timerHealthy = simulator != nullptr;
#else
    bool timerHealthy = timerDevice != nullptr;
#endif
    bool brightnessHealthy = brightnessController != nullptr;

    if (!displayHealthy) {
      LOG_ERROR("HEALTH", "Display manager is not healthy");
    }

    if (!timerHealthy) {
      LOG_ERROR("HEALTH", "Timer device is not healthy");
    }

    if (!brightnessHealthy) {
      LOG_ERROR("HEALTH", "Brightness controller is not healthy");
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
#ifdef USE_SIMULATOR
  bool timerHealthy = simulator != nullptr;
#else
  bool timerHealthy = timerDevice != nullptr;
#endif
  bool brightnessHealthy = brightnessController != nullptr;
  bool activityHealthy = (millis() - lastActivityTime) < AppConfig::WATCHDOG_TIMEOUT_MS;

  return displayHealthy && timerHealthy && brightnessHealthy && activityHealthy;
}

unsigned long TimerApplication::getUptimeMs() const {
  return millis();
}

bool TimerApplication::isInitialized() const {
  bool displayHealthy = displayManager && displayManager->isInitialized();
#ifdef USE_SIMULATOR
  bool timerHealthy = simulator != nullptr;
#else
  bool timerHealthy = timerDevice != nullptr;
#endif
  bool brightnessHealthy = brightnessController != nullptr;
  bool stateMachineHealthy = stateMachine != nullptr;
  bool buttonHealthy = buttonHandler != nullptr;

  return displayHealthy && timerHealthy && brightnessHealthy && stateMachineHealthy && buttonHealthy;
}

void TimerApplication::resetToInitialState() {
  LOG_SYSTEM("Resetting application to initial state");

  // Reset application state
  sessionActive = false;
  lastShotNumber = 0;
  lastShotTime = 0;
  showSessionEnd = false;

  // Disconnect from device if connected
#ifdef USE_SIMULATOR
  if (simulator && simulator->isConnected()) {
    simulator->disconnect();
  }
#else
  if (timerDevice && timerDevice->isConnected()) {
    timerDevice->disconnect();
  }
#endif

  // Update activity time
  updateActivityTime();
}

void TimerApplication::handleButtonPress() {
  LOG_SYSTEM("Button press detected");
  if (stateMachine) {
    stateMachine->onButtonPressed();
  }
}