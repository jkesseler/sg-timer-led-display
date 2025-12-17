#pragma once

#include "ITimerDevice.h"
#include "Logger.h"
#include "common.h"
#include <BLEDevice.h>
#include <BLEClient.h>

/**
 * @brief Base class for timer device implementations
 *
 * Provides common functionality shared between all timer device implementations:
 * - BLE connection management
 * - Callback registration
 * - Connection state tracking
 * - Update loop with heartbeat logging
 *
 * Derived classes must implement:
 * - attemptConnectionInternal() - Device-specific connection logic
 * - processTimerData() - Protocol-specific data parsing
 * - getServiceUUID() - Return device-specific service UUID
 * - getCharacteristicUUID() - Return device-specific characteristic UUID
 */
class BaseTimerDevice : public ITimerDevice {
protected:
  // BLE components
  BLEClient* pClient;
  BLERemoteService* pService;

  // Connection state
  DeviceConnectionState connectionState;
  bool isConnectedFlag;
  unsigned long lastReconnectAttempt;
  unsigned long lastHeartbeat;
  BLEAddress deviceAddress;
  String deviceName;
  String deviceModel;

  // Session tracking
  SessionData currentSession;

  // Callbacks
  std::function<void(const NormalizedShotData&)> shotDetectedCallback;
  std::function<void(const SessionData&)> sessionStartedCallback;
  std::function<void(const SessionData&)> countdownCompleteCallback;
  std::function<void(const SessionData&)> sessionStoppedCallback;
  std::function<void(const SessionData&)> sessionSuspendedCallback;
  std::function<void(const SessionData&)> sessionResumedCallback;
  std::function<void(DeviceConnectionState)> connectionStateCallback;

  // Internal helper - sets state and notifies callback
  void setConnectionState(DeviceConnectionState newState) {
    if (connectionState != newState) {
      connectionState = newState;
      if (connectionStateCallback) {
        connectionStateCallback(newState);
      }
    }
  }

  // Pure virtual - must be implemented by derived classes
  virtual const char* getLogTag() const = 0;

public:
  BaseTimerDevice(const char* model)
    : pClient(nullptr),
      pService(nullptr),
      connectionState(DeviceConnectionState::DISCONNECTED),
      isConnectedFlag(false),
      lastReconnectAttempt(0),
      lastHeartbeat(0),
      deviceAddress("00:00:00:00:00:00"),
      deviceName(""),
      deviceModel(model) {
    currentSession = {};
  }

  virtual ~BaseTimerDevice() {
    disconnect();
  }

  // Common ITimerDevice implementations
  bool initialize() override {
    LOG_INFO(getLogTag(), "Initializing %s device interface", deviceModel.c_str());
    setConnectionState(DeviceConnectionState::DISCONNECTED);
    return true;
  }

  bool startScanning() override {
    LOG_INFO(getLogTag(), "Will start scanning for %s devices", deviceModel.c_str());
    setConnectionState(DeviceConnectionState::SCANNING);
    return true;
  }

  bool connect(BLEAddress address) override {
    deviceAddress = address;
    return true;
  }

  void disconnect() override {
    if (pClient && isConnectedFlag) {
      pClient->disconnect();
      delete pClient;
      pClient = nullptr;
    }
    isConnectedFlag = false;
    pService = nullptr;
    setConnectionState(DeviceConnectionState::DISCONNECTED);
  }

  DeviceConnectionState getConnectionState() const override {
    return connectionState;
  }

  bool isConnected() const override {
    return isConnectedFlag;
  }

  const char* getDeviceModel() const override {
    return deviceModel.c_str();
  }

  const char* getDeviceName() const override {
    return deviceName.c_str();
  }

  BLEAddress getDeviceAddress() const override {
    return deviceAddress;
  }

  // Callback registration - common implementation
  void onShotDetected(std::function<void(const NormalizedShotData&)> callback) override {
    shotDetectedCallback = callback;
  }

  void onSessionStarted(std::function<void(const SessionData&)> callback) override {
    sessionStartedCallback = callback;
  }

  void onCountdownComplete(std::function<void(const SessionData&)> callback) override {
    countdownCompleteCallback = callback;
  }

  void onSessionStopped(std::function<void(const SessionData&)> callback) override {
    sessionStoppedCallback = callback;
  }

  void onSessionSuspended(std::function<void(const SessionData&)> callback) override {
    sessionSuspendedCallback = callback;
  }

  void onSessionResumed(std::function<void(const SessionData&)> callback) override {
    sessionResumedCallback = callback;
  }

  void onConnectionStateChanged(std::function<void(DeviceConnectionState)> callback) override {
    connectionStateCallback = callback;
  }

  // Default implementations for optional features
  bool supportsRemoteStart() const override { return false; }
  bool supportsShotList() const override { return false; }
  bool supportsSessionControl() const override { return false; }
  bool requestShotList(uint32_t sessionId) override { return false; }
  bool startSession() override { return false; }
  bool stopSession() override { return false; }

  // Common update loop pattern
  void update() override {
    if (isConnectedFlag) {
      if (pClient && pClient->isConnected()) {
        // Print heartbeat at regular intervals
        if (millis() - lastHeartbeat > BLE_HEARTBEAT_INTERVAL_MS) {
          LOG_BLE("%s connected - waiting for events", deviceModel.c_str());
          lastHeartbeat = millis();
        }
      } else {
        // Connection lost
        handleConnectionLost();
      }
    }
  }

protected:
  // Can be overridden by derived classes for device-specific cleanup
  virtual void handleConnectionLost() {
    LOG_WARN(getLogTag(), "Connection lost");
    isConnectedFlag = false;
    pService = nullptr;

    if (pClient) {
      delete pClient;
      pClient = nullptr;
    }

    setConnectionState(DeviceConnectionState::DISCONNECTED);

    // Reset session tracking
    currentSession = {};

    LOG_BLE("Will attempt to reconnect");
  }
};
