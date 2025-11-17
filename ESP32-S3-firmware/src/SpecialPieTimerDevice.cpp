#include "SpecialPieTimerDevice.h"
#include "Logger.h"
#include "common.h"

// Static constants
const char* SpecialPieTimerDevice::SERVICE_UUID = "0000FFF0-0000-1000-8000-00805F9B34FB";
const char* SpecialPieTimerDevice::CHARACTERISTIC_UUID = "0000FFF1-0000-1000-8000-00805F9B34FB";

// Static instance for callbacks
SpecialPieTimerDevice* SpecialPieTimerDevice::instance = nullptr;

SpecialPieTimerDevice::SpecialPieTimerDevice() :
  pClient(nullptr),
  pNotifyCharacteristic(nullptr),
  pService(nullptr),
  connectionState(DeviceConnectionState::DISCONNECTED),
  isConnectedFlag(false),
  lastReconnectAttempt(0),
  lastHeartbeat(0),
  deviceAddress("00:00:00:00:00:00"),
  deviceName(""),
  deviceModel("Special Pie Timer"),
  currentSessionId(0),
  sessionActiveFlag(false),
  previousTimeSeconds(0),
  previousTimeCentiseconds(0),
  hasPreviousShot(false)
{
  instance = this;
  currentSession = {};
}

SpecialPieTimerDevice::~SpecialPieTimerDevice() {
  disconnect();
  instance = nullptr;
}

bool SpecialPieTimerDevice::initialize() {
  LOG_INFO("SPECIAL-PIE", "Initializing Special Pie Timer device interface");
  setConnectionState(DeviceConnectionState::DISCONNECTED);
  return true;
}

bool SpecialPieTimerDevice::startScanning() {
  LOG_INFO("SPECIAL-PIE", "Will start scanning for Special Pie Timer devices");
  setConnectionState(DeviceConnectionState::SCANNING);
  return true;
}

bool SpecialPieTimerDevice::connect(BLEAddress address) {
  // Store the target address
  deviceAddress = address;
  return true;
}

void SpecialPieTimerDevice::disconnect() {
  if (pClient && isConnectedFlag) {
    pClient->disconnect();
    delete pClient;
    pClient = nullptr;
  }
  isConnectedFlag = false;
  pNotifyCharacteristic = nullptr;
  pService = nullptr;
  setConnectionState(DeviceConnectionState::DISCONNECTED);
}

DeviceConnectionState SpecialPieTimerDevice::getConnectionState() const {
  return connectionState;
}

bool SpecialPieTimerDevice::isConnected() const {
  return isConnectedFlag;
}

const char* SpecialPieTimerDevice::getDeviceModel() const {
  return deviceModel.c_str();
}

const char* SpecialPieTimerDevice::getDeviceName() const {
  return deviceName.c_str();
}

BLEAddress SpecialPieTimerDevice::getDeviceAddress() const {
  return deviceAddress;
}

void SpecialPieTimerDevice::onShotDetected(std::function<void(const NormalizedShotData&)> callback) {
  shotDetectedCallback = callback;
}

void SpecialPieTimerDevice::onSessionStarted(std::function<void(const SessionData&)> callback) {
  sessionStartedCallback = callback;
}

void SpecialPieTimerDevice::onSessionStopped(std::function<void(const SessionData&)> callback) {
  sessionStoppedCallback = callback;
}

void SpecialPieTimerDevice::onSessionSuspended(std::function<void(const SessionData&)> callback) {
  sessionSuspendedCallback = callback;
}

void SpecialPieTimerDevice::onSessionResumed(std::function<void(const SessionData&)> callback) {
  sessionResumedCallback = callback;
}

void SpecialPieTimerDevice::onConnectionStateChanged(std::function<void(DeviceConnectionState)> callback) {
  connectionStateCallback = callback;
}

bool SpecialPieTimerDevice::supportsRemoteStart() const {
  return false;
}

bool SpecialPieTimerDevice::supportsShotList() const {
  return false;
}

bool SpecialPieTimerDevice::supportsSessionControl() const {
  return false;
}

bool SpecialPieTimerDevice::requestShotList(uint32_t sessionId) {
  return false;
}

bool SpecialPieTimerDevice::startSession() {
  return false;
}

bool SpecialPieTimerDevice::stopSession() {
  return false;
}

// Static helper to check if a device is a Special Pie Timer
bool SpecialPieTimerDevice::isSpecialPieTimer(BLEAdvertisedDevice* device) {
  if (!device) return false;

  BLEUUID serviceUuid(SERVICE_UUID);
  return device->haveServiceUUID() && device->isAdvertisingService(serviceUuid);
}

// Main update loop
void SpecialPieTimerDevice::update() {
  // If connected, check connection status
  if (isConnectedFlag) {
    if (pClient && pClient->isConnected()) {
      // Print heartbeat every 30 seconds
      if (millis() - lastHeartbeat > 30000) {
        Serial.printf("[SPECIAL-PIE] Connected - waiting for events...\n");
        lastHeartbeat = millis();
      }
    } else {
      // Connection lost
      Serial.println("\n!!! Connection lost !!!");
      isConnectedFlag = false;
      pService = nullptr;
      pNotifyCharacteristic = nullptr;
      sessionActiveFlag = false;
      hasPreviousShot = false;

      if (pClient) {
        delete pClient;
        pClient = nullptr;
      }

      setConnectionState(DeviceConnectionState::DISCONNECTED);

      // Reset session tracking
      currentSession = {};

      Serial.println("Will attempt to reconnect...\n");
    }
  }
  // Note: Scanning is handled by TimerApplication for multi-device support
}

