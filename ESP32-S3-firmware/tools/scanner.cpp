#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

const int SCAN_DURATION = 10; // seconds

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // Device found during scan - will be collected in scan results
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n");
  Serial.println("========================================");
  Serial.println("  BLE Device Scanner (ESP32-S3)");
  Serial.println("========================================");
  Serial.println();
  Serial.println("This will scan for nearby BLE devices");
  Serial.println("Power on your timer and wait...");
  Serial.println();

  // Initialize BLE
  BLEDevice::init("ESP32-BLE-Scanner");

  Serial.printf("Starting BLE discovery (%d seconds)...\n", SCAN_DURATION);
  Serial.println("Please wait...");
  Serial.println();

  // Create scanner and configure
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  // Start scan
  BLEScanResults foundDevices = pBLEScan->start(SCAN_DURATION, false);
  int count = foundDevices.getCount();

  if (count == 0) {
    Serial.println("No BLE devices found.");
    Serial.println("Make sure your timer is powered on and in range.");
  } else {
    Serial.printf("Found %d BLE device(s):\n\n", count);

    for (int i = 0; i < count; i++) {
      BLEAdvertisedDevice device = foundDevices.getDevice(i);

      Serial.println("----------------------------------------");
      Serial.printf("Device #%d:\n", i + 1);

      // Device name
      String name = device.getName().c_str();
      if (name.length() > 0) {
        Serial.printf("  Name:           %s\n", name.c_str());
      } else {
        Serial.println("  Name:           <Unknown>");
      }

      // MAC address
      Serial.printf("  MAC Address:    %s\n", device.getAddress().toString().c_str());

      // Signal strength
      Serial.printf("  RSSI:           %d dBm\n", device.getRSSI());

      // Service UUID if advertised
      if (device.haveServiceUUID()) {
        Serial.printf("  Service UUID:   %s\n", device.getServiceUUID().toString().c_str());
      }

      // Check if this looks like a known timer device
      if (name.indexOf("SP M1A2") >= 0) {
        Serial.println();
        Serial.println("  >>> Special Pie Timer M1A2+ <<<");
        Serial.println("  Use with: test-special-pie");
      } else if (name.indexOf("SG-SST") >= 0 || name.indexOf("SG Timer") >= 0) {
        Serial.println();
        Serial.println("  >>> SG Timer Device <<<");
        Serial.println("  Use with: test-sg-timer");
      } else if (name.indexOf("TIMER") >= 0 || name.indexOf("Timer") >= 0) {
        Serial.println();
        Serial.println("  >>> Possible timer device <<<");
      }

      Serial.println();
    }

    Serial.println("========================================");
    Serial.println("How to connect to a device:");
    Serial.println("1. Copy the MAC address from above");
    Serial.println("2. Use the appropriate test environment");
    Serial.println("3. Format: \"XX:XX:XX:XX:XX:XX\"");
    Serial.println("========================================");
  }

  Serial.println();
  Serial.println("Scan complete. Restart to scan again.");

  // Cleanup
  pBLEScan->clearResults();
}

void loop() {
  // Scan is done in setup, nothing to do in loop
  delay(1000);
}
