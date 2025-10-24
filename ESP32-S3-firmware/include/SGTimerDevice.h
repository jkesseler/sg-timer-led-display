#pragma once

#include "ITimerDevice.h"
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLE2902.h>

class SGTimerDevice : public ITimerDevice {
private:
  // SG Timer specific configuration
  static const char* DEVICE_NAME_PREFIX;
  static const char* SERVICE_UUID;
  static const char* CHARACTERISTIC_UUID;
  static const char* SHOT_LIST_UUID;

  // BLE components
  BLEClient* pClient;
  BLERemoteCharacteristic* pRemoteCharacteristic;
  BLERemoteCharacteristic* pShotListCharacteristic;
  BLEAddress* pServerAddress;

  // Connection state
  DeviceConnectionState connectionState;
  bool deviceConnected;
  bool doConnect;
  bool doScan;

  // Device information
  String deviceName;
  String deviceModel;

  // Session tracking
  SessionData currentSession;
  uint32_t previousShotTime;
  bool hasFirstShot;

  // Callbacks
  std::function<void(const NormalizedShotData&)> shotDetectedCallback;
  std::function<void(const SessionData&)> sessionStartedCallback;
  std::function<void(const SessionData&)> sessionStoppedCallback;
  std::function<void(const SessionData&)> sessionSuspendedCallback;
  std::function<void(const SessionData&)> sessionResumedCallback;
  std::function<void(DeviceConnectionState)> connectionStateCallback;

  // Internal methods
  void scanForDevices();
  void connectToServer();
  void processTimerData(uint8_t* data, size_t length);
  void setConnectionState(DeviceConnectionState newState);
  String parseHexData(uint8_t* data, size_t length);
  void readShotListInternal(uint32_t sessionId);

  // BLE callback classes
  class AdvertisedDeviceCallbacks;
  class ClientCallback;

  // Static callback for BLE notifications
  static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify);

public:
  SGTimerDevice();
  virtual ~SGTimerDevice();

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
  static SGTimerDevice* instance;
};