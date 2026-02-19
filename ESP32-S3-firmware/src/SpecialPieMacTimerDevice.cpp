#include "SpecialPieMacTimerDevice.h"
#include "Logger.h"
#include "common.h"
#include <cctype>
#include <string>

// Static constants
const char* SpecialPieMacTimerDevice::LOG_TAG = "SP-M1A2-F";
const char* SpecialPieMacTimerDevice::SERVICE_UUID = "0000FFF0-0000-1000-8000-00805F9B34FB";
const char* SpecialPieMacTimerDevice::CHARACTERISTIC_UUID = "0000FFF1-0000-1000-8000-00805F9B34FB";

// Static instance for callbacks
SpecialPieMacTimerDevice* SpecialPieMacTimerDevice::instance = nullptr;

SpecialPieMacTimerDevice::SpecialPieMacTimerDevice() :
  BaseTimerDevice("SP M1A2 Timer"),
  pNotifyCharacteristic(nullptr),
  previousTimeSeconds(0),
  previousTimeCentiseconds(0),
  hasPreviousShot(false),
  currentSessionId(0),
  sessionActiveFlag(false) {
  instance = this;
}

SpecialPieMacTimerDevice::~SpecialPieMacTimerDevice() {
  disconnect();
  instance = nullptr;
}

// Static method to check if advertised device matches SP M1A2 Timer name pattern
bool SpecialPieMacTimerDevice::matchesDevice(BLEAdvertisedDevice* device) {
  if (!device || !device->haveName()) {
    return false;
  }

  std::string name = device->getName().c_str();
  // Validate pattern: "SP M1A2 Timer " prefix + exactly 4 alphanumeric characters
  // Example: "SP M1A2 Timer 2196"
  static const char* PREFIX = "SP M1A2 Timer ";
  static const size_t PREFIX_LEN = 14;
  static const size_t SUFFIX_LEN = 4;
  if (name.size() != PREFIX_LEN + SUFFIX_LEN) return false;
  if (name.substr(0, PREFIX_LEN) != PREFIX) return false;
  for (size_t i = PREFIX_LEN; i < name.size(); i++) {
    if (!isalnum((unsigned char)name[i])) return false;
  }
  return true;
}

bool SpecialPieMacTimerDevice::attemptConnection(BLEAdvertisedDevice* device) {
  if (!device) return false;

  if (device->haveName()) {
    LOG_INFO(LOG_TAG, "Timer found: %s (%s)",
         device->getName().c_str(),
         device->getAddress().toString().c_str());

    // Store device name for display
    strncpy(deviceName, device->getName().c_str(), sizeof(deviceName)-1);
    deviceName[sizeof(deviceName)-1] = '\0';
  } else {
    LOG_INFO(LOG_TAG, "Timer found: %s", device->getAddress().toString().c_str());
    strncpy(deviceName, device->getAddress().toString().c_str(), sizeof(deviceName)-1);
    deviceName[sizeof(deviceName)-1] = '\0';
  }

  // Connect using the advertised device's MAC address
  return connectToMacAddress(device->getAddress().toString().c_str());
}

bool SpecialPieMacTimerDevice::connectToMacAddress(const char* macAddress) {
  LOG_INFO(LOG_TAG, "Connecting to %s", macAddress);

  // Disconnect any existing connection before attempting new one
  disconnect();
  pNotifyCharacteristic = nullptr;
  pService = nullptr;
  isConnectedFlag = false;

  setConnectionState(DeviceConnectionState::CONNECTING);

  // Create BLE address from MAC string
  BLEAddress bleAddress(macAddress);
  deviceAddress = bleAddress;

  // Brief delay before connection
  delay(BLE_CONNECTION_DELAY_MS);

  pClient = BLEDevice::createClient();
  if (!pClient) {
    LOG_ERROR(LOG_TAG, "Failed to create BLE client");
    setConnectionState(DeviceConnectionState::ERROR);
    return false;
  }

  LOG_INFO(LOG_TAG, "Attempting connection...");
  if (pClient->connect(bleAddress)) {
    LOG_INFO(LOG_TAG, "Connected to device!");

    // Get timer service
    BLEUUID serviceUuid(SERVICE_UUID);
    pService = pClient->getService(serviceUuid);

    if (pService != nullptr) {
      LOG_INFO(LOG_TAG, "Timer Service (FFF0) found");

      // Get notification characteristic
      pNotifyCharacteristic = pService->getCharacteristic(CHARACTERISTIC_UUID);

      if (pNotifyCharacteristic != nullptr) {
        LOG_INFO(LOG_TAG, "FFF1 (timer events) characteristic found");

        if (pNotifyCharacteristic->canNotify()) {
          LOG_INFO(LOG_TAG, "Registering for notifications...");
          pNotifyCharacteristic->registerForNotify(notifyCallback);
          LOG_INFO(LOG_TAG, "Successfully registered for timer event notifications!");

          isConnectedFlag = true;
          lastHeartbeat = millis();
          setConnectionState(DeviceConnectionState::CONNECTED);

          if (deviceName[0] == '\0') {
            strncpy(deviceName, macAddress, sizeof(deviceName)-1);
            deviceName[sizeof(deviceName)-1] = '\0';
          }

          return true;
        } else {
          LOG_ERROR(LOG_TAG, "FFF1 characteristic cannot notify");
          pClient->disconnect();
          delete pClient;
          pClient = nullptr;
          setConnectionState(DeviceConnectionState::ERROR);
          return false;
        }
      } else {
        LOG_ERROR(LOG_TAG, "FFF1 characteristic not found");
        pClient->disconnect();
        delete pClient;
        pClient = nullptr;
        setConnectionState(DeviceConnectionState::ERROR);
        return false;
      }
    } else {
      LOG_ERROR(LOG_TAG, "Timer service not found");
      pClient->disconnect();
      delete pClient;
      pClient = nullptr;
      setConnectionState(DeviceConnectionState::ERROR);
      return false;
    }
  } else {
    LOG_ERROR(LOG_TAG, "Failed to connect to device");
    delete pClient;
    pClient = nullptr;
    setConnectionState(DeviceConnectionState::ERROR);
  }

  return false;
}

