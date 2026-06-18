#pragma once

#include "FrameProtocolTimerDevice.h"
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEScan.h>

/**
 * @brief ASN Tracker device.
 *
 * Uses the same F8/F9 frame protocol as the Special Pie timers (handled by
 * FrameProtocolTimerDevice) with its own service/characteristic UUIDs.
 */
class ASNTracker : public FrameProtocolTimerDevice {
private:
  static const char* LOG_TAG;
  static const char* CHARACTERISTIC_UUID;

  // Static callback for BLE notifications
  static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify);

public:
  ASNTracker();
  virtual ~ASNTracker();

  static const char *SERVICE_UUID;

  // Device identification - check if advertised device is an ASN Tracker
  static bool matchesDevice(BLEAdvertisedDevice* device);

  // Public connection method for TimerApplication
  bool attemptConnection(BLEAdvertisedDevice* device);

  // Static instance for callbacks
  static ASNTracker* instance;
};
