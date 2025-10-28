#include "SGTimerDevice.h"
#include "Logger.h"
#include "common.h"

// Static constants
const char* SGTimerDevice::DEVICE_NAME_PREFIX = "SG-SST4";
const char* SGTimerDevice::SERVICE_UUID = "7520FFFF-14D2-4CDA-8B6B-697C554C9311";
const char* SGTimerDevice::CHARACTERISTIC_UUID = "75200001-14D2-4CDA-8B6B-697C554C9311";
const char* SGTimerDevice::SHOT_LIST_UUID = "75200004-14D2-4CDA-8B6B-697C554C9311";

// Static instance for callbacks
SGTimerDevice* SGTimerDevice::instance = nullptr;

// BLE Advertised Device Callback class
class SGTimerDevice::AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
private:
  SGTimerDevice* device;

public:
  AdvertisedDeviceCallbacks(SGTimerDevice* dev) : device(dev) {}

  void onResult(BLEAdvertisedDevice advertisedDevice) {
    LOG_DEBUG("SG-SCAN", "Found device: %s (RSSI: %d)",
              advertisedDevice.toString().c_str(),
              advertisedDevice.getRSSI());

    // Check if this is our target device
    String deviceName = advertisedDevice.getName().c_str();
    if (deviceName.startsWith(DEVICE_NAME_PREFIX) ||
        (advertisedDevice.haveServiceUUID() &&
         advertisedDevice.getServiceUUID().equals(BLEUUID(SERVICE_UUID)))) {

      LOG_INFO("SG-SCAN", "Found SG Shot Timer: %s", deviceName.c_str());

      if (device->pServerAddress) {
        delete device->pServerAddress;
      }
      device->pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      device->deviceName = deviceName;
      device->doConnect = true;
      device->doScan = false;

      // Stop scanning
      BLEDevice::getScan()->stop();
    }
  }
};

// BLE Client Callback class
class SGTimerDevice::ClientCallback : public BLEClientCallbacks {
private:
  SGTimerDevice* device;

public:
  ClientCallback(SGTimerDevice* dev) : device(dev) {}

  void onConnect(BLEClient* pclient) {
    LOG_BLE("Connected to SG Shot Timer");
    device->deviceConnected = true;
    device->setConnectionState(DeviceConnectionState::CONNECTED);
  }

  void onDisconnect(BLEClient* pclient) {
    LOG_BLE("Disconnected from SG Shot Timer");
    device->deviceConnected = false;
    device->doScan = true;
    device->setConnectionState(DeviceConnectionState::DISCONNECTED);

    // Reset session tracking
    device->hasFirstShot = false;
    device->previousShotTime = 0;
    device->currentSession = {};
  }
};

SGTimerDevice::SGTimerDevice() :
  pClient(nullptr),
  pRemoteCharacteristic(nullptr),
  pShotListCharacteristic(nullptr),
  pServerAddress(nullptr),
  connectionState(DeviceConnectionState::DISCONNECTED),
  deviceConnected(false),
  doConnect(false),
  doScan(false),
  deviceModel("SG Timer"),
  previousShotTime(0),
  hasFirstShot(false)
{
  instance = this;
  currentSession = {};
}

SGTimerDevice::~SGTimerDevice() {
  disconnect();
  if (pServerAddress) {
    delete pServerAddress;
    pServerAddress = nullptr;
  }
  instance = nullptr;
}

bool SGTimerDevice::initialize() {
  LOG_INFO("SG-TIMER", "Initializing SG Timer device interface");

  // BLE should already be initialized by main
  setConnectionState(DeviceConnectionState::DISCONNECTED);
  return true;
}

bool SGTimerDevice::startScanning() {
  LOG_INFO("SG-TIMER", "Starting scan for SG Timer devices");
  setConnectionState(DeviceConnectionState::SCANNING);
  doScan = true;
  return true;
}

bool SGTimerDevice::connect(BLEAddress address) {
  if (pServerAddress) {
    delete pServerAddress;
  }
  pServerAddress = new BLEAddress(address);
  doConnect = true;
  return true;
}

