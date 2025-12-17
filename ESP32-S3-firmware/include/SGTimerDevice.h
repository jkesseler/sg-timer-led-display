#pragma once

#include "BaseTimerDevice.h"
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEScan.h>

// SG Timer Protocol Event IDs (from BLE API 3.2)
enum class SGTimerEvent : uint8_t {
  SESSION_STARTED = 0x00,
  SESSION_SUSPENDED = 0x01,
  SESSION_RESUMED = 0x02,
  SESSION_STOPPED = 0x03,
  SHOT_DETECTED = 0x04,
  SESSION_SET_BEGIN = 0x05
};

class SGTimerDevice : public BaseTimerDevice {
private:
  // SG Timer specific configuration
  static const char* CHARACTERISTIC_UUID;
  static const char* SHOT_LIST_UUID;

  // BLE components (simplified)
  BLERemoteCharacteristic* pEventCharacteristic;

  // Shot tracking state
  uint32_t previousShotTime;
  bool hasFirstShot;
  uint16_t lastShotNum;
  uint32_t lastShotSeconds;
  uint32_t lastShotHundredths;
  bool hasLastShot;

  // Internal methods
  void processTimerData(uint8_t* data, size_t length);
  const char* getLogTag() const override;

  // Static callback for BLE notifications
  static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify);

public:
  SGTimerDevice();
  virtual ~SGTimerDevice();

  static const char *SERVICE_UUID;

  // Public connection method for TimerApplication
  bool attemptConnection(BLEAdvertisedDevice* device);

  // Static instance for callbacks
  static SGTimerDevice* instance;
};
