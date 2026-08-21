#pragma once

#include "FrameProtocolTimerDevice.h"
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEScan.h>

/**
 * @brief Special Pie Timer M1A2 device implementation with name-based identification
 *
 * Identifies the timer by device name pattern "SP M1A2 Timer <xxxx>" where
 * <xxxx> is a 4-character identifier (e.g., "SP M1A2 Timer 2196"). These units
 * do not advertise their service UUID, so name matching is the only way to
 * recognise them during a scan.
 *
 * Distinct hardware from the M1A2+ (see SpecialPieM1A2Plus), which advertises
 * its service UUID and is discovered that way. The two models are separate
 * devices that happen to share the same GATT profile (FFF0/FFF1) and the same
 * F8/F9 frame protocol - hence the identical UUIDs here and there.
 *
 * Protocol: Frame-based with markers [F8 F9] [MESSAGE_TYPE] [DATA...] [F9 F8]
 * (shared with FrameProtocolTimerDevice).
 */
class SpecialPieM1A2F : public FrameProtocolTimerDevice {
private:
  static const char* LOG_TAG;
  static const char* CHARACTERISTIC_UUID;

  // Static callback for BLE notifications
  static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify);

public:
  SpecialPieM1A2F();
  virtual ~SpecialPieM1A2F();

  static const char* SERVICE_UUID;

  // Shot number the device puts on the wire for the first shot of a session.
  // Special Pie M1A2 counts from 1; the base class normalizes to 1-based.
  static constexpr uint8_t SHOT_INDEX_BASE = 1;

  // Public connection method for TimerApplication
  bool attemptConnection(BLEAdvertisedDevice* device);

  // Device identification - check if advertised device matches target name pattern
  static bool matchesDevice(BLEAdvertisedDevice* device);

  // Static instance for callbacks
  static SpecialPieM1A2F* instance;
};
