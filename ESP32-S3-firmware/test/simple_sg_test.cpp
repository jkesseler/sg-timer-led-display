/*
 * Minimal SG Timer Connection Test
 * Based on iPhone BT Inspector/nRF Connect behavior
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <BLEScan.h>

// SG Timer BLE Constants
const char* SG_DEVICE_NAME_PREFIX = "SG-SST4";
const char* SG_SERVICE_UUID = "7520FFFF-14D2-4CDA-8B6B-697C554C9311";
const char* SG_API_VERSION_UUID = "7520FFFE-14D2-4CDA-8B6B-697C554C9311";

// Global variables
BLEClient* pClient = nullptr;
BLEAddress* pServerAddress = nullptr;
bool deviceFound = false;
bool doConnect = false;
bool connected = false;
bool testComplete = false;

// Simple logging
void logInfo(const char* tag, const char* message) {
  Serial.printf("[%6lu] %s: %s\n", millis(), tag, message);
}

void logError(const char* tag, const char* message) {
  Serial.printf("[%6lu] ERROR %s: %s\n", millis(), tag, message);
}

// BLE Scan Callback
class MyScanCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String deviceName = advertisedDevice.getName().c_str();

    if (deviceName.length() > 0) {
      Serial.printf("[%6lu] SCAN: Found '%s' (RSSI: %d)\n",
                    millis(), deviceName.c_str(), advertisedDevice.getRSSI());
    }

    // Check if this is our SG Timer
    if (deviceName.startsWith(SG_DEVICE_NAME_PREFIX)) {
      logInfo("SCAN", "Found SG Timer! Stopping scan...");

      if (pServerAddress) delete pServerAddress;
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      deviceFound = true;
      doConnect = true;

      BLEDevice::getScan()->stop();
    }
  }
};

// BLE Client Callback
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    logInfo("BLE", "Connected successfully!");
    connected = true;
  }

  void onDisconnect(BLEClient* pclient) {
    logInfo("BLE", "Disconnected");
    connected = false;
    deviceFound = false;
    doConnect = false;
  }
};

// Simple connection test - just like iPhone apps
bool simpleConnectTest() {
  if (!pServerAddress) {
    logError("CONNECT", "No server address available");
    return false;
  }

  logInfo("CONNECT", "Creating BLE client...");
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  // Use default connection parameters (like iPhone)
  Serial.printf("[%6lu] CONNECT: Connecting to %s\n",
                millis(), pServerAddress->toString().c_str());

  bool success = pClient->connect(*pServerAddress);

  if (success) {
    logInfo("CONNECT", "Basic connection successful!");

    // Wait for connection to stabilize
    delay(1000);

    if (!pClient->isConnected()) {
      logError("CONNECT", "Connection lost during stabilization");
      return false;
    }

    // Try to discover services (like iPhone apps do)
    logInfo("CONNECT", "Discovering services...");
    std::map<std::string, BLERemoteService*>* pServices = pClient->getServices();

    if (pServices && pServices->size() > 0) {
      Serial.printf("[%6lu] SUCCESS: Found %d services\n", millis(), pServices->size());

      for (auto& service : *pServices) {
        Serial.printf("[%6lu] SERVICE: %s\n", millis(), service.first.c_str());

        // If this is our main service, try to read characteristics
        if (service.first == SG_SERVICE_UUID) {
          logInfo("CONNECT", "Found main SG Timer service!");

          BLERemoteService* pMainService = service.second;

          // List all characteristics in the service
          std::map<std::string, BLERemoteCharacteristic*>* pCharacteristics = pMainService->getCharacteristics();

          if (pCharacteristics) {
            Serial.printf("[%6lu] Found %d characteristics:\n", millis(), pCharacteristics->size());

            for (auto& characteristic : *pCharacteristics) {
              BLERemoteCharacteristic* pChar = characteristic.second;
              String props = "";
              if (pChar->canRead()) props += "R ";
              if (pChar->canWrite()) props += "W ";
              if (pChar->canNotify()) props += "N ";
              if (pChar->canIndicate()) props += "I ";

              Serial.printf("[%6lu] CHAR: %s [%s]\n",
                           millis(), characteristic.first.c_str(), props.c_str());

              // Try to read API version if this is the right characteristic
              if (characteristic.first == SG_API_VERSION_UUID && pChar->canRead()) {
                try {
                  logInfo("TEST", "Reading API version...");
                  std::string apiVersion = pChar->readValue();
                  Serial.printf("[%6lu] SUCCESS: API Version = '%s'\n",
                               millis(), apiVersion.c_str());
                } catch (...) {
                  logError("TEST", "Failed to read API version");
                }
              }
            }
          }
        }
      }

      logInfo("SUCCESS", "Connection and discovery completed like iPhone apps!");
      return true;

    } else {
      logError("CONNECT", "No services discovered");
      pClient->disconnect();
      return false;
    }

  } else {
    logError("CONNECT", "Basic connection failed");
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  logInfo("SETUP", "Starting Simple SG Timer Connection Test");
  logInfo("SETUP", "Mimicking iPhone BT Inspector behavior");

  // Simple BLE initialization (like iPhone)
  BLEDevice::init("iPhone-Test");

  // Start scanning with simple parameters
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyScanCallback());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(96);   // 60ms - iOS-like
  pBLEScan->setWindow(48);     // 30ms - iOS-like

  logInfo("SETUP", "Starting scan...");
}

void loop() {
  // If we found a device and need to connect
  if (doConnect && !connected && !testComplete) {
    doConnect = false;

    logInfo("LOOP", "Attempting simple connection test...");
    if (simpleConnectTest()) {
      logInfo("LOOP", "SUCCESS! Connection test passed!");
      testComplete = true;

      // Keep connection for testing
      delay(5000);

      if (pClient && pClient->isConnected()) {
        logInfo("LOOP", "Disconnecting...");
        pClient->disconnect();
      }

    } else {
      logError("LOOP", "Connection test failed, restarting scan in 5s...");
      delay(5000);
      deviceFound = false;
      BLEDevice::getScan()->start(10, false);
    }
  }

  // Keep scanning if needed
  if (!connected && !deviceFound && !testComplete) {
    static unsigned long lastScanStart = 0;
    if (millis() - lastScanStart > 15000) {
      logInfo("LOOP", "Restarting scan...");
      BLEDevice::getScan()->start(10, false);
      lastScanStart = millis();
    }
  }

  // Status update
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 10000) {
    if (!testComplete) {
      Serial.printf("[%6lu] STATUS: Scanning for SG Timer...\n", millis());
    } else {
      Serial.printf("[%6lu] STATUS: Test completed successfully!\n", millis());
    }
    lastStatus = millis();
  }

  delay(100);
}