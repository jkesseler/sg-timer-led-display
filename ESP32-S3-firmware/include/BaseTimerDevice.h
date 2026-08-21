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
 * - Connection lost handling with automatic cleanup
 *
 * Derived classes typically implement:
 * - attemptConnection(BLEAdvertisedDevice*) - Device-specific connection logic
 * - processTimerData(uint8_t*, size_t) - BLE protocol-specific data parsing
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
  char deviceName[64];
  char deviceModel[32];

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

  // ---------------------------------------------------------------------------
  // Shared BLE connection helpers
  //
  // These centralize the connect -> getService -> getCharacteristic ->
  // registerForNotify sequence (and its failure cleanup) that every concrete
  // device repeated. Keeping the teardown in one place avoids the leaked-client
  // bugs that came from copy-pasted error paths.
  // ---------------------------------------------------------------------------

  // BLE notification callback signature required by the ESP32 BLE library.
  typedef void (*BLENotifyCallback)(BLERemoteCharacteristic*, uint8_t*, size_t, bool);

  // Disconnect and free the active client, if any. Safe when already null.
  void cleanupClient() {
    if (pClient) {
      pClient->disconnect();
      delete pClient;
      pClient = nullptr;
    }
  }

  // Copy the advertised device's identity into the owned buffers, falling back
  // to the address string when no name is advertised.
  void storeDeviceInfo(BLEAdvertisedDevice* device) {
    if (!device) return;
    deviceAddress = device->getAddress();
    if (device->haveName()) {
      strncpy(deviceName, device->getName().c_str(), sizeof(deviceName) - 1);
    } else {
      strncpy(deviceName, device->getAddress().toString().c_str(), sizeof(deviceName) - 1);
    }
    deviceName[sizeof(deviceName) - 1] = '\0';
  }

  // Dump a raw notification payload as hex when DEBUG logging is enabled.
  static void logNotificationBytes(const char* tag, const uint8_t* data, size_t length) {
    if (Logger::getLevel() > LogLevel::DEBUG) return;
    LOG_DEBUG(tag, "Notification received (%d bytes)", (int)length);
    for (size_t i = 0; i < length; i++) {
      Serial.printf("%02X ", data[i]);
    }
    Serial.println();
  }

  // Tear down any existing connection, pause for the BLE stack to settle, then
  // create a fresh client. Reports ERROR state and returns false on failure.
  bool beginConnection(const char* tag) {
    disconnect();

    // Blocking delay is acceptable here: connection setup is not on a hot path.
    LOG_INFO(tag, "Waiting %dms before connecting", BLE_CONNECTION_DELAY_MS);
    delay(BLE_CONNECTION_DELAY_MS);

    setConnectionState(DeviceConnectionState::CONNECTING);
    pClient = BLEDevice::createClient();
    if (!pClient) {
      LOG_ERROR(tag, "Failed to create BLE client");
      setConnectionState(DeviceConnectionState::ERROR);
      return false;
    }
    return true;
  }

  // Resolve the service + notify characteristic on an already-connected client
  // and register the callback. Cleans up the client on any failure.
  bool subscribeAfterConnect(const char* tag, const char* serviceUuid,
                             const char* charUuid, BLENotifyCallback notifyCb,
                             BLERemoteCharacteristic** outChar) {
    pService = pClient->getService(BLEUUID(serviceUuid));
    if (!pService) {
      LOG_ERROR(tag, "Service not found");
      cleanupClient();
      setConnectionState(DeviceConnectionState::ERROR);
      return false;
    }

    BLERemoteCharacteristic* characteristic = pService->getCharacteristic(charUuid);
    if (!characteristic) {
      LOG_ERROR(tag, "Notify characteristic not found");
      cleanupClient();
      setConnectionState(DeviceConnectionState::ERROR);
      return false;
    }

    if (!characteristic->canNotify()) {
      LOG_ERROR(tag, "Characteristic cannot notify");
      cleanupClient();
      setConnectionState(DeviceConnectionState::ERROR);
      return false;
    }

    LOG_INFO(tag, "Registering for notifications");
    characteristic->registerForNotify(notifyCb);
    if (outChar) *outChar = characteristic;

    isConnectedFlag = true;
    lastHeartbeat = millis();
    setConnectionState(DeviceConnectionState::CONNECTED);
    LOG_INFO(tag, "Connected - listening for events");
    return true;
  }

  // Connect to an advertised device, then subscribe to its notify characteristic.
  bool connectAndSubscribe(const char* tag, BLEAdvertisedDevice* device,
                           const char* serviceUuid, const char* charUuid,
                           BLENotifyCallback notifyCb,
                           BLERemoteCharacteristic** outChar = nullptr) {
    if (outChar) *outChar = nullptr;
    if (!beginConnection(tag)) return false;

    LOG_INFO(tag, "Attempting connection");
    if (!pClient->connect(device)) {
      LOG_ERROR(tag, "Failed to connect");
      cleanupClient();
      setConnectionState(DeviceConnectionState::ERROR);
      return false;
    }
    return subscribeAfterConnect(tag, serviceUuid, charUuid, notifyCb, outChar);
  }

  // Connect to a raw BLE address, then subscribe (for MAC/name-pattern devices).
  bool connectAndSubscribe(const char* tag, BLEAddress address,
                           const char* serviceUuid, const char* charUuid,
                           BLENotifyCallback notifyCb,
                           BLERemoteCharacteristic** outChar = nullptr) {
    if (outChar) *outChar = nullptr;
    if (!beginConnection(tag)) return false;

    LOG_INFO(tag, "Attempting connection");
    if (!pClient->connect(address)) {
      LOG_ERROR(tag, "Failed to connect");
      cleanupClient();
      setConnectionState(DeviceConnectionState::ERROR);
      return false;
    }
    return subscribeAfterConnect(tag, serviceUuid, charUuid, notifyCb, outChar);
  }

