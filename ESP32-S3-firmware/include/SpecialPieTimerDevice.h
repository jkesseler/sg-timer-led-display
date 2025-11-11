#pragma once

#include "ITimerDevice.h"
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEScan.h>

class SpecialPieTimerDevice : public ITimerDevice {
private:
  // Special Pie Timer specific configuration
  static const char* SERVICE_UUID;
  static const char* CHARACTERISTIC_UUID;

  // BLE components
  BLEClient* pClient;
  BLERemoteCharacteristic* pNotifyCharacteristic;
  BLERemoteService* pService;

  // Connection state
  DeviceConnectionState connectionState;
  bool isConnectedFlag;
  unsigned long lastReconnectAttempt;
  unsigned long lastHeartbeat;
  BLEAddress deviceAddress;
  String deviceName;

  // Device information
  String deviceModel;

  // Session tracking
  SessionData currentSession;
  uint8_t currentSessionId;
  bool sessionActiveFlag;

  // Shot tracking for split calculation
  uint32_t previousTimeSeconds;
  uint32_t previousTimeCentiseconds;
  bool hasPreviousShot;

  // Callbacks
  std::function<void(const NormalizedShotData&)> shotDetectedCallback;
  std::function<void(const SessionData&)> sessionStartedCallback;
  std::function<void(const SessionData&)> sessionStoppedCallback;
  std::function<void(const SessionData&)> sessionSuspendedCallback;
  std::function<void(const SessionData&)> sessionResumedCallback;
  std::function<void(DeviceConnectionState)> connectionStateCallback;

  // Internal methods
  void processTimerData(uint8_t* data, size_t length);
  void setConnectionState(DeviceConnectionState newState);

  // Static callback for BLE notifications
  static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify);

public:
  SpecialPieTimerDevice();
  virtual ~SpecialPieTimerDevice();

  // ITimerDevice interface implementation
  bool initialize() override;
  bool startScanning() override;
  bool connect(BLEAddress address) override;
  void disconnect() override;
  DeviceConnectionState getConnectionState() const override;
  bool isConnected() const override;

  const char* getDeviceModel() const override;
  const char* getDeviceName() const override;
  BLEAddress getDeviceAddress() const override;

  void onShotDetected(std::function<void(const NormalizedShotData&)> callback) override;
  void onSessionStarted(std::function<void(const SessionData&)> callback) override;
  void onSessionStopped(std::function<void(const SessionData&)> callback) override;
  void onSessionSuspended(std::function<void(const SessionData&)> callback) override;
  void onSessionResumed(std::function<void(const SessionData&)> callback) override;
  void onConnectionStateChanged(std::function<void(DeviceConnectionState)> callback) override;

  bool supportsRemoteStart() const override;
  bool supportsShotList() const override;
  bool supportsSessionControl() const override;

  bool requestShotList(uint32_t sessionId) override;
  bool startSession() override;
  bool stopSession() override;

  void update() override;

  // Static instance for callbacks
  static SpecialPieTimerDevice* instance;

  // Helper method to check if a device is a Special Pie Timer
  static bool isSpecialPieTimer(BLEAdvertisedDevice* device);

  // Public connection method for TimerApplication
  bool attemptConnection(BLEAdvertisedDevice* device);
};
