#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Discovers any SG Timer device by service UUID
// Example device: Name: SG-SST4B11880, Address: dd:0e:9d:04:72:c3

// Global references for connection management
BLEClient *g_pClient = nullptr;
BLERemoteService *g_pService = nullptr;
BLEAdvertisedDevice *g_pTargetDevice = nullptr;
bool g_isConnected = false;
unsigned long g_lastHeartbeat = 0;
String g_deviceName = "";
String g_deviceModel = "SG Timer";

// Track last shot received
uint16_t g_lastShotNum = 0;
uint32_t g_lastShotSeconds = 0;
uint32_t g_lastShotHundredths = 0;
bool g_hasLastShot = false;

// Callback for notifications
void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
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
        }
        break;

      case 0x01: // SESSION_SUSPENDED
        Serial.println("SESSION_SUSPENDED");
        if (length >= 8) {
          uint32_t sess_id = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
          uint16_t total_shots = (pData[6] << 8) | pData[7];
          Serial.printf("  Session ID: %u\n", sess_id);
          Serial.printf("  Total Shots: %u\n", total_shots);

          // NOTE: SHOT_LIST may only work for saved/stopped sessions, not suspended ones
          Serial.println("  (Session suspended - shots may not be readable until stopped)");
        }
        break;

      case 0x02: // SESSION_RESUMED
        Serial.println("SESSION_RESUMED");
        if (length >= 8) {
          uint32_t sess_id = (pData[2] << 24) | (pData[3] << 16) | (pData[4] << 8) | pData[5];
          uint16_t total_shots = (pData[6] << 8) | pData[7];
          Serial.printf("  Session ID: %u\n", sess_id);
          Serial.printf("  Total Shots: %u\n", total_shots);
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
          if (g_hasLastShot) {
            Serial.printf("  Last Shot: #%u: %u:%02u\n", g_lastShotNum + 1, g_lastShotSeconds, g_lastShotHundredths);
          }

          // Reset last shot tracking for next session
          g_hasLastShot = false;
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
          g_lastShotNum = shot_num;
          g_lastShotSeconds = seconds;
          g_lastShotHundredths = hundredths;
          g_hasLastShot = true;
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

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== ESP32-S3 BLE Client Starting ===");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

  Serial.println("Initializing BLE...");
  BLEDevice::init("ESP32S3_Client");
  delay(2000); // Give time for serial to stabilize

  Serial.println("Setup complete. Moving to loop for connection handling...");
}

void loop() {
  // If not connected, try to scan and connect
  if (!g_isConnected) {
    Serial.println("\n--- Starting device scan ---");

    BLEScan *pScan = BLEDevice::getScan();
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);

    Serial.println("Scanning for SG Timer devices (10 seconds)...");
    BLEScanResults foundDevices = pScan->start(10, false);

    BLEUUID serviceUuid("7520FFFF-14D2-4CDA-8B6B-697C554C9311");
    bool deviceFound = false;

    // Look for any device advertising the SG Timer service UUID
    for (int i = 0; i < foundDevices.getCount(); i++)
    {
      BLEAdvertisedDevice device = foundDevices.getDevice(i);

      // Check if device advertises the SG Timer service
      if (device.isAdvertisingService(serviceUuid))
      {
        Serial.printf("SG Timer found: %s", device.getAddress().toString().c_str());
        if (device.haveName()) {
          Serial.printf(" (%s)", device.getName().c_str());
        }
        Serial.println();
        deviceFound = true;

        // Store device information
        if (device.haveName()) {
          g_deviceName = device.getName().c_str();
          // Extract model from name (SG-SST4XYYYYY where X is model identifier)
          if (g_deviceName.startsWith("SG-SST4")) {
            char modelId = g_deviceName.charAt(7);
            if (modelId == 'A') {
              g_deviceModel = "SG Timer Sport";
            } else if (modelId == 'B') {
              g_deviceModel = "SG Timer GO";
            } else {
              g_deviceModel = "SG Timer";
            }
          }
          Serial.printf("Device Model: %s\n", g_deviceModel.c_str());
        } else {
          g_deviceName = device.getAddress().toString().c_str();
        }

        // Wait before attempting connection
        Serial.println("Waiting 2 seconds before connecting...");
        delay(2000);

        g_pClient = BLEDevice::createClient();

        if (!g_pClient)
        {
          Serial.println("ERROR: Failed to create client");
          break;
        }

        Serial.println("Attempting connection...");
        if (g_pClient->connect(&device))
        {
          Serial.println("Connected to device!");
          BLERemoteService *pService = g_pClient->getService(serviceUuid);

          if (pService != nullptr)
          {
            Serial.println("Service found");
            g_pService = pService;

            BLERemoteCharacteristic *pEventCharacteristic = pService->getCharacteristic("75200001-14D2-4CDA-8B6B-697C554C9311");

            if (pEventCharacteristic != nullptr)
            {
              Serial.println("EVENT characteristic found");

              // Check if characteristic can notify
              if (pEventCharacteristic->canNotify())
              {
                Serial.println("Registering for notifications...");
                pEventCharacteristic->registerForNotify(notifyCallback);
                Serial.println("SUCCESS: Registered for notifications!");
                Serial.println("Listening for events indefinitely...\n");
                g_isConnected = true;
                g_lastHeartbeat = millis();
              }
              else
              {
                Serial.println("ERROR: Characteristic cannot notify");
                g_pClient->disconnect();
                delete g_pClient;
                g_pClient = nullptr;
              }
            }
            else
            {
              Serial.println("ERROR: EVENT characteristic not found");
              g_pClient->disconnect();
              delete g_pClient;
              g_pClient = nullptr;
            }
          }
          else
          {
            Serial.println("ERROR: Service not found");
            g_pClient->disconnect();
            delete g_pClient;
            g_pClient = nullptr;
          }
        }
        else
        {
          Serial.println("ERROR: Failed to connect");
          delete g_pClient;
          g_pClient = nullptr;
        }

        break;
      }
    }

    pScan->clearResults();

    if (!deviceFound) {
      Serial.println("No SG Timer devices found. Retrying in 5 seconds...");
      delay(5000);
    } else if (!g_isConnected) {
      Serial.println("Connection failed. Retrying in 5 seconds...");
      delay(5000);
    }
  }
  else
  {
    // Connected - check connection status and print heartbeat
    if (g_pClient && g_pClient->isConnected()) {

      // Print heartbeat every 30 seconds
      if (millis() - g_lastHeartbeat > 30000) {
        // Serial.printf("[%lu] Connected - waiting for events...\n", millis() / 1000);
        g_lastHeartbeat = millis();
      }
      delay(100); // Small delay to keep loop responsive
    }
    else
    {
      // Connection lost
      Serial.println("\n!!! Connection lost !!!");
      g_isConnected = false;
      g_pService = nullptr;

      if (g_pClient) {
        delete g_pClient;
        g_pClient = nullptr;
      }

      Serial.println("Will attempt to reconnect...\n");
      delay(2000);
    }
  }
}