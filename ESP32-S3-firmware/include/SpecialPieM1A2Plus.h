#pragma once

#include "FrameProtocolTimerDevice.h"
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEScan.h>

/**
 * @brief Special Pie Timer M1A2+ with service-UUID based discovery.
 *
 * These units advertise their service UUID, so they are matched on that rather
 * than on the device name.
 *
 * Distinct hardware from the M1A2 (see SpecialPieM1A2F), which advertises no
 * service UUID and is matched by name pattern instead. The two models are
 * separate devices that happen to share the same GATT profile (FFF0/FFF1) and
 * the same F8/F9 frame protocol, so the UUIDs below match that device's.
 *
 * Frame protocol parsing is shared via FrameProtocolTimerDevice; only
 * discovery, connection, and UUIDs live here.
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

  // Shot number the device puts on the wire for the first shot of a session.
  // Special Pie M1A2+ counts from 1; the base class normalizes to 1-based.
  static constexpr uint8_t SHOT_INDEX_BASE = 1;

  // Device identification - check if advertised device is a Special Pie Timer
  static bool matchesDevice(BLEAdvertisedDevice* device);

  // Public connection method for TimerApplication
  bool attemptConnection(BLEAdvertisedDevice* device);

  // Static instance for callbacks
  static SpecialPieM1A2Plus* instance;
};