public:
  BaseTimerDevice(const char* model)
    : pClient(nullptr),
      pService(nullptr),
      connectionState(DeviceConnectionState::DISCONNECTED),
      isConnectedFlag(false),
      lastReconnectAttempt(0),
      lastHeartbeat(0),
      deviceAddress("00:00:00:00:00:00"),
      deviceName{},
      deviceModel{} {
    strncpy(deviceModel, model, sizeof(deviceModel) - 1);
  }

  virtual ~BaseTimerDevice() {
    disconnect();
  }

  // Common ITimerDevice implementations
  bool initialize() override {
    LOG_INFO("DEVICE", "Initializing %s device interface", deviceModel);
    setConnectionState(DeviceConnectionState::DISCONNECTED);
    return true;
  }

  bool startScanning() override {
    LOG_INFO("DEVICE", "Will start scanning for %s devices", deviceModel);
    setConnectionState(DeviceConnectionState::SCANNING);
    return true;
  }

  bool connect(BLEAddress address) override {
    deviceAddress = address;
    return true;
  }

  void disconnect() override {
    cleanupClient();
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
    return deviceModel;
  }

  const char* getDeviceName() const override {
    return deviceName;
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
        // Print heartbeat at regular intervals. Wording must not read like a
        // fresh connection - this fires on a fixed cadence from initial
        // connect regardless of shot activity, including mid-session.
        if (millis() - lastHeartbeat > BLE_HEARTBEAT_INTERVAL_MS) {
          LOG_BLE("%s heartbeat - connection alive", deviceModel);
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
    LOG_WARN("BLE", "Connection lost");
    isConnectedFlag = false;
    pService = nullptr;
    cleanupClient();

    setConnectionState(DeviceConnectionState::DISCONNECTED);

    // Reset session tracking
    currentSession = {};

    LOG_BLE("Will attempt to reconnect");
  }
};
