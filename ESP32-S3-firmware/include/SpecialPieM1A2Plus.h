#pragma once

#include "FrameProtocolTimerDevice.h"
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEScan.h>

/**
 * @brief Special Pie Timer M1A2+ with service-UUID based discovery.
 *
 * Shares the F8/F9 frame protocol parsing with FrameProtocolTimerDevice;
 * only discovery, connection, and UUIDs are device-specific.
 */
class SpecialPieM1A2Plus : public FrameProtocolTimerDevice {
private:
  static const char* LOG_TAG;
  static const char* CHARACTERISTIC_UUID;

  // Static callback for BLE notifications
  static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify);

public:
  SpecialPieM1A2Plus();
  virtual ~SpecialPieM1A2Plus();

  static const char *SERVICE_UUID;

  // Device identification - check if advertised device is a Special Pie Timer
  static bool matchesDevice(BLEAdvertisedDevice* device);

  // Public connection method for TimerApplication
  bool attemptConnection(BLEAdvertisedDevice* device);

  // Static instance for callbacks
  static SpecialPieM1A2Plus* instance;
};
