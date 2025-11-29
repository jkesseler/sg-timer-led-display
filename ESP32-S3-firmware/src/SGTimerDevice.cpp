#include "SGTimerDevice.h"
#include "Logger.h"
#include "common.h"

// Static constants - Service UUIDs for device discovery
const char* SGTimerDevice::SERVICE_UUID = "7520FFFF-14D2-4CDA-8B6B-697C554C9311";
const char* SGTimerDevice::CHARACTERISTIC_UUID = "75200001-14D2-4CDA-8B6B-697C554C9311";
const char* SGTimerDevice::SHOT_LIST_UUID = "75200004-14D2-4CDA-8B6B-697C554C9311";

// Static instance for callbacks
SGTimerDevice* SGTimerDevice::instance = nullptr;

SGTimerDevice::SGTimerDevice() :
  pClient(nullptr),
  pEventCharacteristic(nullptr),
  pService(nullptr),
  connectionState(DeviceConnectionState::DISCONNECTED),
  isConnectedFlag(false),
  lastReconnectAttempt(0),
  lastHeartbeat(0),
  deviceModel("SG Timer"),
  deviceName(""),
  deviceAddress("00:00:00:00:00:00"),
  previousShotTime(0),
  hasFirstShot(false),
  lastShotNum(0),
  lastShotSeconds(0),
  lastShotHundredths(0),
  hasLastShot(false)
{
  instance = this;
  currentSession = {};
}

SGTimerDevice::~SGTimerDevice() {
  disconnect();
  instance = nullptr;
}

bool SGTimerDevice::initialize() {
  LOG_INFO("SG-TIMER", "Initializing SG Timer device interface");
  setConnectionState(DeviceConnectionState::DISCONNECTED);
  return true;
}

bool SGTimerDevice::startScanning() {
  LOG_INFO("SG-TIMER", "Will start scanning for target device");
  setConnectionState(DeviceConnectionState::SCANNING);
  return true;
}

bool SGTimerDevice::connect(BLEAddress address) {
  // Not used in simplified version - we target specific device
  return false;
}

void SGTimerDevice::disconnect() {
  if (pClient && isConnectedFlag) {
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
  }
  isConnectedFlag = false;
  pEventCharacteristic = nullptr;
  pService = nullptr;
  setConnectionState(DeviceConnectionState::DISCONNECTED);
}

DeviceConnectionState SGTimerDevice::getConnectionState() const {
  return connectionState;
}

bool SGTimerDevice::isConnected() const {
  return isConnectedFlag;
}

const char* SGTimerDevice::getDeviceModel() const {
  return deviceModel.c_str();
}

const char* SGTimerDevice::getDeviceName() const {
  return deviceName.c_str();
}

BLEAddress SGTimerDevice::getDeviceAddress() const {
  return deviceAddress;
}

void SGTimerDevice::onShotDetected(std::function<void(const NormalizedShotData&)> callback) {
  shotDetectedCallback = callback;
}

void SGTimerDevice::onSessionStarted(std::function<void(const SessionData&)> callback) {
  sessionStartedCallback = callback;
}

void SGTimerDevice::onCountdownComplete(std::function<void(const SessionData&)> callback) {
  countdownCompleteCallback = callback;
}

void SGTimerDevice::onSessionStopped(std::function<void(const SessionData&)> callback) {
  sessionStoppedCallback = callback;
}

void SGTimerDevice::onSessionSuspended(std::function<void(const SessionData&)> callback) {
  sessionSuspendedCallback = callback;
}

void SGTimerDevice::onSessionResumed(std::function<void(const SessionData&)> callback) {
  sessionResumedCallback = callback;
}

void SGTimerDevice::onConnectionStateChanged(std::function<void(DeviceConnectionState)> callback) {
  connectionStateCallback = callback;
}

bool SGTimerDevice::supportsRemoteStart() const {
  return false;
}

bool SGTimerDevice::supportsShotList() const {
  return false;  // Simplified for now
}

bool SGTimerDevice::supportsSessionControl() const {
  return false;
}

bool SGTimerDevice::requestShotList(uint32_t sessionId) {
  return false;  // Simplified for now
}

bool SGTimerDevice::startSession() {
  return false;
}

bool SGTimerDevice::stopSession() {
  return false;
}

// Main update loop - check connection health
void SGTimerDevice::update() {
  // If connected, check connection status
  if (isConnectedFlag) {
    if (pClient && pClient->isConnected()) {
      // Print heartbeat every 30 seconds
      if (millis() - lastHeartbeat > 30000) {
        LOG_BLE("SG-TIMER connected - waiting for events");
        lastHeartbeat = millis();
      }
    } else {
      // Connection lost
      LOG_WARN("SG-TIMER", "Connection lost");
      isConnectedFlag = false;
      pService = nullptr;
      pEventCharacteristic = nullptr;

      if (pClient) {
        delete pClient;
        pClient = nullptr;
      }

      setConnectionState(DeviceConnectionState::DISCONNECTED);

      // Reset session tracking
      hasFirstShot = false;
      previousShotTime = 0;
      currentSession = {};

      LOG_BLE("Will attempt to reconnect");
    }
  }
  // Note: Scanning is handled by TimerApplication for multi-device support
}

