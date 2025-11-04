/*
 * Minimal BLE Crash Test
 *
 * This test focuses ONLY on reproducing the crash that happens during
 * BLE peer device addition. No extra features, just basic connection.
 *
 * Goal: Isolate the exact point where the crash occurs
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>

// SG Timer Constants
const char* SG_DEVICE_PREFIX = "SG-SST4";
const char* SG_SERVICE_UUID = "7520FFFF-14D2-4CDA-8B6B-697C554C9311";

// Global state
BLEAddress* targetAddress = nullptr;
BLEClient* client = nullptr;
bool deviceFound = false;
bool attemptConnection = false;

void logWithTime(const char* message) {
  Serial.printf("[%6lu] %s\n", millis(), message);
}

class ScanCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice device) {
    String name = device.getName().c_str();

    if (name.length() > 0) {
      Serial.printf("[%6lu] SCAN: Found '%s'\n", millis(), name.c_str());
    }

    if (name.startsWith(SG_DEVICE_PREFIX)) {
      logWithTime("FOUND SG TIMER! Stopping scan...");

      if (targetAddress) delete targetAddress;
      targetAddress = new BLEAddress(device.getAddress());
      deviceFound = true;
      attemptConnection = true;

      BLEDevice::getScan()->stop();
    }
  }
};

class ClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* client) {
    logWithTime("BLE: CONNECTED successfully!");

    // This is where the crash typically happens - during service discovery
    logWithTime("BLE: Starting service discovery...");

    try {
      // Try to get services - this is where peer device addition happens
      logWithTime("BLE: Calling getServices()...");
      std::map<std::string, BLERemoteService*>* services = client->getServices();

      if (services) {
        Serial.printf("[%6lu] BLE: Found %d services\n", millis(), services->size());
        logWithTime("BLE: Service discovery completed without crash!");
      } else {
        logWithTime("BLE: No services found");
      }

    } catch (...) {
      logWithTime("BLE: EXCEPTION during service discovery");
    }
  }

  void onDisconnect(BLEClient* client) {
    logWithTime("BLE: Disconnected");
  }
};

void attemptSimpleConnection() {
  if (!targetAddress) {
    logWithTime("ERROR: No target address");
    return;
  }

  logWithTime("Creating BLE client...");
  client = BLEDevice::createClient();

  logWithTime("Setting callbacks...");
  client->setClientCallbacks(new ClientCallback());

  Serial.printf("[%6lu] Connecting to %s...\n", millis(), targetAddress->toString().c_str());

  // This is the critical call - where the crash happens
  logWithTime("CRITICAL: Calling connect() - this is where crash typically occurs");

  bool success = client->connect(*targetAddress);

  if (success) {
    logWithTime("Connect() returned SUCCESS");

    // Wait a bit to see if connection holds
    delay(2000);

    if (client->isConnected()) {
      logWithTime("Connection is stable after 2 seconds");

      // Disconnect after successful test
      logWithTime("Disconnecting...");
      client->disconnect();

    } else {
      logWithTime("Connection lost after connect() success");
    }

  } else {
    logWithTime("Connect() returned FAILURE");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  logWithTime("=== MINIMAL BLE CRASH TEST ===");
  logWithTime("Goal: Reproduce crash during BLE connection");

  Serial.printf("[%6lu] Free heap at start: %zu bytes\n", millis(), ESP.getFreeHeap());

  logWithTime("Initializing BLE...");
  BLEDevice::init("CrashTest");

  logWithTime("Starting scan...");
  BLEScan* scanner = BLEDevice::getScan();
  scanner->setAdvertisedDeviceCallbacks(new ScanCallback());
  scanner->setActiveScan(true);
  scanner->setInterval(100);
  scanner->setWindow(50);

  scanner->start(10, false);
}

void loop() {
  if (attemptConnection && deviceFound) {
    attemptConnection = false;

    logWithTime("Starting connection attempt...");
    Serial.printf("[%6lu] Free heap before connection: %zu bytes\n", millis(), ESP.getFreeHeap());

    attemptSimpleConnection();

    Serial.printf("[%6lu] Free heap after connection: %zu bytes\n", millis(), ESP.getFreeHeap());
    logWithTime("Connection test completed");

    // Reset for another attempt if needed
    deviceFound = false;
    delay(5000);

    logWithTime("Restarting scan...");
    BLEDevice::getScan()->start(10, false);
  }

  delay(100);
}