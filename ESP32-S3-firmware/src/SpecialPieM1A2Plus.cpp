#include "SpecialPieM1A2Plus.h"
#include "Logger.h"
#include "common.h"

// Static constants
const char* SpecialPieM1A2Plus::LOG_TAG = "SP-M1A2+";
const char* SpecialPieM1A2Plus::SERVICE_UUID = "0000FFF0-0000-1000-8000-00805F9B34FB";
const char* SpecialPieM1A2Plus::CHARACTERISTIC_UUID = "0000FFF1-0000-1000-8000-00805F9B34FB";

// Static instance for callbacks
SpecialPieM1A2Plus* SpecialPieM1A2Plus::instance = nullptr;

SpecialPieM1A2Plus::SpecialPieM1A2Plus()
  : FrameProtocolTimerDevice("Special Pie Timer", LOG_TAG, SHOT_INDEX_BASE) {
  instance = this;
}

SpecialPieM1A2Plus::~SpecialPieM1A2Plus() {
  disconnect();
  instance = nullptr;
}

// Static method to check if advertised device is a Special Pie Timer (UUID-based)
bool SpecialPieM1A2Plus::matchesDevice(BLEAdvertisedDevice* device) {
  if (!device || !device->haveServiceUUID()) {
    return false;
  }
  return device->isAdvertisingService(BLEUUID(SERVICE_UUID));
}

bool SpecialPieM1A2Plus::attemptConnection(BLEAdvertisedDevice* device) {
  if (!device) {
    return false;
  }

  if (device->haveName()) {
    LOG_INFO(LOG_TAG, "Special Pie Timer found: %s (%s)",
             device->getName().c_str(),
             device->getAddress().toString().c_str());
  } else {
    LOG_INFO(LOG_TAG, "Special Pie Timer found: %s", device->getAddress().toString().c_str());
  }

  storeDeviceInfo(device);
  return connectAndSubscribe(LOG_TAG, device, SERVICE_UUID, CHARACTERISTIC_UUID,
                             notifyCallback, &pNotifyCharacteristic);
}

// Static notification callback
void SpecialPieM1A2Plus::notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                          uint8_t* pData, size_t length, bool isNotify) {
  if (instance && pData && length > 0) {
    instance->processTimerData(pData, length);
  }
}
