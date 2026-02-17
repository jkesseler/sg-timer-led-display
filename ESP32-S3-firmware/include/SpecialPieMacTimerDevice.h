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
 * @brief Special Pie Timer M1A2+ device implementation with MAC-based connection
 *
 * Connects to Special Pie Timer by MAC address instead of service UUID scanning.
 * Uses the same BLE protocol as SpecialPieTimerDevice but with direct MAC addressing.
 *
 * Protocol: Frame-based with markers [F8 F9] [MESSAGE_TYPE] [DATA...] [F9 F8]
 * Time format: Seconds + Centiseconds (converted to milliseconds for normalization)
 */
class SpecialPieMacTimerDevice : public BaseTimerDevice {
private:
  // Hardcoded MAC address for target device
  // TODO: Make this configurable via settings
  static const char* TARGET_MAC_ADDRESS;

  // Special Pie Timer specific configuration
  static const char* SERVICE_UUID;
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
  const char* getLogTag() const override;
  bool connectToMacAddress(const char* macAddress);

  // Static callback for BLE notifications
  static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify);

public:
  SpecialPieMacTimerDevice();
  virtual ~SpecialPieMacTimerDevice();

  // ITimerDevice interface implementation
  bool initialize() override;
  bool startScanning() override;
  bool connect(BLEAddress address) override;
  void disconnect() override;
  DeviceConnectionState getConnectionState() const override;
  bool isConnected() const override;

  // Device information
  const char* getDeviceModel() const override;
  const char* getDeviceName() const override;
  BLEAddress getDeviceAddress() const override;

  // Callback registration
  void onShotDetected(std::function<void(const NormalizedShotData&)> callback) override;
  void onSessionStarted(std::function<void(const SessionData&)> callback) override;
  void onCountdownComplete(std::function<void(const SessionData&)> callback) override;
  void onSessionStopped(std::function<void(const SessionData&)> callback) override;
  void onSessionSuspended(std::function<void(const SessionData&)> callback) override;
  void onSessionResumed(std::function<void(const SessionData&)> callback) override;
  void onConnectionStateChanged(std::function<void(DeviceConnectionState)> callback) override;

  // Device capabilities
  bool supportsRemoteStart() const override { return false; }
  bool supportsShotList() const override { return false; }
  bool supportsSessionControl() const override { return false; }

  // Advanced features (not supported)
  bool requestShotList(uint32_t sessionId) override { return false; }
  bool startSession() override { return false; }
  bool stopSession() override { return false; }

  // Update method
  void update() override;

  // Public connection method for TimerApplication (UUID-based)
  bool attemptConnection(BLEAdvertisedDevice* device);

  // Helper method to check if a device is a Special Pie Timer
  static bool isSpecialPieTimer(BLEAdvertisedDevice* device);

  // Static instance for callbacks
  static SpecialPieMacTimerDevice* instance;
};
