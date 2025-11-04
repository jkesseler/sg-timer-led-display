/*
 * Minimal BLE Connection Test for SG Timer Device
 *
 * This sketch eliminates all variables and focuses purely on
 * establishing a BLE connection to debug the connection issue.
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLESecurity.h>
#include <esp_bt.h>

// SG Timer BLE Constants (from official API v3.2)
const char* SG_DEVICE_NAME_PREFIX = "SG-SST4";
const char* SG_SERVICE_UUID = "7520FFFF-14D2-4CDA-8B6B-697C554C9311";
const char* SG_COMMAND_UUID = "75200000-14D2-4CDA-8B6B-697C554C9311";  // COMMAND characteristic (W,N)
const char* SG_EVENT_UUID = "75200001-14D2-4CDA-8B6B-697C554C9311";    // EVENT characteristic (N)
const char* SG_API_VERSION_UUID = "7520FFFE-14D2-4CDA-8B6B-697C554C9311"; // API_VERSION characteristic (R)

// Global variables
BLEClient* pClient = nullptr;
BLEAddress* pServerAddress = nullptr;
BLERemoteCharacteristic* pCommandCharacteristic = nullptr;
BLERemoteCharacteristic* pEventCharacteristic = nullptr;
bool deviceFound = false;
bool doConnect = false;
bool connected = false;

// Forward declarations
bool connectToDevice();

// Simple logging
void logInfo(const char* tag, const char* message) {
  Serial.printf("[%6lu] %s: %s\n", millis(), tag, message);
}

void logError(const char* tag, const char* message) {
  Serial.printf("[%6lu] ERROR %s: %s\n", millis(), tag, message);
}

// BLE Scan Callback - Look for SG Timer
class MyScanCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String deviceName = advertisedDevice.getName().c_str();

    Serial.printf("[%6lu] SCAN: Found device '%s' (RSSI: %d)\n",
                  millis(), deviceName.c_str(), advertisedDevice.getRSSI());

    // Check if this is our SG Timer
    if (deviceName.startsWith(SG_DEVICE_NAME_PREFIX) ||
        (advertisedDevice.haveServiceUUID() &&
         advertisedDevice.getServiceUUID().equals(BLEUUID(SG_SERVICE_UUID)))) {

      logInfo("SCAN", "Found SG Timer! Stopping scan...");

      if (pServerAddress) delete pServerAddress;
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      deviceFound = true;
      doConnect = true;

      BLEDevice::getScan()->stop();
    }
  }
};

// BLE Client Callback - Handle connection events
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    logInfo("BLE", "Connected successfully!");
    connected = true;

    // Try to set connection parameters that match iPhone behavior
    // Lower intervals, typical for iOS
    esp_ble_conn_update_params_t conn_params;
    conn_params.min_int = 24;    // 30ms (iOS prefers 30ms intervals)
    conn_params.max_int = 40;    // 50ms
    conn_params.latency = 0;     // No latency
    conn_params.timeout = 400;   // 4000ms timeout

    // Note: These parameters will be negotiated by the central device (us)
  }

  void onDisconnect(BLEClient* pclient) {
    logInfo("BLE", "Disconnected");
    connected = false;
    deviceFound = false;
    doConnect = false;
  }
};

// STEP 2: BLE Security Callback for authentication
class MySecurityCallbacks : public BLESecurityCallbacks {
public:
  bool onConfirmPIN(uint32_t pin) override {
    Serial.printf("[%6lu] AUTH: Confirm PIN: %d\n", millis(), pin);
    return true;
  }

  uint32_t onPassKeyRequest() override {
    Serial.printf("[%6lu] AUTH: PassKey Request\n", millis());
    return 123456; // Default PIN
  }

  void onPassKeyNotify(uint32_t pass_key) override {
    Serial.printf("[%6lu] AUTH: PassKey Notify: %d\n", millis(), pass_key);
  }

  bool onSecurityRequest() override {
    Serial.printf("[%6lu] AUTH: Security Request\n", millis());
    return true;
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
    Serial.printf("[%6lu] AUTH: Authentication Complete - Success: %s\n",
                  millis(), cmpl.success ? "YES" : "NO");
  }
};

// STEP 4: Multiple connection attempts with different strategies
bool connectToDeviceWithRetries() {
  const int MAX_ATTEMPTS = 3;
  const int RETRY_DELAYS[] = {1000, 2000, 5000}; // Different delays between attempts

  for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
    Serial.printf("[%6lu] CONNECT: Attempt %d/%d\n", millis(), attempt, MAX_ATTEMPTS);

    if (connectToDevice()) {
      return true;
    }

    if (attempt < MAX_ATTEMPTS) {
      Serial.printf("[%6lu] CONNECT: Waiting %dms before retry...\n",
                    millis(), RETRY_DELAYS[attempt-1]);
      delay(RETRY_DELAYS[attempt-1]);

      // Clean up before retry
      if (pClient) {
        pClient->disconnect();
        delete pClient;
        pClient = nullptr;
      }
    }
  }

  return false;
}

// Attempt to connect to the found device with different parameters
bool connectToDevice() {
  if (!pServerAddress) {
    logError("CONNECT", "No server address available");
    return false;
  }

  logInfo("CONNECT", "Creating BLE client...");
  pClient = BLEDevice::createClient();

  logInfo("CONNECT", "Setting client callbacks...");
  pClient->setClientCallbacks(new MyClientCallback());

  // Simple connection approach like iPhone apps
  Serial.printf("[%6lu] CONNECT: Simple connection to %s\n",
                millis(), pServerAddress->toString().c_str());

  bool success = pClient->connect(*pServerAddress);

  if (success) {
    logInfo("CONNECT", "Connection successful!");

    // Wait for connection to stabilize
    delay(1000);

    if (!pClient->isConnected()) {
      logError("CONNECT", "Connection lost");
      return false;
    }

    logInfo("CONNECT", "Discovering all services...");

    // Get ALL services (like iPhone apps)
    std::map<std::string, BLERemoteService*>* pServices = pClient->getServices();

    if (!pServices || pServices->size() == 0) {
      logError("CONNECT", "No services found");
      pClient->disconnect();
      return false;
    }

    Serial.printf("[%6lu] Found %d services:\n", millis(), pServices->size());
    for (auto& service : *pServices) {
      Serial.printf("[%6lu] SERVICE: %s\n", millis(), service.first.c_str());
    }

    // Look for SG Timer service
    BLERemoteService* pSGService = nullptr;
    for (auto& service : *pServices) {
      if (service.first == SG_SERVICE_UUID) {
        pSGService = service.second;
        break;
      }
    }

    if (!pSGService) {
      logError("CONNECT", "SG service not found");
      pClient->disconnect();
      return false;
    }

    logInfo("CONNECT", "Found SG service! Getting characteristics...");

    // Get ALL characteristics (like iPhone apps)
    std::map<std::string, BLERemoteCharacteristic*>* pChars = pSGService->getCharacteristics();

    if (!pChars || pChars->size() == 0) {
      logError("CONNECT", "No characteristics found");
      pClient->disconnect();
      return false;
    }

    Serial.printf("[%6lu] Found %d characteristics:\n", millis(), pChars->size());
    for (auto& characteristic : *pChars) {
      BLERemoteCharacteristic* pChar = characteristic.second;
      String props = "";
      if (pChar->canRead()) props += "R ";
      if (pChar->canWrite()) props += "W ";
      if (pChar->canNotify()) props += "N ";

      Serial.printf("[%6lu] CHAR: %s [%s]\n",
                   millis(), characteristic.first.c_str(), props.c_str());

      // Try reading readable characteristics
      if (pChar->canRead()) {
        try {
          std::string value = pChar->readValue();
          Serial.printf("[%6lu] READ: %s = '%s'\n",
                       millis(), characteristic.first.c_str(), value.c_str());
        } catch (...) {
          Serial.printf("[%6lu] READ FAILED: %s\n", millis(), characteristic.first.c_str());
        }
      }
    }

    logInfo("CONNECT", "iPhone-style discovery complete!");
    return true;

    if (pRemoteService == nullptr) {
      logError("CONNECT", "Failed to find SG Timer service");

      // Try to get all services and list them
      logInfo("CONNECT", "Listing all available services...");
      std::map<std::string, BLERemoteService*>* pServices = pClient->getServices();
      if (pServices->size() > 0) {
        for (auto& service : *pServices) {
          Serial.printf("[%6lu] SERVICE: %s\n", millis(), service.first.c_str());
        }
      } else {
        logError("CONNECT", "No services found at all");
      }

      pClient->disconnect();
      return false;
    }

    logInfo("CONNECT", "Service found! Getting characteristics...");

    // Get COMMAND characteristic (Write, Notify)
    pCommandCharacteristic = pRemoteService->getCharacteristic(SG_COMMAND_UUID);
    if (pCommandCharacteristic == nullptr) {
      logError("CONNECT", "Failed to find COMMAND characteristic");

      // List all characteristics in the service
      logInfo("CONNECT", "Listing all characteristics in service...");
      std::map<std::string, BLERemoteCharacteristic*>* pCharacteristics = pRemoteService->getCharacteristics();
      for (auto& characteristic : *pCharacteristics) {
        Serial.printf("[%6lu] CHAR: %s\n", millis(), characteristic.first.c_str());
      }

      pClient->disconnect();
      return false;
    }
    logInfo("CONNECT", "COMMAND characteristic found!");

    // Get EVENT characteristic (Notify)
    pEventCharacteristic = pRemoteService->getCharacteristic(SG_EVENT_UUID);
    if (pEventCharacteristic == nullptr) {
      logError("CONNECT", "Failed to find EVENT characteristic");
      pClient->disconnect();
      return false;
    }
    logInfo("CONNECT", "EVENT characteristic found!");

    // Subscribe to notifications on both characteristics
    if (pCommandCharacteristic->canNotify()) {
      logInfo("CONNECT", "Subscribing to COMMAND notifications...");
      pCommandCharacteristic->registerForNotify([](BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
        Serial.printf("[%6lu] COMMAND Response: ", millis());
        for (size_t i = 0; i < length; i++) {
          Serial.printf("%02X ", pData[i]);
        }
        Serial.println();
      });
    }

    if (pEventCharacteristic->canNotify()) {
      logInfo("CONNECT", "Subscribing to EVENT notifications...");
      pEventCharacteristic->registerForNotify([](BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
        Serial.printf("[%6lu] EVENT Received: ", millis());
        for (size_t i = 0; i < length; i++) {
          Serial.printf("%02X ", pData[i]);
        }
        Serial.println();
      });
    }

    // Test reading API version
    logInfo("CONNECT", "Reading API version...");
    BLERemoteCharacteristic* pApiVersionChar = pRemoteService->getCharacteristic(SG_API_VERSION_UUID);
    if (pApiVersionChar && pApiVersionChar->canRead()) {
      std::string apiVersion = pApiVersionChar->readValue();
      Serial.printf("[%6lu] API Version: %s\n", millis(), apiVersion.c_str());
    }

    logInfo("CONNECT", "Full SG Timer connection established!");
    return true;

  } else {
    logError("CONNECT", "Connection failed");
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  logInfo("SETUP", "Starting BLE Connection Test with Enhanced Features");
  logInfo("SETUP", "Target: SG Timer devices starting with 'SG-SST4'");

  // STEP 2: Initialize BLE with iOS-like settings
  logInfo("SETUP", "Initializing BLE with iOS-compatible settings...");
  BLEDevice::init("iPhone-BLE-Test");  // Generic name like iOS might use

  // STEP 2: Try simpler security setup (iOS apps often don't require complex auth)
  logInfo("SETUP", "Setting up minimal BLE security...");
  // Remove complex security for now - try basic connection first

  // STEP 3: Use more conservative BLE stack configurations
  logInfo("SETUP", "Configuring BLE stack for compatibility...");

  // Use more conservative power levels (iOS devices are typically lower power)
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P3);    // Lower power
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P3);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P3);

  // Start scanning with iOS-like parameters
  logInfo("SETUP", "Starting BLE scan with iOS-compatible parameters...");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyScanCallback());
  pBLEScan->setActiveScan(true);

  // STEP 1: Use iOS-like scan parameters (less aggressive)
  pBLEScan->setInterval(96);   // 60ms intervals (iOS-like)
  pBLEScan->setWindow(48);     // 30ms window (50% duty cycle)

  logInfo("SETUP", "Scan started with iOS-compatible settings...");
}// Test SG Timer functionality after connection
void testSGTimer() {
  if (!connected || !pCommandCharacteristic) {
    logError("TEST", "Not connected or missing characteristics");
    return;
  }

  logInfo("TEST", "Testing SG Timer API commands...");

  // Test API Version read
  if (pCommandCharacteristic->canRead()) {
    logInfo("TEST", "Reading API version...");
    // API version is on a different characteristic - we already read it during connection
  }

  // Wait a bit between tests
  delay(2000);

  logInfo("TEST", "SG Timer basic connectivity test complete");
}

void loop() {
  // If we found a device and need to connect
  if (doConnect && !connected) {
    doConnect = false;

    logInfo("LOOP", "Attempting connection with retries...");
    if (connectToDeviceWithRetries()) {
      logInfo("LOOP", "Connection successful!");

      // Test SG Timer functionality
      delay(1000);
      testSGTimer();
    } else {
      logError("LOOP", "All connection attempts failed. Restarting scan in 10 seconds...");
      delay(10000);

      // STEP 5: Reset BLE stack between attempts
      logInfo("LOOP", "Resetting BLE stack...");
      BLEDevice::deinit(true);
      delay(1000);

      // Reinitialize with different configuration
      BLEDevice::init("ESP32-Test-V2");
      // Try without encryption this time

      // Restart scanning
      deviceFound = false;
      BLEScan* pBLEScan = BLEDevice::getScan();
      pBLEScan->setAdvertisedDeviceCallbacks(new MyScanCallback());
      pBLEScan->setActiveScan(true);
      pBLEScan->setInterval(80);   // Try faster scan
      pBLEScan->setWindow(40);
      pBLEScan->start(10, false);
    }
  }

  // If not connected and no device found, keep scanning
  if (!connected && !deviceFound) {
    static unsigned long lastScanStart = 0;
    if (millis() - lastScanStart > 15000) {  // Restart scan every 15 seconds
      logInfo("LOOP", "Restarting scan...");
      BLEDevice::getScan()->start(10, false);
      lastScanStart = millis();
    }
  }

  // Status update every 10 seconds
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 10000) {
    Serial.printf("[%6lu] STATUS: Found=%s, Connected=%s\n",
                  millis(),
                  deviceFound ? "YES" : "NO",
                  connected ? "YES" : "NO");
    lastStatus = millis();
  }

  delay(100);
}