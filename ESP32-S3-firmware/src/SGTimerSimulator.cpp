#include "SGTimerSimulator.h"
#include "Logger.h"
#include "common.h"

SGTimerSimulator::SGTimerSimulator(SimulationMode mode)
  : connectionState(DeviceConnectionState::DISCONNECTED),
    deviceConnected(false),
    deviceName("SG-SST4-SIM-12345"),
    deviceModel("SG Timer GO Simulator"),
    simulatedAddress("AA:BB:CC:DD:EE:FF"),
    sessionActive(false),
    shotCount(0),
    lastShotTime(0),
    sessionStartTime(0),
    simulationMode(mode),
    lastUpdateTime(0),
    nextShotTime(0),
    connectionStartTime(0),
    sessionStartTime_sim(0),
    autoSessionStarted(false) {

  // Initialize session data
  currentSession = {};
  currentSession.sessionId = 0;
  currentSession.isActive = false;
  currentSession.totalShots = 0;
  currentSession.startTimestamp = 0;
  currentSession.startDelaySeconds = 3.0f;
}

SGTimerSimulator::~SGTimerSimulator() {
  disconnect();
}

bool SGTimerSimulator::initialize() {
  LOG_INFO("SIM-TIMER", "Initializing SG Timer Simulator (Mode: %d)", (int)simulationMode);
  setConnectionState(DeviceConnectionState::DISCONNECTED);
  return true;
}

bool SGTimerSimulator::startScanning() {
  LOG_INFO("SIM-TIMER", "Starting scan simulation");
  setConnectionState(DeviceConnectionState::SCANNING);

  if (simulationMode == SimulationMode::AUTO_CONNECT ||
      simulationMode == SimulationMode::AUTO_SHOTS ||
      simulationMode == SimulationMode::REALISTIC) {
    // Auto-connect after a short delay
    connectionStartTime = millis();
  }

  return true;
}

bool SGTimerSimulator::connect(BLEAddress address) {
  LOG_INFO("SIM-TIMER", "Connecting to simulated device");
  setConnectionState(DeviceConnectionState::CONNECTING);
  connectionStartTime = millis();
  return true;
}

void SGTimerSimulator::disconnect() {
  LOG_INFO("SIM-TIMER", "Disconnecting from simulated device");
  deviceConnected = false;
  sessionActive = false;
  setConnectionState(DeviceConnectionState::DISCONNECTED);
  reset();
}

DeviceConnectionState SGTimerSimulator::getConnectionState() const {
  return connectionState;
}

bool SGTimerSimulator::isConnected() const {
  return deviceConnected;
}

const char* SGTimerSimulator::getDeviceModel() const {
  return deviceModel.c_str();
}

const char* SGTimerSimulator::getDeviceName() const {
  return deviceName.c_str();
}

BLEAddress SGTimerSimulator::getDeviceAddress() const {
  return simulatedAddress;
}

void SGTimerSimulator::onShotDetected(std::function<void(const NormalizedShotData&)> callback) {
  shotDetectedCallback = callback;
}

void SGTimerSimulator::onSessionStarted(std::function<void(const SessionData&)> callback) {
  sessionStartedCallback = callback;
}

void SGTimerSimulator::onSessionStopped(std::function<void(const SessionData&)> callback) {
  sessionStoppedCallback = callback;
}

void SGTimerSimulator::onSessionSuspended(std::function<void(const SessionData&)> callback) {
  sessionSuspendedCallback = callback;
}

void SGTimerSimulator::onSessionResumed(std::function<void(const SessionData&)> callback) {
  sessionResumedCallback = callback;
}

void SGTimerSimulator::onConnectionStateChanged(std::function<void(DeviceConnectionState)> callback) {
  connectionStateCallback = callback;
}

bool SGTimerSimulator::supportsRemoteStart() const {
  return true; // Simulator supports remote start for testing
}

bool SGTimerSimulator::supportsShotList() const {
  return true; // Simulator supports shot list for testing
}

bool SGTimerSimulator::supportsSessionControl() const {
  return true; // Simulator supports session control for testing
}

bool SGTimerSimulator::requestShotList(uint32_t sessionId) {
  LOG_INFO("SIM-TIMER", "Requesting shot list for session %u", sessionId);
  // In a real simulator, this would return stored shot data
  return true;
}

