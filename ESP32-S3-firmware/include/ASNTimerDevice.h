#pragma once

#include "BaseTimerDevice.h"
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEScan.h>

// ASN Timer Protocol Message Types (matches Special Pie)
enum class ASNMessageType : uint8_t {
  SESSION_STOP = 0x18,    // 24 decimal
  SESSION_START = 0x34,   // 52 decimal
  SHOT_DETECTED = 0x36    // 54 decimal
};

class ASNTimerDevice : public BaseTimerDevice {
private:
  // ASN Timer specific configuration
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
  const char* getLogTag() const override;

  // Static callback for BLE notifications
  static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify);

public:
  ASNTimerDevice();
  virtual ~ASNTimerDevice();

  static const char *SERVICE_UUID;

  // Public connection method for TimerApplication
  bool attemptConnection(BLEAdvertisedDevice* device);

  // Static instance for callbacks
  static ASNTimerDevice* instance;
};