void SGTimerDevice::setConnectionState(DeviceConnectionState newState) {
  if (connectionState != newState) {
    connectionState = newState;
    if (connectionStateCallback) {
      connectionStateCallback(newState);
    }
  }
}

// Discover and connect to any SG Timer device by service UUID
void SGTimerDevice::attemptConnection() {
  LOG_BLE("Starting SG Timer device scan");
  setConnectionState(DeviceConnectionState::SCANNING);

  BLEScan* pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);

  LOG_BLE("Scanning for SG Timer devices (10 seconds)");
  BLEScanResults foundDevices = pScan->start(10, false);

  BLEUUID serviceUuid(SERVICE_UUID);
  bool deviceFound = false;

  // Look for any device advertising the SG Timer service UUID
  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);

    // Check if device advertises the SG Timer service
    if (device.isAdvertisingService(serviceUuid)) {
      if (device.haveName()) {
        LOG_BLE("SG Timer found: %s (%s)", device.getAddress().toString().c_str(), device.getName().c_str());
      } else {
        LOG_BLE("SG Timer found: %s", device.getAddress().toString().c_str());
      }
      deviceFound = true;

      // Store device information
      deviceAddress = device.getAddress();
      if (device.haveName()) {
        deviceName = device.getName().c_str();
        // Extract model from name (SG-SST4XYYYYY where X is model identifier)
        if (deviceName.startsWith("SG-SST4")) {
          char modelId = deviceName.charAt(7);
          if (modelId == 'A') {
            deviceModel = "SG Timer Sport";
          } else if (modelId == 'B') {
            deviceModel = "SG Timer GO";
          } else {
            deviceModel = "SG Timer";
          }
        }
      } else {
        deviceName = device.getAddress().toString().c_str();
      }

      // Wait before attempting connection
      LOG_BLE("Waiting 2 seconds before connecting");
      delay(2000);

      setConnectionState(DeviceConnectionState::CONNECTING);
      pClient = BLEDevice::createClient();

      if (!pClient) {
        LOG_ERROR("SG-TIMER", "Failed to create BLE client");
        setConnectionState(DeviceConnectionState::ERROR);
        break;
      }

      LOG_BLE("Attempting connection");
      if (pClient->connect(&device)) {
        LOG_BLE("Connected to device");
        pService = pClient->getService(serviceUuid);

        if (pService != nullptr) {
          LOG_BLE("Service found");

          pEventCharacteristic = pService->getCharacteristic(CHARACTERISTIC_UUID);

          if (pEventCharacteristic != nullptr) {
            LOG_BLE("EVENT characteristic found");

            // Check if characteristic can notify
            if (pEventCharacteristic->canNotify()) {
              LOG_BLE("Registering for notifications");
              pEventCharacteristic->registerForNotify(notifyCallback);
              LOG_BLE("Successfully registered for notifications - listening for events");
              isConnectedFlag = true;
              lastHeartbeat = millis();
              setConnectionState(DeviceConnectionState::CONNECTED);
            } else {
              LOG_ERROR("SG-TIMER", "Characteristic cannot notify");
              pClient->disconnect();
              delete pClient;
              pClient = nullptr;
              setConnectionState(DeviceConnectionState::ERROR);
            }
          } else {
            LOG_ERROR("SG-TIMER", "EVENT characteristic not found");
            pClient->disconnect();
            delete pClient;
            pClient = nullptr;
            setConnectionState(DeviceConnectionState::ERROR);
          }
        } else {
          LOG_ERROR("SG-TIMER", "Service not found");
          pClient->disconnect();
          delete pClient;
          pClient = nullptr;
          setConnectionState(DeviceConnectionState::ERROR);
        }
      } else {
        LOG_ERROR("SG-TIMER", "Failed to connect");
        delete pClient;
        pClient = nullptr;
        setConnectionState(DeviceConnectionState::ERROR);
      }

      break;
    }
  }

  pScan->clearResults();

  if (!deviceFound) {
    LOG_BLE("No SG Timer devices found - retrying in 5 seconds");
    setConnectionState(DeviceConnectionState::DISCONNECTED);
  } else if (!isConnectedFlag) {
    LOG_ERROR("SG-TIMER", "Connection failed - retrying in 5 seconds");
    setConnectionState(DeviceConnectionState::ERROR);
  }
}

// Static notification callback
void SGTimerDevice::notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                  uint8_t* pData, size_t length, bool isNotify) {
  if (instance) {
    instance->processTimerData(pData, length);
  }
}

