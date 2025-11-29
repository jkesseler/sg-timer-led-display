#pragma once

#include "ITimerDevice.h"
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

class SGTimerDevice : public ITimerDevice {
private:
  // SG Timer specific configuration
  static const char* CHARACTERISTIC_UUID;
  static const char* SHOT_LIST_UUID;

  // BLE components (simplified)
  BLEClient* pClient;
  BLERemoteCharacteristic* pEventCharacteristic;
  BLERemoteService* pService;

  // Connection state
  DeviceConnectionState connectionState;
  bool isConnectedFlag;
  unsigned long lastReconnectAttempt;
  unsigned long lastHeartbeat;

  // Device information
  String deviceModel;
  String deviceName;
  BLEAddress deviceAddress;

  // Session tracking
  SessionData currentSession;
  uint32_t previousShotTime;
  bool hasFirstShot;
  uint16_t lastShotNum;
  uint32_t lastShotSeconds;
  uint32_t lastShotHundredths;
  bool hasLastShot;

  // Callbacks
  std::function<void(const NormalizedShotData&)> shotDetectedCallback;
  std::function<void(const SessionData&)> sessionStartedCallback;
  std::function<void(const SessionData&)> sessionStoppedCallback;
  std::function<void(const SessionData&)> sessionSuspendedCallback;
  std::function<void(const SessionData&)> sessionResumedCallback;
  std::function<void(DeviceConnectionState)> connectionStateCallback;

  // Internal methods (simplified)
  void processTimerData(uint8_t* data, size_t length);
  void setConnectionState(DeviceConnectionState newState);

  // Static callback for BLE notifications
  static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                           uint8_t* pData, size_t length, bool isNotify);

public:
  SGTimerDevice();
  virtual ~SGTimerDevice();

  static const char *SERVICE_UUID;

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

  // Public connection method for TimerApplication
  void attemptConnection();
};