// Static notification callback
void SpecialPieMacTimerDevice::notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                              uint8_t* pData, size_t length, bool isNotify) {
  if (instance && pData && length > 0) {
    instance->processTimerData(pData, length);
  }
}

void SpecialPieMacTimerDevice::processTimerData(uint8_t* pData, size_t length) {
  if (!pData || length == 0) {
    LOG_WARN(LOG_TAG, "Invalid data received");
    return;
  }

  if (Logger::getLevel() <= LogLevel::DEBUG) {
    LOG_DEBUG(LOG_TAG, "Notification (%d bytes)", length);
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
    LOG_WARN(LOG_TAG, "Invalid frame markers");
    return;
  }

  uint8_t messageType = pData[2];

  switch (messageType) {
    case static_cast<uint8_t>(SpecialPieMacMessageType::SESSION_START): {
      LOG_INFO(LOG_TAG, "SESSION_START");
      if (length >= 6) {
        currentSessionId = pData[3];
        LOG_INFO(LOG_TAG, "  Session ID: 0x%02X", currentSessionId);

        sessionActiveFlag = true;
        hasPreviousShot = false;
        previousTimeSeconds = 0;
        previousTimeCentiseconds = 0;

        // Notify callback
        if (sessionStartedCallback) {
          currentSession.sessionId = currentSessionId;
          currentSession.isActive = true;
          currentSession.totalShots = 0;
          currentSession.startTimestamp = millis();
          currentSession.startDelaySeconds = 0.0f;
          sessionStartedCallback(currentSession);
        }

        // Device has no separate countdown; signal ready immediately
        if (countdownCompleteCallback) {
          countdownCompleteCallback(currentSession);
        }
      }
      break;
    }

    case static_cast<uint8_t>(SpecialPieMacMessageType::SESSION_STOP): {
      LOG_INFO(LOG_TAG, "SESSION_STOP");
      if (length >= 6) {
        uint8_t sessionId = pData[3];
        LOG_INFO(LOG_TAG, "  Session ID: 0x%02X", sessionId);

        sessionActiveFlag = false;
        hasPreviousShot = false;

        // Notify callback
        if (sessionStoppedCallback) {
          currentSession.isActive = false;
          sessionStoppedCallback(currentSession);
        }
      }
      break;
    }

    case static_cast<uint8_t>(SpecialPieMacMessageType::SHOT_DETECTED): {
      LOG_INFO(LOG_TAG, "SHOT_DETECTED");
      if (length >= 10) {
        // Protocol format: F8 F9 36 00 [SEC] [CS] [SHOT#] [CHECKSUM?] F9 F8
        uint32_t currentSeconds = pData[4];
        uint32_t currentCentiseconds = pData[5];
        uint8_t shotNumber = pData[6];

        LOG_INFO(LOG_TAG, "  Shot #%u: %u.%02u", shotNumber, currentSeconds, currentCentiseconds);

        // Convert to milliseconds (centiseconds * 10 = milliseconds)
        uint32_t absoluteTimeMs = (currentSeconds * 1000) + (currentCentiseconds * 10);

        // Calculate split time
        uint32_t splitTimeMs = 0;
        bool isFirstShot = !hasPreviousShot;

        if (hasPreviousShot) {
          uint32_t previousTimeMs = (previousTimeSeconds * 1000) + (previousTimeCentiseconds * 10);
          splitTimeMs = absoluteTimeMs - previousTimeMs;
          LOG_INFO(LOG_TAG, "  Split: %u ms", splitTimeMs);
        } else {
          LOG_INFO(LOG_TAG, "  First shot");
        }

        // Store current shot for next split calculation
        previousTimeSeconds = currentSeconds;
        previousTimeCentiseconds = currentCentiseconds;
        hasPreviousShot = true;

        // Notify callback
        if (shotDetectedCallback) {
          NormalizedShotData shotData;
          shotData.sessionId = currentSessionId;
          shotData.shotNumber = shotNumber + 1;  // Normalize to 1-based
          shotData.absoluteTimeMs = absoluteTimeMs;
          shotData.splitTimeMs = splitTimeMs;
          shotData.timestampMs = millis();
          strncpy(shotData.deviceModel, deviceModel, sizeof(shotData.deviceModel) - 1);
          shotData.deviceModel[sizeof(shotData.deviceModel) - 1] = '\0';
          shotData.isFirstShot = isFirstShot;

          shotDetectedCallback(shotData);
        }

        // Update session shot count
        // SpecialPieTimer use 0 index shot numbers
        currentSession.totalShots = shotNumber + 1;
      }
      break;
    }

    default:
      LOG_DEBUG(LOG_TAG, "Unknown message type: 0x%02X", messageType);
      break;
  }
}
