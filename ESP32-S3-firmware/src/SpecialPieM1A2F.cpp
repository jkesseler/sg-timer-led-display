#include "SpecialPieM1A2F.h"
#include "Logger.h"
#include "common.h"
#include <cctype>
#include <string>

// Static constants
const char* SpecialPieM1A2F::LOG_TAG = "SP-M1A2-F";
const char* SpecialPieM1A2F::SERVICE_UUID = "0000FFF0-0000-1000-8000-00805F9B34FB";
const char* SpecialPieM1A2F::CHARACTERISTIC_UUID = "0000FFF1-0000-1000-8000-00805F9B34FB";

// Static instance for callbacks
SpecialPieM1A2F* SpecialPieM1A2F::instance = nullptr;

SpecialPieM1A2F::SpecialPieM1A2F()
  : FrameProtocolTimerDevice("SP M1A2 Timer", LOG_TAG) {
  instance = this;
}

SpecialPieM1A2F::~SpecialPieM1A2F() {
  disconnect();
  instance = nullptr;
}

// Static method to check if advertised device matches SP M1A2 Timer name pattern
bool SpecialPieM1A2F::matchesDevice(BLEAdvertisedDevice* device) {
  if (!device || !device->haveName()) {
    return false;
  }

  std::string name = device->getName().c_str();
  // Validate pattern: "SP M1A2 Timer " prefix + exactly 4 alphanumeric characters
  // Example: "SP M1A2 Timer 2196"
  static const char* PREFIX = "SP M1A2 Timer ";
  static const size_t PREFIX_LEN = 14;
  static const size_t SUFFIX_LEN = 4;
  if (name.size() != PREFIX_LEN + SUFFIX_LEN) {
    return false;
  }

  if (name.substr(0, PREFIX_LEN) != PREFIX) {
     return false;
  }

  for (size_t i = PREFIX_LEN; i < PREFIX_LEN + SUFFIX_LEN; ++i) {
    if (!std::isalnum(static_cast<unsigned char>(name[i]))){
      return false;
    }
  }

  return true;
}

bool SpecialPieM1A2F::attemptConnection(BLEAdvertisedDevice* device) {
  if (!device) return false;

  if (device->haveName()) {
    LOG_INFO(LOG_TAG, "Timer found: %s (%s)",
             device->getName().c_str(),
             device->getAddress().toString().c_str());
  } else {
    LOG_INFO(LOG_TAG, "Timer found: %s", device->getAddress().toString().c_str());
  }

  // Connect via the advertised device object, not the raw MAC. These units use
  // a random-static BLE address (MSB top bits = 0b11, e.g. F4:..), and the
  // BLEAddress overload defaults to BLE_ADDR_TYPE_PUBLIC, which fails to connect
  // to a random-address peer. Passing the device preserves its address type.
  // (Discovery is still by name pattern; that's independent of how we connect.)
  storeDeviceInfo(device);
  return connectAndSubscribe(LOG_TAG, device, SERVICE_UUID,
                             CHARACTERISTIC_UUID, notifyCallback, &pNotifyCharacteristic);
}

// Static notification callback
void SpecialPieM1A2F::notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                                              uint8_t* pData, size_t length, bool isNotify) {
  if (instance && pData && length > 0) {
    instance->processTimerData(pData, length);
  }
}
