#include "SpecialPieTimerDevice.h"
#include "Logger.h"
#include "common.h"

// Static constants
const char* SpecialPieTimerDevice::SERVICE_UUID = "0000FFF0-0000-1000-8000-00805F9B34FB";
const char* SpecialPieTimerDevice::CHARACTERISTIC_UUID = "0000FFF1-0000-1000-8000-00805F9B34FB";

// Static instance for callbacks
SpecialPieTimerDevice* SpecialPieTimerDevice::instance = nullptr;

SpecialPieTimerDevice::SpecialPieTimerDevice() :
  BaseTimerDevice("Special Pie Timer"),
  pNotifyCharacteristic(nullptr),
  previousTimeSeconds(0),
  previousTimeCentiseconds(0),
  hasPreviousShot(false),
  currentSessionId(0),
  sessionActiveFlag(false) {
  instance = this;
}

SpecialPieTimerDevice::~SpecialPieTimerDevice() {
  disconnect();
  instance = nullptr;
}

const char* SpecialPieTimerDevice::getLogTag() const {
  return "SPECIAL-PIE";
}

bool SpecialPieTimerDevice::attemptConnection(BLEAdvertisedDevice* device) {
  if (!device) return false;

  if (device->haveName()) {
    LOG_BLE("Special Pie Timer found: %s (%s)",
            device->getName().c_str(),
            device->getAddress().toString().c_str());
  } else {
    LOG_BLE("Special Pie Timer found: %s", device->getAddress().toString().c_str());
  }

  // Store device info
  deviceAddress = device->getAddress();
  if (device->haveName()) {
    deviceName = device->getName().c_str();
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
    LOG_ERROR("SPECIAL-PIE", "Failed to create BLE client");
    setConnectionState(DeviceConnectionState::ERROR);
    return false;
  }

  LOG_BLE("Attempting connection");
  if (pClient->connect(device)) {
    LOG_BLE("Connected to device");
    BLEUUID serviceUuid(SERVICE_UUID);
    pService = pClient->getService(serviceUuid);

    if (pService != nullptr) {
      LOG_BLE("Special Pie Timer service found");

      pNotifyCharacteristic = pService->getCharacteristic(CHARACTERISTIC_UUID);

      if (pNotifyCharacteristic != nullptr) {
        LOG_BLE("FFF1 characteristic found");

        // Check if characteristic can notify
        if (pNotifyCharacteristic->canNotify()) {
          LOG_BLE("Registering for notifications");
          pNotifyCharacteristic->registerForNotify(notifyCallback);
          LOG_BLE("Successfully registered for notifications - listening for events");
          isConnectedFlag = true;
          lastHeartbeat = millis();
          setConnectionState(DeviceConnectionState::CONNECTED);
          return true;
        } else {
          LOG_ERROR("SPECIAL-PIE", "Characteristic cannot notify");
          pClient->disconnect();
          delete pClient;
          pClient = nullptr;
          setConnectionState(DeviceConnectionState::ERROR);
        }
      } else {
        LOG_ERROR("SPECIAL-PIE", "FFF1 characteristic not found");
        pClient->disconnect();
        delete pClient;
        pClient = nullptr;
        setConnectionState(DeviceConnectionState::ERROR);
      }
    } else {
      LOG_ERROR("SPECIAL-PIE", "Service not found");
      pClient->disconnect();
      delete pClient;
      pClient = nullptr;
      setConnectionState(DeviceConnectionState::ERROR);
    }
  } else {
    LOG_ERROR("SPECIAL-PIE", "Failed to connect");
    delete pClient;
    pClient = nullptr;
    setConnectionState(DeviceConnectionState::ERROR);
  }

  return false;
}

// Static notification callback
void SpecialPieTimerDevice::notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                          uint8_t* pData, size_t length, bool isNotify) {
  if (instance && pData && length > 0) {
    instance->processTimerData(pData, length);
  }
}