bool SGTimerSimulator::startSession() {
  if (!deviceConnected) {
    LOG_WARN("SIM-TIMER", "Cannot start session - not connected");
    return false;
  }

  simulateSessionStart();
  return true;
}

bool SGTimerSimulator::stopSession() {
  if (!sessionActive) {
    LOG_WARN("SIM-TIMER", "Cannot stop session - no active session");
    return false;
  }

  simulateSessionEnd();
  return true;
}

void SGTimerSimulator::update() {
  unsigned long currentTime = millis();

  // Only update at specified intervals
  if (currentTime - lastUpdateTime < SimulatorConfig::SIMULATION_STEP_MS) {
    return;
  }

  lastUpdateTime = currentTime;

  switch (connectionState) {
    case DeviceConnectionState::SCANNING:
      if (simulationMode != SimulationMode::MANUAL) {
        // Auto-connect after scanning for a bit
        if (currentTime - connectionStartTime > 1000) {
          simulateConnection();
        }
      }
      break;

    case DeviceConnectionState::CONNECTING:
      simulateConnection();
      break;

    case DeviceConnectionState::CONNECTED:
      // Handle auto-session start
      if (!autoSessionStarted &&
          (simulationMode == SimulationMode::AUTO_SHOTS || simulationMode == SimulationMode::REALISTIC)) {
        if (currentTime - connectionStartTime > SimulatorConfig::SESSION_START_DELAY_MS) {
          simulateSessionStart();
          autoSessionStarted = true;
        }
      }

      // Handle shot simulation
      if (sessionActive &&
          (simulationMode == SimulationMode::AUTO_SHOTS || simulationMode == SimulationMode::REALISTIC)) {

        if (currentTime >= nextShotTime && shotCount < SimulatorConfig::MAX_SIMULATOR_SHOTS) {
          simulateShot();
          generateRealisticShotTiming();
        }

        // Check for auto session end
        if (simulationMode == SimulationMode::REALISTIC) {
          float secondsSinceStart = (currentTime - sessionStartTime_sim) / 1000.0f;
          if (secondsSinceStart > 10.0f && random(0, 100) < (SimulatorConfig::SESSION_AUTO_STOP_CHANCE * 100)) {
            simulateSessionEnd();
          }
        }
      }
      break;

    default:
      break;
  }
}

void SGTimerSimulator::setSimulationMode(SimulationMode mode) {
  LOG_INFO("SIM-TIMER", "Changing simulation mode to %d", (int)mode);
  simulationMode = mode;
}

void SGTimerSimulator::simulateManualShot() {
  if (sessionActive) {
    simulateShot();
  } else {
    LOG_WARN("SIM-TIMER", "Cannot simulate shot - no active session");
  }
}

void SGTimerSimulator::simulateManualSessionStart() {
  if (deviceConnected && !sessionActive) {
    simulateSessionStart();
  } else {
    LOG_WARN("SIM-TIMER", "Cannot start session - not connected or session already active");
  }
}

void SGTimerSimulator::simulateManualSessionStop() {
  if (sessionActive) {
    simulateSessionEnd();
  } else {
    LOG_WARN("SIM-TIMER", "Cannot stop session - no active session");
  }
}

void SGTimerSimulator::reset() {
  shotCount = 0;
  lastShotTime = 0;
  sessionStartTime = 0;
  sessionStartTime_sim = 0;
  nextShotTime = 0;
  autoSessionStarted = false;
  currentSession = {};
}

void SGTimerSimulator::setConnectionState(DeviceConnectionState newState) {
  if (connectionState != newState) {
    connectionState = newState;
    if (connectionStateCallback) {
      connectionStateCallback(newState);
    }
  }
}

void SGTimerSimulator::simulateConnection() {
  unsigned long currentTime = millis();

  if (connectionState == DeviceConnectionState::SCANNING) {
    setConnectionState(DeviceConnectionState::CONNECTING);
    connectionStartTime = currentTime;
  }
  else if (connectionState == DeviceConnectionState::CONNECTING) {
    if (currentTime - connectionStartTime > SimulatorConfig::CONNECTION_DELAY_MS) {
      deviceConnected = true;
      setConnectionState(DeviceConnectionState::CONNECTED);
      LOG_INFO("SIM-TIMER", "Simulated connection established");
    }
  }
}