void SGTimerDevice::disconnect() {
  if (pClient && deviceConnected) {
    pClient->disconnect();
  }
  deviceConnected = false;
  setConnectionState(DeviceConnectionState::DISCONNECTED);
}

DeviceConnectionState SGTimerDevice::getConnectionState() const {
  return connectionState;
}

bool SGTimerDevice::isConnected() const {
  return deviceConnected;
}

const char* SGTimerDevice::getDeviceModel() const {
  return deviceModel.c_str();
}

const char* SGTimerDevice::getDeviceName() const {
  return deviceName.c_str();
}

BLEAddress SGTimerDevice::getDeviceAddress() const {
  if (pServerAddress) {
    return *pServerAddress;
  }
  return BLEAddress("");
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
  return false; // SG Timer doesn't support remote start
}

bool SGTimerDevice::supportsShotList() const {
  return true; // SG Timer supports shot list retrieval
}

bool SGTimerDevice::supportsSessionControl() const {
  return false; // SG Timer doesn't support remote session control
}

bool SGTimerDevice::requestShotList(uint32_t sessionId) {
  if (!deviceConnected || !pShotListCharacteristic) {
    return false;
  }

  readShotListInternal(sessionId);
  return true;
}

bool SGTimerDevice::startSession() {
  return false; // Not supported by SG Timer
}

bool SGTimerDevice::stopSession() {
  return false; // Not supported by SG Timer
}

void SGTimerDevice::update() {
  // Handle connection logic
  if (doConnect) {
    connectToServer();
    doConnect = false;
  }

  // Handle scanning
  if (doScan) {
    scanForDevices();
    doScan = false;
  }

  // Check for reconnection
  if (!deviceConnected && pClient != nullptr && connectionState == DeviceConnectionState::DISCONNECTED) {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt > BLE_RECONNECT_INTERVAL) {
      LOG_INFO("SG-TIMER", "Attempting to reconnect...");
      startScanning();
      lastReconnectAttempt = millis();
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

void SGTimerDevice::scanForDevices() {
  Serial.println("[SG-SCAN] Starting BLE scan for SG Shot Timer...");

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(this));
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);

  // Scan for 10 seconds
  BLEScanResults foundDevices = pBLEScan->start(10, false);
  Serial.printf("[SG-SCAN] Scan completed. Found %d devices\n", foundDevices.getCount());

  if (!doConnect) {
    Serial.println("[SG-SCAN] SG Shot Timer not found. Will retry in 5 seconds...");
    setConnectionState(DeviceConnectionState::DISCONNECTED);
  }

  pBLEScan->clearResults();
}

void SGTimerDevice::connectToServer() {
  if (!pServerAddress) {
    Serial.println("[SG-BLE] No server address available");
    return;
  }

  Serial.printf("[SG-BLE] Connecting to device at address: %s\n", pServerAddress->toString().c_str());
  setConnectionState(DeviceConnectionState::CONNECTING);

  // Create BLE client
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new ClientCallback(this));

  // Connect to the server
  if (!pClient->connect(*pServerAddress)) {
    Serial.println("[SG-BLE] Failed to connect to server");
    setConnectionState(DeviceConnectionState::ERROR);
    return;
  }

  Serial.println("[SG-BLE] Connected to server, discovering services...");

  // Get the service
  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.printf("[SG-BLE] Failed to find service UUID: %s\n", SERVICE_UUID);
    pClient->disconnect();
    setConnectionState(DeviceConnectionState::ERROR);
    return;
  }

  Serial.println("[SG-BLE] Found service, getting characteristics...");

  // Get the EVENT characteristic
  pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.printf("[SG-BLE] Failed to find characteristic UUID: %s\n", CHARACTERISTIC_UUID);
    pClient->disconnect();
    setConnectionState(DeviceConnectionState::ERROR);
    return;
  }

  // Get the SHOT_LIST characteristic
  pShotListCharacteristic = pRemoteService->getCharacteristic(SHOT_LIST_UUID);
  if (pShotListCharacteristic == nullptr) {
    Serial.printf("[SG-BLE] Warning: Failed to find shot list characteristic UUID: %s\n", SHOT_LIST_UUID);
  }

  Serial.println("[SG-BLE] Found characteristics, setting up notifications...");

  // Check if characteristic can notify
  if (pRemoteCharacteristic->canNotify()) {
    // Register for notifications
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println("[SG-BLE] Notification callback registered");
    Serial.println("[SG-BLE] === Ready to receive shot timer data ===");
  } else {
    Serial.println("[SG-BLE] Characteristic cannot notify");
    pClient->disconnect();
    setConnectionState(DeviceConnectionState::ERROR);
    return;
  }

  deviceConnected = true;
  setConnectionState(DeviceConnectionState::CONNECTED);
}