void SpecialPieTimerDevice::processTimerData(uint8_t* pData, size_t length) {
  if (!pData || length == 0) {
    LOG_WARN("SPECIAL-PIE", "Invalid data received (null or empty)");
    return;
  }

  if (Logger::getLevel() <= LogLevel::DEBUG) {
    LOG_DEBUG("SPECIAL-PIE", "Notification received (%d bytes)", length);
    for (size_t i = 0; i < length; i++) {
      Serial.printf("%02X ", pData[i]);
    }
    Serial.println();
  }

  // Special Pie Timer Protocol:
  // [F8] [F9] [MESSAGE_TYPE] [DATA...] [F9] [F8]

  // Validate frame markers
  if (length < 6 || pData[0] != 0xF8 || pData[1] != 0xF9 ||
      pData[length - 2] != 0xF9 || pData[length - 1] != 0xF8) {
    LOG_WARN("SPECIAL-PIE", "Invalid frame markers");
    return;
  }

  SpecialPieMessageType messageType = static_cast<SpecialPieMessageType>(pData[2]);

  switch (messageType) {
    case SpecialPieMessageType::SESSION_START:
      if (length >= 6) {
        currentSessionId = pData[3];
        LOG_TIMER("SESSION_START - ID: 0x%02X", currentSessionId);

        // Update session state
        currentSession.sessionId = currentSessionId;
        currentSession.isActive = true;
        currentSession.totalShots = 0;
        currentSession.startTimestamp = millis();
        currentSession.startDelaySeconds = 0.0f;  // Special Pie doesn't report start delay

        sessionActiveFlag = true;
        hasPreviousShot = false;
        previousTimeSeconds = 0;
        previousTimeCentiseconds = 0;

        // Notify callbacks
        if (sessionStartedCallback) {
          sessionStartedCallback(currentSession);
        }

        // Special Pie doesn't have a separate countdown - immediately signal ready
        if (countdownCompleteCallback) {
          countdownCompleteCallback(currentSession);
        }
      }
      break;

    case SpecialPieMessageType::SESSION_STOP:
      if (length >= 6) {
        uint8_t sessionId = pData[3];
        LOG_TIMER("SESSION_STOP - ID: 0x%02X", sessionId);

        currentSession.isActive = false;
        sessionActiveFlag = false;
        hasPreviousShot = false;

        if (sessionStoppedCallback) {
          sessionStoppedCallback(currentSession);
        }
      }
      break;

    case SpecialPieMessageType::SHOT_DETECTED:
      if (length >= 10) {
        // Protocol format: F8 F9 36 00 [SEC] [CS] [SHOT#] [CHECKSUM?] F9 F8
        // Byte 4: Seconds
        // Byte 5: Centiseconds (0-99)
        // Byte 6: Shot number

        uint32_t currentSeconds = pData[4];
        uint32_t currentCentiseconds = pData[5];
        uint8_t shotNumber = pData[6];

        LOG_DEBUG("SPECIAL-PIE", "SHOT_DETECTED #%u: %u.%02u", shotNumber, currentSeconds, currentCentiseconds);

        // Calculate split time if we have a previous shot
        uint32_t splitTimeMs = 0;
        bool isFirstShot = !hasPreviousShot;

        if (hasPreviousShot) {
          int32_t deltaSeconds = currentSeconds - previousTimeSeconds;
          int32_t deltaCentiseconds = currentCentiseconds - previousTimeCentiseconds;

          // Handle negative centiseconds (borrow from seconds)
          if (deltaCentiseconds < 0) {
            deltaSeconds -= 1;
            deltaCentiseconds += 100;
          }

          // Convert to milliseconds (centiseconds = 10ms)
          splitTimeMs = (deltaSeconds * 1000) + (deltaCentiseconds * 10);

          LOG_DEBUG("SPECIAL-PIE", "Split: %d.%02d", deltaSeconds, deltaCentiseconds);
        }

        // Store current shot as previous for next split calculation
        previousTimeSeconds = currentSeconds;
        previousTimeCentiseconds = currentCentiseconds;
        hasPreviousShot = true;

        // Convert to milliseconds for absolute time
        uint32_t absoluteTimeMs = (currentSeconds * 1000) + (currentCentiseconds * 10);

        // Update session shot count
        currentSession.totalShots = shotNumber + 1;

        // Create normalized shot data
        NormalizedShotData shotData;
        shotData.sessionId = currentSessionId;
        shotData.shotNumber = shotNumber;
        shotData.absoluteTimeMs = absoluteTimeMs;
        shotData.splitTimeMs = splitTimeMs;
        shotData.timestampMs = millis();
        shotData.deviceModel = deviceModel.c_str();
        shotData.isFirstShot = isFirstShot;

        // Notify callback
        if (shotDetectedCallback) {
          shotDetectedCallback(shotData);
        }
      }
      break;

    default:
      LOG_WARN("SPECIAL-PIE", "Unknown message type: 0x%02X", static_cast<uint8_t>(messageType));
      break;
  }
}
