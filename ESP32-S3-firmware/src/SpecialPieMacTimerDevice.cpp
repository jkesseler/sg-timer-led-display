#include "SpecialPieMacTimerDevice.h"
#include "Logger.h"
#include "common.h"

// Static constants
const char* SpecialPieMacTimerDevice::SERVICE_UUID = "0000FFF0-0000-1000-8000-00805F9B34FB";
const char* SpecialPieMacTimerDevice::CHARACTERISTIC_UUID = "0000FFF1-0000-1000-8000-00805F9B34FB";
const char* SpecialPieMacTimerDevice::DEVICE_INFO_SERVICE_UUID = "0917FE11-5D37-816D-8000-00805F9B34FB";
const char* SpecialPieMacTimerDevice::FIRMWARE_CHAR_UUID = "09170002-5D37-816D-8000-00805F9B34FB";

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

const char* SpecialPieMacTimerDevice::getLogTag() const {
  return "SP-MAC";
}

// Static method to check if advertised device matches SP M1A2 Timer name pattern
bool SpecialPieMacTimerDevice::matchesDevice(BLEAdvertisedDevice* device) {
  if (!device || !device->haveName()) {
    return false;
  }

  String deviceName = device->getName().c_str();
  // LLM: Rather string.startsWith use string.Matcjhes with regex "SP M1A2 Timer [0-9A-Fa-f]{4}"
  // Match pattern "SP M1A2 Timer <xxxx>" where <xxxx> is a 4-character identifier
  // Example: "SP M1A2 Timer 2196"
  return deviceName.startsWith("SP M1A2 Timer ");
}

bool SpecialPieMacTimerDevice::attemptConnection(BLEAdvertisedDevice* device) {
  if (!device) return false;

  if (device->haveName()) {
    LOG_BLE("SP M1A2 Timer found: %s (%s)",
            device->getName().c_str(),
            device->getAddress().toString().c_str());

    // Store device name for display
    deviceName = device->getName().c_str();
  } else {
    LOG_BLE("SP M1A2 Timer found: %s", device->getAddress().toString().c_str());
    deviceName = device->getAddress().toString().c_str();
  }

  // Connect using the advertised device's MAC address
  return connectToMacAddress(device->getAddress().toString().c_str());
}

