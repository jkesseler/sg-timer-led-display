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
  BaseTimerDevice("SG Timer"),
  pEventCharacteristic(nullptr),
  previousShotTime(0),
  hasFirstShot(false),
  lastShotNum(0),
  lastShotSeconds(0),
  lastShotHundredths(0),
  hasLastShot(false) {
  instance = this;
}

SGTimerDevice::~SGTimerDevice() {
  disconnect();
  instance = nullptr;
}

const char* SGTimerDevice::getLogTag() const {
  return "SG-TIMER";
}

// Connect to the already-discovered SG Timer device
bool SGTimerDevice::attemptConnection(BLEAdvertisedDevice* device) {
  if (!device) {
    LOG_ERROR("SG-TIMER", "Null device pointer passed to attemptConnection");
    setConnectionState(DeviceConnectionState::ERROR);
    return false;
  }

  // Disconnect any existing connection and clean up resources before attempting new connection
  // This prevents memory leaks if this method is called multiple times (retry scenarios)
  disconnect();

  // Reset connection state to ensure clean slate for new connection attempt
  pEventCharacteristic = nullptr;
  pService = nullptr;
  isConnectedFlag = false;

  if (device->haveName()) {
    LOG_BLE("SG Timer found: %s (%s)", device->getAddress().toString().c_str(), device->getName().c_str());
  } else {
    LOG_BLE("SG Timer found: %s", device->getAddress().toString().c_str());
  }

  // Store device information
  deviceAddress = device->getAddress();
  if (device->haveName()) {
    deviceName = device->getName().c_str();
    // Extract model from name (SG-SST4XYYYYY where X is model identifier)
    if (deviceName.startsWith("SG-SST4") && deviceName.length() > 7) {
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
    deviceName = device->getAddress().toString().c_str();
  }

  // Brief delay before connection attempt to allow BLE stack to stabilize
  // Note: This blocking delay is acceptable during initial connection setup
  LOG_BLE("Waiting %dms before connecting", BLE_CONNECTION_DELAY_MS);
  delay(BLE_CONNECTION_DELAY_MS);

  setConnectionState(DeviceConnectionState::CONNECTING);
  pClient = BLEDevice::createClient();

  if (!pClient) {
    LOG_ERROR("SG-TIMER", "Failed to create BLE client");
    setConnectionState(DeviceConnectionState::ERROR);
    return false;
  }

  LOG_BLE("Attempting connection");
  if (pClient->connect(device)) {
    LOG_BLE("Connected to device");
    BLEUUID serviceUuid(SERVICE_UUID);
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
          return true;
        } else {
          LOG_ERROR("SG-TIMER", "Characteristic cannot notify");
          pClient->disconnect();
          delete pClient;
          pClient = nullptr;
          setConnectionState(DeviceConnectionState::ERROR);
          return false;
        }
      } else {
        LOG_ERROR("SG-TIMER", "EVENT characteristic not found");
        pClient->disconnect();
        delete pClient;
        pClient = nullptr;
        setConnectionState(DeviceConnectionState::ERROR);
        return false;
      }
    } else {
      LOG_ERROR("SG-TIMER", "Service not found");
      pClient->disconnect();
      delete pClient;
      pClient = nullptr;
      setConnectionState(DeviceConnectionState::ERROR);
      return false;
    }
  } else {
    LOG_ERROR("SG-TIMER", "Failed to connect");
    delete pClient;
    pClient = nullptr;
    setConnectionState(DeviceConnectionState::ERROR);
    return false;
  }
}

// Static notification callback
void SGTimerDevice::notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                  uint8_t* pData, size_t length, bool isNotify) {
  if (instance && pData && length > 0) {
    instance->processTimerData(pData, length);
  }
}

void SGTimerDevice::processTimerData(uint8_t* pData, size_t length) {
  if (!pData || length == 0) {
    LOG_WARN("SG-TIMER", "Invalid data received (null or empty)");
    return;
  }

  if (Logger::getLevel() <= LogLevel::DEBUG) {
    LOG_DEBUG("SG-TIMER", "Notification received (%d bytes)", length);
    for (size_t i = 0; i < length; i++) {
      Serial.printf("%02X ", pData[i]);
    }
    Serial.println();
  }

  // Parse event based on API documentation
  if (length >= 2) {
    // Validate packet length field (len = number of bytes after length byte)
    uint8_t len = pData[0];
    if (len != length - 1) {
      LOG_ERROR("SG-TIMER", "Length mismatch: len field = %u, actual = %u. Discarding packet.", len, length - 1);
      return;
    }

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