#include "SGTimerDevice.h"
#include "Logger.h"
#include "common.h"

// Static constants - Target the known device address
const char* SGTimerDevice::TARGET_DEVICE_ADDRESS = "dd:0e:9d:04:72:c3";
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
  return TARGET_DEVICE_ADDRESS;
}

BLEAddress SGTimerDevice::getDeviceAddress() const {
  return BLEAddress(TARGET_DEVICE_ADDRESS);
}

void SGTimerDevice::onShotDetected(std::function<void(const NormalizedShotData&)> callback) {
  shotDetectedCallback = callback;
}

void SGTimerDevice::onSessionStarted(std::function<void(const SessionData&)> callback) {
  sessionStartedCallback = callback;
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

// Main update loop - simplified to match minimal_test.cpp pattern
void SGTimerDevice::update() {
  // If not connected, try to scan and connect
  if (!isConnectedFlag) {
    unsigned long now = millis();

    // Throttle reconnection attempts
    if (now - lastReconnectAttempt < 5000) {
      return;
    }

    lastReconnectAttempt = now;
    attemptConnection();
  } else {
    // Connected - check connection status and print heartbeat
    if (pClient && pClient->isConnected()) {
      // Print heartbeat every 30 seconds
      if (millis() - lastHeartbeat > 30000) {
        Serial.printf("[SG-TIMER] Connected - waiting for events...\n");
        lastHeartbeat = millis();
      }
    } else {
      // Connection lost
      Serial.println("\n!!! Connection lost !!!");
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

      Serial.println("Will attempt to reconnect...\n");
    }
  }
}

void SGTimerDevice::setConnectionState(DeviceConnectionState newState) {
  if (connectionState != newState) {
    connectionState = newState;
    if (connectionStateCallback) {
      connectionStateCallback(newState);
    }
  }
}

// Simplified connection attempt matching minimal_test.cpp
void SGTimerDevice::attemptConnection() {
  Serial.println("\n--- Starting device scan ---");
  setConnectionState(DeviceConnectionState::SCANNING);

  BLEScan* pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);

  Serial.println("Scanning for 10 seconds...");
  BLEScanResults foundDevices = pScan->start(10, false);

  BLEUUID serviceUuid(SERVICE_UUID);
  bool deviceFound = false;

  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);

    if (device.getAddress().toString() == TARGET_DEVICE_ADDRESS) {
      Serial.println("Target device found!");
      deviceFound = true;

      // Wait before attempting connection
      Serial.println("Waiting 2 seconds before connecting...");
      delay(2000);

      setConnectionState(DeviceConnectionState::CONNECTING);
      pClient = BLEDevice::createClient();

      if (!pClient) {
        Serial.println("ERROR: Failed to create client");
        setConnectionState(DeviceConnectionState::ERROR);
        break;
      }

      Serial.println("Attempting connection...");
      if (pClient->connect(&device)) {
        Serial.println("Connected to device!");
        pService = pClient->getService(serviceUuid);

        if (pService != nullptr) {
          Serial.println("Service found");

          pEventCharacteristic = pService->getCharacteristic(CHARACTERISTIC_UUID);

          if (pEventCharacteristic != nullptr) {
            Serial.println("EVENT characteristic found");

            // Check if characteristic can notify
            if (pEventCharacteristic->canNotify()) {
              Serial.println("Registering for notifications...");
              pEventCharacteristic->registerForNotify(notifyCallback);
              Serial.println("SUCCESS: Registered for notifications!");
              Serial.println("Listening for events indefinitely...\n");
              isConnectedFlag = true;
              lastHeartbeat = millis();
              setConnectionState(DeviceConnectionState::CONNECTED);
            } else {
              Serial.println("ERROR: Characteristic cannot notify");
              pClient->disconnect();
              delete pClient;
              pClient = nullptr;
              setConnectionState(DeviceConnectionState::ERROR);
            }
          } else {
            Serial.println("ERROR: EVENT characteristic not found");
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

      break;
    }
  }

  pScan->clearResults();

  if (!deviceFound) {
    Serial.println("Target device not found. Retrying in 5 seconds...");
    setConnectionState(DeviceConnectionState::DISCONNECTED);
  } else if (!isConnectedFlag) {
    Serial.println("Connection failed. Retrying in 5 seconds...");
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
  Serial.printf("\n*** Notification received (%d bytes): ", length);
  for (size_t i = 0; i < length; i++) {
    Serial.printf("%02X ", pData[i]);
  }
  Serial.println();

  // Parse event based on API documentation
  if (length >= 2) {
    uint8_t len = pData[0];
    uint8_t event_id = pData[1];

    Serial.printf("Event ID: 0x%02X - ", event_id);

    switch (event_id) {
      case 0x00: // SESSION_STARTED
        Serial.println("SESSION_STARTED");
        if (length >= 8) {
          uint32_t sess_id = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
          uint16_t start_delay = (pData[6] << 8) | pData[7];
          Serial.printf("  Session ID: %u\n", sess_id);
          Serial.printf("  Start Delay: %.1f seconds\n", start_delay * 0.1);

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

      case 0x01: // SESSION_SUSPENDED
        Serial.println("SESSION_SUSPENDED");
        if (length >= 8) {
          uint32_t sess_id = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
          uint16_t total_shots = (pData[6] << 8) | pData[7];
          Serial.printf("  Session ID: %u\n", sess_id);
          Serial.printf("  Total Shots: %u\n", total_shots);
          Serial.println("  (Session suspended - shots may not be readable until stopped)");

          currentSession.totalShots = total_shots;
          if (sessionSuspendedCallback) {
            sessionSuspendedCallback(currentSession);
          }
        }
        break;

      case 0x02: // SESSION_RESUMED
        Serial.println("SESSION_RESUMED");
        if (length >= 8) {
          uint32_t sess_id = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
          uint16_t total_shots = (pData[6] << 8) | pData[7];
          Serial.printf("  Session ID: %u\n", sess_id);
          Serial.printf("  Total Shots: %u\n", total_shots);

          currentSession.totalShots = total_shots;
          if (sessionResumedCallback) {
            sessionResumedCallback(currentSession);
          }
        }
        break;

      case 0x03: // SESSION_STOPPED
        Serial.println("SESSION_STOPPED");
        if (length >= 8) {
          uint32_t sess_id = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
          uint16_t total_shots = (pData[6] << 8) | pData[7];
          Serial.printf("  Session ID: %u\n", sess_id);
          Serial.printf("  Total Shots: %u\n", total_shots);

          // Display last shot if we have one
          if (hasLastShot) {
            Serial.printf("  Last Shot: #%u: %u:%02u\n", lastShotNum + 1, lastShotSeconds, lastShotHundredths);
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

      case 0x04: // SHOT_DETECTED
        Serial.println("SHOT_DETECTED");
        if (length >= 12) {
          uint32_t sess_id = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
          uint16_t shot_num = (pData[6] << 8) | pData[7];
          uint32_t shot_time_ms = (pData[8] << 24) | (pData[9] << 16) | (pData[10] << 8) | pData[11];

          // Convert milliseconds to seconds:hundredths format
          uint32_t seconds = shot_time_ms / 1000;
          uint32_t hundredths = (shot_time_ms % 1000) / 10;

          Serial.printf("  Shot #%u: %u:%02u\n", shot_num + 1, seconds, hundredths);

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
          shotData.shotNumber = shot_num;
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

      case 0x05: // SESSION_SET_BEGIN
        Serial.println("SESSION_SET_BEGIN");
        if (length >= 6) {
          uint32_t sess_id = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
          Serial.printf("  Session ID: %u\n", sess_id);
        }
        break;

      default:
        Serial.println("UNKNOWN");
        break;
    }
  }
}