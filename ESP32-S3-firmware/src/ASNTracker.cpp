#include "ASNTracker.h"
#include "Logger.h"
#include "common.h"

// Static constants
const char *ASNTracker::LOG_TAG = "ASN-Tracker";
const char *ASNTracker::SERVICE_UUID = "E5A10001-F1A2-4B63-9F8C-D7B781E35E2A";
const char *ASNTracker::CHARACTERISTIC_UUID = "E5A10002-F1A2-4B63-9F8C-D7B781E35E2A";

// Static instance for callbacks
ASNTracker* ASNTracker::instance = nullptr;

ASNTracker::ASNTracker()
  : FrameProtocolTimerDevice("ASN Tracker", LOG_TAG) {
  instance = this;
}

ASNTracker::~ASNTracker() {
  disconnect();
  instance = nullptr;
}

// Static method to check if advertised device is an ASN Tracker
bool ASNTracker::matchesDevice(BLEAdvertisedDevice* device) {
  if (!device || !device->haveServiceUUID()) {
    return false;
  }
  return device->isAdvertisingService(BLEUUID(SERVICE_UUID));
}

bool ASNTracker::attemptConnection(BLEAdvertisedDevice* device) {
  if (!device) return false;

  if (device->haveName()) {
    LOG_INFO(LOG_TAG, "ASN Tracker found: %s (%s)",
             device->getName().c_str(),
             device->getAddress().toString().c_str());
  } else {
    LOG_INFO(LOG_TAG, "ASN Tracker found: %s", device->getAddress().toString().c_str());
  }

  storeDeviceInfo(device);
  return connectAndSubscribe(LOG_TAG, device, SERVICE_UUID, CHARACTERISTIC_UUID,
                             notifyCallback, &pNotifyCharacteristic);
}

// Static notification callback
void ASNTracker::notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                          uint8_t* pData, size_t length, bool isNotify) {
  if (instance && pData && length > 0) {
    instance->processTimerData(pData, length);
  }
}