void SGTimerDevice::processTimerData(uint8_t* pData, size_t length) {
  if (Logger::getLevel() <= LogLevel::DEBUG) {
    LOG_DEBUG("SG-TIMER", "Notification received (%d bytes)", length);
    for (size_t i = 0; i < length; i++) {
      Serial.printf("%02X ", pData[i]);
    }
    Serial.println();
  }

  // Parse event based on API documentation
  if (length >= 2) {
    uint8_t len = pData[0];
    SGTimerEvent event_id = static_cast<SGTimerEvent>(pData[1]);

    switch (event_id) {
      case SGTimerEvent::SESSION_STARTED:
        if (length >= 8) {
          uint32_t sess_id = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
          uint16_t start_delay = (pData[6] << 8) | pData[7];
          LOG_TIMER("SESSION_STARTED - ID: %u, Delay: %.1fs", sess_id, start_delay * 0.1);

          // Update session state
          currentSession.sessionId = sess_id;
          currentSession.isActive = true;
          currentSession.totalShots = 0;
          currentSession.startTimestamp = millis();
          currentSession.startDelaySeconds = start_delay * 0.1;

          // Reset shot tracking
          hasFirstShot = false;
          previousShotTime = 0;

          // Notify callback
          if (sessionStartedCallback) {
            sessionStartedCallback(currentSession);
          }
        }
        break;

      case SGTimerEvent::SESSION_SUSPENDED:
        if (length >= 8) {
          uint32_t sess_id = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
          uint16_t total_shots = (pData[6] << 8) | pData[7];
          LOG_TIMER("SESSION_SUSPENDED - ID: %u, Total shots: %u", sess_id, total_shots);

          currentSession.totalShots = total_shots;
          if (sessionSuspendedCallback) {
            sessionSuspendedCallback(currentSession);
          }
        }
        break;

      case SGTimerEvent::SESSION_RESUMED:
        if (length >= 8) {
          uint32_t sess_id = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
          uint16_t total_shots = (pData[6] << 8) | pData[7];
          LOG_TIMER("SESSION_RESUMED - ID: %u, Total shots: %u", sess_id, total_shots);

          currentSession.totalShots = total_shots;
          if (sessionResumedCallback) {
            sessionResumedCallback(currentSession);
          }
        }
        break;

      case SGTimerEvent::SESSION_STOPPED:
        if (length >= 8) {
          uint32_t sess_id = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
          uint16_t total_shots = (pData[6] << 8) | pData[7];
          if (hasLastShot) {
            LOG_TIMER("SESSION_STOPPED - ID: %u, Total shots: %u, Last: #%u at %u:%02u",
                     sess_id, total_shots, lastShotNum + 1, lastShotSeconds, lastShotHundredths);
          } else {
            LOG_TIMER("SESSION_STOPPED - ID: %u, Total shots: %u", sess_id, total_shots);
          }

          currentSession.isActive = false;
          currentSession.totalShots = total_shots;

          if (sessionStoppedCallback) {
            sessionStoppedCallback(currentSession);
          }

          // Reset last shot tracking for next session
          hasLastShot = false;
          hasFirstShot = false;
          previousShotTime = 0;
        }
        break;

      case SGTimerEvent::SHOT_DETECTED:
        if (length >= 12) {
          uint32_t sess_id = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
          uint16_t shot_num = (pData[6] << 8) | pData[7];
          uint32_t shot_time_ms = (pData[8] << 24) | (pData[9] << 16) | (pData[10] << 8) | pData[11];

          // Convert milliseconds to seconds:hundredths format
          uint32_t seconds = shot_time_ms / 1000;
          uint32_t hundredths = (shot_time_ms % 1000) / 10;

          LOG_DEBUG("SG-TIMER", "SHOT_DETECTED #%u: %u:%02u", shot_num + 1, seconds, hundredths);

          // Store as last shot
          lastShotNum = shot_num;
          lastShotSeconds = seconds;
          lastShotHundredths = hundredths;
          hasLastShot = true;

          // Calculate split time
          uint32_t splitTime = 0;
          bool isFirstShot = !hasFirstShot;

          if (hasFirstShot) {
            splitTime = shot_time_ms - previousShotTime;
          } else {
            hasFirstShot = true;
          }

          // Update previous shot time
          previousShotTime = shot_time_ms;

          // Create normalized shot data
          NormalizedShotData shotData;
          shotData.sessionId = sess_id;
          shotData.shotNumber = shot_num + 1;  // SG Timer reports 0-based, convert to 1-based
          shotData.absoluteTimeMs = shot_time_ms;
          shotData.splitTimeMs = splitTime;
          shotData.timestampMs = millis();
          shotData.deviceModel = deviceModel.c_str();
          shotData.isFirstShot = isFirstShot;

          // Notify callback
          if (shotDetectedCallback) {
            shotDetectedCallback(shotData);
          }
        }
        break;

      case SGTimerEvent::SESSION_SET_BEGIN:
        if (length >= 6) {
          uint32_t sess_id = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
          LOG_TIMER("SESSION_SET_BEGIN - ID: %u (countdown complete)", sess_id);

          // Notify callback that countdown has completed
          if (countdownCompleteCallback) {
            countdownCompleteCallback(currentSession);
          }
        }
        break;

      default:
        LOG_WARN("SG-TIMER", "Unknown event ID: 0x%02X", static_cast<uint8_t>(event_id));
        break;
    }
  }
}