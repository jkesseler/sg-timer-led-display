#include "SGTimer.h"
#include "Logger.h"
#include "common.h"

// Static constants - Service UUIDs for device discovery
const char* SGTimer::LOG_TAG = "SG-TIMER";
const char* SGTimer::SERVICE_UUID = "7520FFFF-14D2-4CDA-8B6B-697C554C9311";
const char* SGTimer::CHARACTERISTIC_UUID = "75200001-14D2-4CDA-8B6B-697C554C9311";
const char* SGTimer::SHOT_LIST_UUID = "75200004-14D2-4CDA-8B6B-697C554C9311";

// Static instance for callbacks
SGTimer* SGTimer::instance = nullptr;

SGTimer::SGTimer() :
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

SGTimer::~SGTimer() {
  disconnect();
  instance = nullptr;
}

// Static method to check if advertised device is an SG Timer
bool SGTimer::matchesDevice(BLEAdvertisedDevice* device) {
  if (!device || !device->haveServiceUUID()) {
    return false;
  }

  BLEUUID serviceUuid(SERVICE_UUID);
  return device->isAdvertisingService(serviceUuid);
}

// Connect to the already-discovered SG Timer device
bool SGTimer::attemptConnection(BLEAdvertisedDevice* device) {
  if (!device) {
    LOG_ERROR(LOG_TAG, "Null device pointer passed to attemptConnection");
    setConnectionState(DeviceConnectionState::ERROR);
    return false;
  }

  if (device->haveName()) {
    LOG_INFO(LOG_TAG, "SG Timer found: %s (%s)", device->getAddress().toString().c_str(), device->getName().c_str());
  } else {
    LOG_INFO(LOG_TAG, "SG Timer found: %s", device->getAddress().toString().c_str());
  }

  // Store device name/address, then refine the model from the SG naming scheme
  // (SG-SST4X... where X identifies the model variant).
  storeDeviceInfo(device);
  if (device->haveName() && strncmp(deviceName, "SG-SST4", 7) == 0 && strlen(deviceName) > 7) {
    char modelId = deviceName[7];
    if (modelId == 'A') {
      strncpy(deviceModel, "SG Timer Sport", sizeof(deviceModel)-1);
    } else if (modelId == 'B') {
      strncpy(deviceModel, "SG Timer GO", sizeof(deviceModel)-1);
    } else {
      strncpy(deviceModel, "SG Timer", sizeof(deviceModel)-1);
    }
    deviceModel[sizeof(deviceModel)-1] = '\0';
  }

  return connectAndSubscribe(LOG_TAG, device, SERVICE_UUID, CHARACTERISTIC_UUID,
                             notifyCallback, &pEventCharacteristic);
}

// Static notification callback
void SGTimer::notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                  uint8_t* pData, size_t length, bool isNotify) {
  if (instance && pData && length > 0) {
    instance->processTimerData(pData, length);
  }
}

void SGTimer::processTimerData(uint8_t* pData, size_t length) {
  if (!pData || length == 0) {
    LOG_WARN(LOG_TAG, "Invalid data received (null or empty)");
    return;
  }

  logNotificationBytes(LOG_TAG, pData, length);

  // Parse event based on API documentation
  if (length >= 2) {
    // Validate packet length field (len = number of bytes after length byte)
    uint8_t len = pData[0];
    if (len != length - 1) {
      LOG_ERROR(LOG_TAG, "Length mismatch: len field = %u, actual = %u. Discarding packet.", len, length - 1);
      return;
    }

    SGTimerEvent event_id = static_cast<SGTimerEvent>(pData[1]);

    switch (event_id) {
      case SGTimerEvent::SESSION_STARTED:
        if (length >= 8) {
          uint32_t sess_id = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
          uint16_t start_delay = (pData[6] << 8) | pData[7];
          LOG_INFO(LOG_TAG, "SESSION_STARTED - ID: %u, Delay: %.1fs", sess_id, start_delay * 0.1);

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
          LOG_INFO(LOG_TAG, "SESSION_SUSPENDED - ID: %u, Total shots: %u", sess_id, total_shots);

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
          LOG_INFO(LOG_TAG, "SESSION_RESUMED - ID: %u, Total shots: %u", sess_id, total_shots);

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
            LOG_INFO(LOG_TAG, "SESSION_STOPPED - ID: %u, Total shots: %u, Last: #%u at %u:%02u",
                     sess_id, total_shots, lastShotNum + 1, lastShotSeconds, lastShotHundredths);
          } else {
            LOG_INFO(LOG_TAG, "SESSION_STOPPED - ID: %u, Total shots: %u", sess_id, total_shots);
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

          LOG_DEBUG(LOG_TAG, "SHOT_DETECTED #%u: %u:%02u", shot_num + 1, seconds, hundredths);

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
          strncpy(shotData.deviceModel, deviceModel, sizeof(shotData.deviceModel) - 1);
          shotData.deviceModel[sizeof(shotData.deviceModel) - 1] = '\0';
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
          LOG_INFO(LOG_TAG, "SESSION_SET_BEGIN - ID: %u (countdown complete)", sess_id);

          // Notify callback that countdown has completed
          if (countdownCompleteCallback) {
            countdownCompleteCallback(currentSession);
          }
        }
        break;

      default:
        LOG_WARN(LOG_TAG, "Unknown event ID: 0x%02X", static_cast<uint8_t>(event_id));
        break;
    }
  }
}