// Static notification callback
void SGTimerDevice::notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                  uint8_t* pData, size_t length, bool isNotify) {
  if (instance) {
    instance->processTimerData(pData, length);
  }
}

String SGTimerDevice::parseHexData(uint8_t* data, size_t length) {
  String hexString = "";
  for (size_t i = 0; i < length; i++) {
    if (i > 0) hexString += " ";
    if (data[i] < 16) hexString += "0";
    hexString += String(data[i], HEX);
  }
  return hexString;
}

void SGTimerDevice::processTimerData(uint8_t* data, size_t length) {
  if (length < 2) {
    Serial.println("[SG-PARSE] Data packet too short");
    return;
  }

  // Print raw hex data for debugging
  String hexData = parseHexData(data, length);
  Serial.printf("[SG-DATA] Received %d bytes: %s\n", length, hexData.c_str());

  // SG Timer Event format: len, event_id, [event_data]
  uint8_t len = data[0];
  uint8_t eventId = data[1];

  Serial.printf("[SG-PARSE] Event ID: %d (0x%02X), Length: %d\n", eventId, eventId, len);

  switch (eventId) {
    case 0x00: // SESSION_STARTED
      {
        if (length < 8) {
          Serial.println("[SG-PARSE] SESSION_STARTED packet too short");
          return;
        }
        // sess_id (4 bytes), start_delay (2 bytes)
        uint32_t sessionId = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
        uint16_t startDelay = (data[6] << 8) | data[7];
        float startDelaySeconds = startDelay / 10.0;

        Serial.printf("[SG-TIMER] === SESSION STARTED === (ID: %u, Delay: %.1fs)\n",
                     sessionId, startDelaySeconds);

        // Update session state
        currentSession.sessionId = sessionId;
        currentSession.isActive = true;
        currentSession.totalShots = 0;
        currentSession.startTimestamp = millis();
        currentSession.startDelaySeconds = startDelaySeconds;

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
      {
        if (length < 8) {
          Serial.println("[SG-PARSE] SESSION_SUSPENDED packet too short");
          return;
        }
        uint32_t sessId = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
        uint16_t totalShots = (data[6] << 8) | data[7];

        Serial.printf("[SG-TIMER] === SESSION SUSPENDED === (ID: %u, Total Shots: %u)\n",
                     sessId, totalShots);

        currentSession.totalShots = totalShots;
        if (sessionSuspendedCallback) {
          sessionSuspendedCallback(currentSession);
        }
      }
      break;

    case 0x02: // SESSION_RESUMED
      {
        if (length < 8) {
          Serial.println("[SG-PARSE] SESSION_RESUMED packet too short");
          return;
        }
        uint32_t sessId = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
        uint16_t totalShots = (data[6] << 8) | data[7];

        Serial.printf("[SG-TIMER] === SESSION RESUMED === (ID: %u, Total Shots: %u)\n",
                     sessId, totalShots);

        currentSession.totalShots = totalShots;
        if (sessionResumedCallback) {
          sessionResumedCallback(currentSession);
        }
      }
      break;

    case 0x03: // SESSION_STOPPED
      {
        if (length < 8) {
          Serial.println("[SG-PARSE] SESSION_STOPPED packet too short");
          return;
        }
        uint32_t sessId = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
        uint16_t totalShots = (data[6] << 8) | data[7];

        Serial.printf("[SG-TIMER] === SESSION STOPPED === (ID: %u, Total Shots: %u)\n",
                     sessId, totalShots);

        currentSession.isActive = false;
        currentSession.totalShots = totalShots;
        hasFirstShot = false;
        previousShotTime = 0;

        if (sessionStoppedCallback) {
          sessionStoppedCallback(currentSession);
        }

        // Auto-request shot list if available
        if (totalShots > 0 && sessId != 0 && pShotListCharacteristic) {
          requestShotList(sessId);
        }
      }
      break;

    case 0x04: // SHOT_DETECTED
      {
        if (length < 12) {
          Serial.println("[SG-PARSE] SHOT_DETECTED packet too short");
          return;
        }
        // sess_id (4 bytes), shot_num (2 bytes), shot_time (4 bytes in milliseconds)
        uint32_t sessId = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
        uint16_t shotNum = (data[6] << 8) | data[7];
        uint32_t shotTime = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];

        uint32_t splitTime = 0;
        bool isFirstShot = !hasFirstShot;

        if (hasFirstShot) {
          splitTime = shotTime - previousShotTime;
        } else {
          hasFirstShot = true;
        }

        // Update previous shot time
        previousShotTime = shotTime;

        // Create normalized shot data
        NormalizedShotData shotData;
        shotData.sessionId = sessId;
        shotData.shotNumber = shotNum;
        shotData.absoluteTimeMs = shotTime;
        shotData.splitTimeMs = splitTime;
        shotData.timestampMs = millis();
        shotData.deviceModel = deviceModel.c_str();
        shotData.isFirstShot = isFirstShot;

        // Format time for logging
        float shotTimeSec = shotTime / 1000.0;
        float splitTimeSec = splitTime / 1000.0;

        Serial.printf("[SG-TIMER] SHOT: %d - TIME: %.3fs | SPLIT: %.3fs\n",
                     shotNum, shotTimeSec, splitTimeSec);

        // Notify callback
        if (shotDetectedCallback) {
          shotDetectedCallback(shotData);
        }
      }
      break;

    case 0x05: // SESSION_SET_BEGIN
      {
        if (length < 6) {
          Serial.println("[SG-PARSE] SESSION_SET_BEGIN packet too short");
          return;
        }
        uint32_t sessId = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];

        Serial.printf("[SG-TIMER] === SESSION SET BEGIN === (ID: %u)\n", sessId);
      }
      break;

    default:
      Serial.printf("[SG-PARSE] Unknown event ID: %d (0x%02X)\n", eventId, eventId);
      break;
  }
}

