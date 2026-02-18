#pragma once

#include "BaseTimerDevice.h"
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEScan.h>

// Special Pie Timer Protocol Message Types
enum class SpecialPieMacMessageType : uint8_t {
  SESSION_STOP = 0x18,    // 24 decimal
  SESSION_START = 0x34,   // 52 decimal
  SHOT_DETECTED = 0x36    // 54 decimal
};

/**
 * @brief Special Pie Timer M1A2+ device implementation with name-based identification
 *
 * Identifies Special Pie Timer by device name pattern "SP M1A2 Timer <xxxx>"
 * where <xxxx> is a 4-character identifier (e.g., "SP M1A2 Timer 2196").
 * Does not rely on service UUID advertising like other timer devices.
 *
 * Protocol: Frame-based with markers [F8 F9] [MESSAGE_TYPE] [DATA...] [F9 F8]
 * Time format: Seconds + Centiseconds (converted to milliseconds for normalization)
 */
class SpecialPieMacTimerDevice : public BaseTimerDevice {
private:
  // Special Pie Timer specific configuration
  static const char* LOG_TAG;
  static const char* CHARACTERISTIC_UUID;
  static const char* DEVICE_INFO_SERVICE_UUID;
  static const char* FIRMWARE_CHAR_UUID;

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
  bool connectToMacAddress(const char* macAddress);

  // Static callback for BLE notifications
  static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify);

public:
  SpecialPieMacTimerDevice();
  virtual ~SpecialPieMacTimerDevice();

  static const char* SERVICE_UUID;

  // Public connection method for TimerApplication
  bool attemptConnection(BLEAdvertisedDevice* device);

  // Device identification - check if advertised device matches target MAC address
  static bool matchesDevice(BLEAdvertisedDevice* device);

  // Static instance for callbacks
  static SpecialPieMacTimerDevice* instance;
};