void SGTimerSimulator::simulateSessionStart() {
  if (!deviceConnected) {
    LOG_WARN("SIM-TIMER", "Cannot start session - not connected");
    return;
  }

  unsigned long currentTime = millis();

  // Generate a unique session ID
  currentSession.sessionId = currentTime / 1000; // Use Unix timestamp
  currentSession.isActive = true;
  currentSession.totalShots = 0;
  currentSession.startTimestamp = currentTime;
  currentSession.startDelaySeconds = 3.0f;

  sessionActive = true;
  shotCount = 0;
  lastShotTime = 0;
  sessionStartTime = currentTime;
  sessionStartTime_sim = currentTime;

  // Schedule first shot
  generateRealisticShotTiming();

  LOG_INFO("SIM-TIMER", "Session started (ID: %u)", currentSession.sessionId);

  if (sessionStartedCallback) {
    sessionStartedCallback(currentSession);
  }
}

void SGTimerSimulator::simulateShot() {
  if (!sessionActive) {
    return;
  }

  unsigned long currentTime = millis();
  shotCount++;

  uint32_t absoluteTime = currentTime - sessionStartTime;
  uint32_t splitTime = (shotCount == 1) ? 0 : (absoluteTime - lastShotTime);

  lastShotTime = absoluteTime;

  NormalizedShotData shotData = createShotData(shotCount, absoluteTime);
  shotData.splitTimeMs = splitTime;

  LOG_INFO("SIM-TIMER", "Shot simulated: #%d, Time: %.3fs",
           shotCount, absoluteTime / 1000.0f);

  if (shotDetectedCallback) {
    shotDetectedCallback(shotData);
  }

  // Update session total
  currentSession.totalShots = shotCount;
}

void SGTimerSimulator::simulateSessionEnd() {
  if (!sessionActive) {
    return;
  }

  sessionActive = false;
  currentSession.isActive = false;
  currentSession.totalShots = shotCount;

  LOG_INFO("SIM-TIMER", "Session ended (ID: %u, Total shots: %d)",
           currentSession.sessionId, shotCount);

  if (sessionStoppedCallback) {
    sessionStoppedCallback(currentSession);
  }
}

void SGTimerSimulator::generateRealisticShotTiming() {
  unsigned long currentTime = millis();
  uint32_t interval;

  if (simulationMode == SimulationMode::REALISTIC) {
    // Generate more realistic timing based on shot number
    if (shotCount < 5) {
      // Initial shots - faster pace
      interval = random(SimulatorConfig::MIN_SHOT_INTERVAL_MS,
                       SimulatorConfig::MIN_SHOT_INTERVAL_MS + 800);
    } else if (shotCount < 10) {
      // Mid-string - variable pace
      interval = random(SimulatorConfig::MIN_SHOT_INTERVAL_MS + 200,
                       SimulatorConfig::MAX_SHOT_INTERVAL_MS - 500);
    } else {
      // Later shots - slower, more deliberate
      interval = random(SimulatorConfig::MAX_SHOT_INTERVAL_MS - 800,
                       SimulatorConfig::MAX_SHOT_INTERVAL_MS);
    }
  } else {
    // Simple random interval for auto mode
    interval = generateRandomShotInterval();
  }

  nextShotTime = currentTime + interval;
}

uint32_t SGTimerSimulator::generateRandomShotInterval() {
  return random(SimulatorConfig::MIN_SHOT_INTERVAL_MS,
                SimulatorConfig::MAX_SHOT_INTERVAL_MS);
}

NormalizedShotData SGTimerSimulator::createShotData(uint16_t shotNumber, uint32_t absoluteTime) {
  NormalizedShotData shotData;
  shotData.sessionId = currentSession.sessionId;
  shotData.shotNumber = shotNumber;
  shotData.absoluteTimeMs = absoluteTime;
  shotData.splitTimeMs = 0; // Will be set by caller
  shotData.timestampMs = millis();
  shotData.deviceModel = deviceModel.c_str();
  shotData.isFirstShot = (shotNumber == 1);

  return shotData;
}