void SGTimerDevice::readShotListInternal(uint32_t sessionId) {
  if (!deviceConnected || !pShotListCharacteristic) {
    Serial.println("[SG-SHOT_LIST] Not connected or shot list characteristic not available");
    return;
  }

  Serial.printf("[SG-SHOT_LIST] Reading shots for session ID: %u\n", sessionId);

  // Write session ID to characteristic (Big Endian format)
  uint8_t sessIdBytes[4];
  sessIdBytes[0] = (sessionId >> 24) & 0xFF;
  sessIdBytes[1] = (sessionId >> 16) & 0xFF;
  sessIdBytes[2] = (sessionId >> 8) & 0xFF;
  sessIdBytes[3] = sessionId & 0xFF;

  pShotListCharacteristic->writeValue(sessIdBytes, 4);
  delay(50);

  // Read shot data
  Serial.println("[SG-SHOT_LIST] Shot details:");
  Serial.println("[SG-SHOT_LIST] #  | Time (s)");
  Serial.println("[SG-SHOT_LIST] ---|----------");

  for (int i = 0; i < currentSession.totalShots && i < 100; i++) {
    std::string value = pShotListCharacteristic->readValue();

    if (value.length() < 6) {
      Serial.println("[SG-SHOT_LIST] Invalid shot data");
      break;
    }

    uint8_t* data = (uint8_t*)value.data();

    // Parse shot data (Big Endian)
    uint16_t shotNumber = (data[0] << 8) | data[1];
    uint32_t shotTime = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];

    // Check for end of list marker
    if (shotTime == 0xFFFFFFFF) {
      Serial.println("[SG-SHOT_LIST] End of shot list");
      break;
    }

    float shotTimeSec = shotTime / 1000.0;
    Serial.printf("[SG-SHOT_LIST] %2d | %.3f\n", shotNumber, shotTimeSec);

    delay(10); // Small delay between reads
  }
}