bool SpecialPieTimerDevice::attemptConnection(BLEAdvertisedDevice* device) {
  if (!device) return false;

  Serial.printf("Special Pie Timer found: %s (%s)\n",
                device->getName().c_str(),
                device->getAddress().toString().c_str());

  // Store device info
  deviceAddress = device->getAddress();
  deviceName = device->getName().c_str();

  // Wait before attempting connection
  Serial.println("Waiting 2 seconds before connecting...");
  delay(2000);

  setConnectionState(DeviceConnectionState::CONNECTING);
  pClient = BLEDevice::createClient();

  if (!pClient) {
    Serial.println("ERROR: Failed to create client");
    setConnectionState(DeviceConnectionState::ERROR);
    return false;
  }

  Serial.println("Attempting connection...");
  if (pClient->connect(device)) {
    Serial.println("Connected to device!");
    BLEUUID serviceUuid(SERVICE_UUID);
    pService = pClient->getService(serviceUuid);

    if (pService != nullptr) {
      Serial.println("Service found");

      pNotifyCharacteristic = pService->getCharacteristic(CHARACTERISTIC_UUID);

      if (pNotifyCharacteristic != nullptr) {
        Serial.println("FFF1 characteristic found");

        // Check if characteristic can notify
        if (pNotifyCharacteristic->canNotify()) {
          Serial.println("Registering for notifications...");
          pNotifyCharacteristic->registerForNotify(notifyCallback);
          Serial.println("SUCCESS: Registered for notifications!");
          Serial.println("Listening for events indefinitely...\n");
          isConnectedFlag = true;
          lastHeartbeat = millis();
          setConnectionState(DeviceConnectionState::CONNECTED);
          return true;
        } else {
          Serial.println("ERROR: Characteristic cannot notify");
          pClient->disconnect();
          delete pClient;
          pClient = nullptr;
          setConnectionState(DeviceConnectionState::ERROR);
        }
      } else {
        Serial.println("ERROR: FFF1 characteristic not found");
        pClient->disconnect();
        delete pClient;
        pClient = nullptr;
        setConnectionState(DeviceConnectionState::ERROR);
      }
    } else {
      Serial.println("ERROR: Service not found");
      pClient->disconnect();
      delete pClient;
      pClient = nullptr;
      setConnectionState(DeviceConnectionState::ERROR);
    }
  } else {
    Serial.println("ERROR: Failed to connect");
    delete pClient;
    pClient = nullptr;
    setConnectionState(DeviceConnectionState::ERROR);
  }

  return false;
}

void SpecialPieTimerDevice::setConnectionState(DeviceConnectionState newState) {
  if (connectionState != newState) {
    connectionState = newState;
    if (connectionStateCallback) {
      connectionStateCallback(newState);
    }
  }
}

// Static notification callback
void SpecialPieTimerDevice::notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                          uint8_t* pData, size_t length, bool isNotify) {
  if (instance) {
    instance->processTimerData(pData, length);
  }
}

void SpecialPieTimerDevice::processTimerData(uint8_t* pData, size_t length) {
  Serial.printf("\n*** Notification received (%d bytes): ", length);
  for (size_t i = 0; i < length; i++) {
    Serial.printf("%02X ", pData[i]);
  }
  Serial.println();

  // Special Pie Timer Protocol:
  // [F8] [F9] [MESSAGE_TYPE] [DATA...] [F9] [F8]

  // Validate frame markers
  if (length < 6 || pData[0] != 0xF8 || pData[1] != 0xF9 ||
      pData[length - 2] != 0xF9 || pData[length - 1] != 0xF8) {
    Serial.println("WARNING: Invalid frame markers");
    return;
  }

  uint8_t messageType = pData[2];

  Serial.printf("Message Type: 0x%02X - ", messageType);

  switch (messageType) {
    case 0x34: // SESSION_START (52 decimal)
      Serial.println("SESSION_START");
      if (length >= 6) {
        currentSessionId = pData[3];
        Serial.printf("  Session ID: 0x%02X\n", currentSessionId);

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

        // Notify callback
        if (sessionStartedCallback) {
          sessionStartedCallback(currentSession);
        }
      }
      break;

    case 0x18: // SESSION_STOP (24 decimal)
      Serial.println("SESSION_STOP");
      if (length >= 6) {
        uint8_t sessionId = pData[3];
        Serial.printf("  Session ID: 0x%02X\n", sessionId);

        currentSession.isActive = false;
        sessionActiveFlag = false;
        hasPreviousShot = false;

        if (sessionStoppedCallback) {
          sessionStoppedCallback(currentSession);
        }
      }
      break;

    case 0x36: // SHOT_DETECTED (54 decimal)
      Serial.println("SHOT_DETECTED");
      if (length >= 10) {
        // Protocol format: F8 F9 36 00 [SEC] [CS] [SHOT#] [CHECKSUM?] F9 F8
        // Byte 4: Seconds
        // Byte 5: Centiseconds (0-99)
        // Byte 6: Shot number

        uint32_t currentSeconds = pData[4];
        uint32_t currentCentiseconds = pData[5];
        uint8_t shotNumber = pData[6];

        // Format time as seconds.centiseconds
        Serial.printf("  Shot #%u: %u.%02u\n", shotNumber, currentSeconds, currentCentiseconds);

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

          Serial.printf("  Split: %d.%02d\n", deltaSeconds, deltaCentiseconds);
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
      Serial.println("UNKNOWN");
      break;
  }
}
