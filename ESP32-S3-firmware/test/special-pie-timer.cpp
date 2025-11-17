#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Target device: Special Pie M1A2+ Timer
//
// Timer Service: FFF0 (0000fff0-0000-1000-8000-00805f9b34fb)
//   - FFF1 characteristic: Timer events (notify)
//   - FFF2 characteristic: Purpose unknown
//
// Device Info Service: 0917FE11-5D37-816D-8000-00805F9B34FB
//   - 09170002 characteristic: Firmware version
//
// Discovery: Scans for devices advertising FFF0 or 0917FE11 service UUIDs

// Global references for connection management
BLEClient *g_pClient = nullptr;
BLERemoteService *g_pService = nullptr;
BLEAdvertisedDevice *g_pTargetDevice = nullptr;
bool g_isConnected = false;
unsigned long g_lastHeartbeat = 0;

// Track shot timing for split calculation
uint32_t g_previousTimeSeconds = 0;
uint32_t g_previousTimeCentiseconds = 0;
bool g_hasPreviousShot = false;

// Session tracking
uint8_t g_currentSessionId = 0;
bool g_sessionActive = false;

// Callback for notifications
void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
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
        g_currentSessionId = pData[3];
        Serial.printf("  Session ID: 0x%02X\n", g_currentSessionId);
        g_sessionActive = true;
        g_hasPreviousShot = false;
        g_previousTimeSeconds = 0;
        g_previousTimeCentiseconds = 0;
      }
      break;

    case 0x18: // SESSION_STOP (24 decimal)
      Serial.println("SESSION_STOP");
      if (length >= 6) {
        uint8_t sessionId = pData[3];
        Serial.printf("  Session ID: 0x%02X\n", sessionId);
        g_sessionActive = false;
        g_hasPreviousShot = false;
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
        if (g_hasPreviousShot) {
          int32_t deltaSeconds = currentSeconds - g_previousTimeSeconds;
          int32_t deltaCentiseconds = currentCentiseconds - g_previousTimeCentiseconds;

          // Handle negative centiseconds (borrow from seconds)
          if (deltaCentiseconds < 0) {
            deltaSeconds -= 1;
            deltaCentiseconds += 100;
          }

          Serial.printf("  Split: %d.%02d\n", deltaSeconds, deltaCentiseconds);
        }

        // Store current shot as previous for next split calculation
        g_previousTimeSeconds = currentSeconds;
        g_previousTimeCentiseconds = currentCentiseconds;
        g_hasPreviousShot = true;
      }
      break;

    default:
      Serial.println("UNKNOWN");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== ESP32-S3 Special Pie Timer BLE Client Starting ===");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

  Serial.println("Initializing BLE...");
  BLEDevice::init("ESP32S3_SpecialPie_Client");
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

    Serial.println("Scanning for 10 seconds...");
    BLEScanResults foundDevices = pScan->start(10, false);

    // Special Pie Timer Service UUIDs
    BLEUUID timerServiceUuid("0000FFF0-0000-1000-8000-00805F9B34FB");
    BLEUUID deviceInfoServiceUuid("0917FE11-5D37-816D-8000-00805F9B34FB");
    bool deviceFound = false;

    Serial.printf("Found %d devices\n", foundDevices.getCount());

    for (int i = 0; i < foundDevices.getCount(); i++)
    {
      BLEAdvertisedDevice device = foundDevices.getDevice(i);

      Serial.printf("Device %d: %s", i, device.getAddress().toString().c_str());
      if (device.haveName()) {
        Serial.printf(" - %s", device.getName().c_str());
      }
      Serial.println();

      // Look for devices advertising the Special Pie timer service or device info service
      bool hasTimerService = device.isAdvertisingService(timerServiceUuid);
      bool hasDeviceInfoService = device.isAdvertisingService(deviceInfoServiceUuid);

      if (hasTimerService || hasDeviceInfoService)
      {
        Serial.println("*** Special Pie Timer found! ***");
        if (hasTimerService) {
          Serial.println("  - Has Timer Service (FFF0)");
        }
        if (hasDeviceInfoService) {
          Serial.println("  - Has Device Info Service (0917FE11)");
        }
        deviceFound = true;        // Wait before attempting connection
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
          BLERemoteService *pService = g_pClient->getService(timerServiceUuid);

          if (pService != nullptr)
          {
            Serial.println("Timer Service (FFF0) found");
            g_pService = pService;

            // Try to get device info service for firmware version
            BLERemoteService *pDeviceInfoService = g_pClient->getService(deviceInfoServiceUuid);
            if (pDeviceInfoService != nullptr) {
              Serial.println("Device Info Service (0917FE11) found");

              // Get firmware version characteristic
              BLERemoteCharacteristic *pFirmwareChar =
                pDeviceInfoService->getCharacteristic("09170002-5D37-816D-8000-00805F9B34FB");

              if (pFirmwareChar != nullptr && pFirmwareChar->canRead()) {
                Serial.println("Reading firmware version...");
                std::string firmwareValue = pFirmwareChar->readValue();
                if (firmwareValue.length() > 0) {
                  Serial.printf("Firmware: %s\n", firmwareValue.c_str());
                }
              }
            }

            // FFF1 characteristic for timer event notifications
            BLERemoteCharacteristic *pNotifyCharacteristic =
              pService->getCharacteristic("0000FFF1-0000-1000-8000-00805F9B34FB");

            if (pNotifyCharacteristic != nullptr)
            {
              Serial.println("FFF1 (timer events) characteristic found");

              // Check if characteristic can notify
              if (pNotifyCharacteristic->canNotify())
              {
                Serial.println("Registering for notifications on FFF1...");
                pNotifyCharacteristic->registerForNotify(notifyCallback);
                Serial.println("SUCCESS: Registered for timer event notifications!");
                Serial.println("Listening for events indefinitely...\n");
                g_isConnected = true;
                g_lastHeartbeat = millis();
              }
              else
              {
                Serial.println("ERROR: FFF1 characteristic cannot notify");
                g_pClient->disconnect();
                delete g_pClient;
                g_pClient = nullptr;
              }
            }
            else
            {
              Serial.println("ERROR: FFF1 characteristic not found");
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
      Serial.println("Target device not found. Retrying in 5 seconds...");
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
        Serial.printf("[%lu] Connected - waiting for events...\n", millis() / 1000);
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
      g_sessionActive = false;
      g_hasPreviousShot = false;

      if (g_pClient) {
        delete g_pClient;
        g_pClient = nullptr;
      }

      Serial.println("Will attempt to reconnect...\n");
      delay(2000);
    }
  }
}