bool SpecialPieMacTimerDevice::connectToMacAddress(const char* macAddress) {
  LOG_BLE("Connecting to %s", macAddress);

  setConnectionState(DeviceConnectionState::CONNECTING);

  // Create BLE address from MAC string
  BLEAddress bleAddress(macAddress);
  deviceAddress = bleAddress;

  // Brief delay before connection
  delay(BLE_CONNECTION_DELAY_MS);

  pClient = BLEDevice::createClient();
  if (!pClient) {
    LOG_ERROR("SP-MAC", "Failed to create BLE client");
    setConnectionState(DeviceConnectionState::ERROR);
    return false;
  }

  LOG_BLE("Attempting connection...");
  if (pClient->connect(bleAddress)) {
    LOG_BLE("Connected to device!");

    // Get timer service
    BLEUUID serviceUuid(SERVICE_UUID);
    pService = pClient->getService(serviceUuid);

    if (pService != nullptr) {
      LOG_BLE("Timer Service (FFF0) found");

      // Try to read firmware version
      BLEUUID deviceInfoUuid(DEVICE_INFO_SERVICE_UUID);
      BLERemoteService* pDeviceInfoService = pClient->getService(deviceInfoUuid);
      if (pDeviceInfoService != nullptr) {
        LOG_BLE("Device Info Service found");
        BLERemoteCharacteristic* pFirmwareChar = pDeviceInfoService->getCharacteristic(FIRMWARE_CHAR_UUID);
        if (pFirmwareChar != nullptr && pFirmwareChar->canRead()) {
          std::string firmwareValue = pFirmwareChar->readValue();
          if (firmwareValue.length() > 0) {
            LOG_BLE("Firmware: %s", firmwareValue.c_str());
            deviceName = "SP M1A2+ FW:" + String(firmwareValue.c_str());
          }
        }
      }

      // Get notification characteristic
      pNotifyCharacteristic = pService->getCharacteristic(CHARACTERISTIC_UUID);

      if (pNotifyCharacteristic != nullptr) {
        LOG_BLE("FFF1 (timer events) characteristic found");

        if (pNotifyCharacteristic->canNotify()) {
          LOG_BLE("Registering for notifications...");
          pNotifyCharacteristic->registerForNotify(notifyCallback);
          LOG_BLE("Successfully registered for timer event notifications!");

          isConnectedFlag = true;
          lastHeartbeat = millis();
          setConnectionState(DeviceConnectionState::CONNECTED);

          if (deviceName.length() == 0) {
            deviceName = String(macAddress);
          }

          return true;
        } else {
          LOG_ERROR("SP-MAC", "FFF1 characteristic cannot notify");
          pClient->disconnect();
          delete pClient;
          pClient = nullptr;
          setConnectionState(DeviceConnectionState::ERROR);
        }
      } else {
        LOG_ERROR("SP-MAC", "FFF1 characteristic not found");
        pClient->disconnect();
        delete pClient;
        pClient = nullptr;
        setConnectionState(DeviceConnectionState::ERROR);
      }
    } else {
      LOG_ERROR("SP-MAC", "Timer service not found");
      pClient->disconnect();
      delete pClient;
      pClient = nullptr;
      setConnectionState(DeviceConnectionState::ERROR);
    }
  } else {
    LOG_ERROR("SP-MAC", "Failed to connect to device");
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
    LOG_WARN("SP-MAC", "Invalid data received");
    return;
  }

  if (Logger::getLevel() <= LogLevel::DEBUG) {
    LOG_DEBUG("SP-MAC", "Notification (%d bytes)", length);
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
    LOG_WARN("SP-MAC", "Invalid frame markers");
    return;
  }

  uint8_t messageType = pData[2];

  switch (messageType) {
    case static_cast<uint8_t>(SpecialPieMacMessageType::SESSION_START): {
      LOG_TIMER("SESSION_START");
      if (length >= 6) {
        currentSessionId = pData[3];
        LOG_TIMER("  Session ID: 0x%02X", currentSessionId);

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
      }
      break;
    }

    case static_cast<uint8_t>(SpecialPieMacMessageType::SESSION_STOP): {
      LOG_TIMER("SESSION_STOP");
      if (length >= 6) {
        uint8_t sessionId = pData[3];
        LOG_TIMER("  Session ID: 0x%02X", sessionId);

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
      LOG_TIMER("SHOT_DETECTED");
      if (length >= 10) {
        // Protocol format: F8 F9 36 00 [SEC] [CS] [SHOT#] [CHECKSUM?] F9 F8
        uint32_t currentSeconds = pData[4];
        uint32_t currentCentiseconds = pData[5];
        uint8_t shotNumber = pData[6];

        LOG_TIMER("  Shot #%u: %u.%02u", shotNumber, currentSeconds, currentCentiseconds);

        // Convert to milliseconds (centiseconds * 10 = milliseconds)
        uint32_t absoluteTimeMs = (currentSeconds * 1000) + (currentCentiseconds * 10);

        // Calculate split time
        uint32_t splitTimeMs = 0;
        bool isFirstShot = !hasPreviousShot;

        if (hasPreviousShot) {
          uint32_t previousTimeMs = (previousTimeSeconds * 1000) + (previousTimeCentiseconds * 10);
          splitTimeMs = absoluteTimeMs - previousTimeMs;
          LOG_TIMER("  Split: %u ms", splitTimeMs);
        } else {
          LOG_TIMER("  First shot");
        }

        // Store current shot for next split calculation
        previousTimeSeconds = currentSeconds;
        previousTimeCentiseconds = currentCentiseconds;
        hasPreviousShot = true;

        // Notify callback
        if (shotDetectedCallback) {
          NormalizedShotData shotData;
          shotData.sessionId = currentSessionId;
          shotData.shotNumber = shotNumber;
          shotData.absoluteTimeMs = absoluteTimeMs;
          shotData.splitTimeMs = splitTimeMs;
          shotData.timestampMs = millis();
          shotData.deviceModel = deviceModel.c_str();
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
      LOG_DEBUG("SP-MAC", "Unknown message type: 0x%02X", messageType);
      break;
  }
}
