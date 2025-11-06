#include <Arduino.h>
#include <BLEDevice.h>

static const char *DEVICE_NAME_PREFIX = "SG-SST4";
static BLEUUID serviceUUID("7520FFFF-14D2-4CDA-8B6B-697C554C9311");
static BLEUUID eventUUID("75200001-14D2-4CDA-8B6B-697C554C9311");

static BLERemoteCharacteristic *pEventChar = nullptr;
static BLEAdvertisedDevice *foundDevice = nullptr;
static BLEClient *pClient = nullptr;

bool connected = false;

// --------------------
// Callback for BLE events
// --------------------
void onNotify(BLERemoteCharacteristic *pBLERemoteCharacteristic,
              uint8_t *data, size_t len, bool isNotify)
{
  if (len < 2)
    return; // invalid packet

  uint8_t eventId = data[1];

  // 0x04 = SHOT_DETECTED (from PDF spec)
  if (eventId == 0x04 && len >= 12)
  {
    uint32_t sess_id = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
    uint16_t shot_num = (data[6] << 8) | data[7];
    uint32_t shot_time = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
    float time_s = shot_time / 1000.0;
    Serial.printf("Shot #%u at %.3f s (session %lu)\n", shot_num, time_s, sess_id);
  }
}

// --------------------
// BLE Connection helpers
// --------------------
class MyClientCallback : public BLEClientCallbacks
{
  void onConnect(BLEClient *) override { Serial.println("✅ Connected"); }
  void onDisconnect(BLEClient *) override
  {
    Serial.println("❌ Disconnected");
    connected = false;
  }
};

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice) override
  {
    Serial.printf("SG-SCAN", "Found device: %s (RSSI: %d)",
                  advertisedDevice.toString().c_str(),
                  advertisedDevice.getRSSI());

    String deviceName = advertisedDevice.getName().c_str();
    if (advertisedDevice.haveName() && deviceName.startsWith(DEVICE_NAME_PREFIX))
    {
      Serial.printf("Found %s\n", deviceName.c_str());
      BLEDevice::getScan()->stop();
      foundDevice = new BLEAdvertisedDevice(advertisedDevice);
    }
  }
};

bool connectToDevice(BLEAdvertisedDevice *device)
{
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  if (!pClient->connect(device))
  {
    Serial.println("❌ Connection failed");
    return false;
  }

  Serial.printf("✅ Connected to %s (RSSI %d)\n",
                device->getName().c_str(),
                device->getRSSI());

  BLERemoteService *pService = pClient->getService(serviceUUID);
  if (!pService)
  {
    Serial.println("❌ Service not found");
    pClient->disconnect();
    return false;
  }

  pEventChar = pService->getCharacteristic(eventUUID);
  if (pEventChar && pEventChar->canNotify())
  {
    pEventChar->registerForNotify(onNotify);
    connected = true;
    Serial.println("🔔 Subscribed to EVENT notifications");
    return true;
  }

  Serial.println("❌ EVENT characteristic not found");
  return false;
}

// --------------------
// Arduino setup / loop
// --------------------
void setup()
{
  Serial.begin(115200);
  Serial.println("Scanning for Smart Shot Timer...");

  BLEDevice::init("ESP32S3_Client");
  BLEScan *scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  scan->setActiveScan(true);
  scan->start(5, false);

  while (foundDevice == nullptr)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (connectToDevice(foundDevice))
  {
    Serial.println("Ready to receive shot times!");
  }
}

void loop()
{
  // BLE notifications are handled asynchronously
  delay(1000);
}
