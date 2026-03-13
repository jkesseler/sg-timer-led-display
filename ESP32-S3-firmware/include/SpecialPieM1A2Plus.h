#pragma once

#include "BaseTimerDevice.h"
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEScan.h>

// Special Pie Timer Protocol Message Types
enum class SpecialPieMessageType : uint8_t {
  SESSION_STOP = 0x18,    // 24 decimal
  SESSION_START = 0x34,   // 52 decimal
  SHOT_DETECTED = 0x36    // 54 decimal
};

class SpecialPieM1A2Plus : public BaseTimerDevice {
private:
  // Special Pie Timer specific configuration
  static const char* LOG_TAG;
  static const char* CHARACTERISTIC_UUID;

  // BLE components
  BLERemoteCharacteristic* pNotifyCharacteristic;

  // Shot tracking for split calculation
  uint32_t previousTimeSeconds;
  uint32_t previousTimeCentiseconds;
  bool hasPreviousShot;
  uint8_t currentSessionId;
  bool sessionActiveFlag;

  // Internal methods
  void processTimerData(uint8_t* data, size_t length);

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

