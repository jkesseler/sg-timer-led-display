#pragma once

#include <Arduino.h>
#include <functional>
#include <BLEDevice.h>

// Unified shot data structure for all timer devices
struct NormalizedShotData {
  uint32_t sessionId = 0;
  uint16_t shotNumber = 0;
  uint32_t absoluteTimeMs = 0;    // Always in milliseconds
  uint32_t splitTimeMs = 0;       // Time since previous shot
  uint64_t timestampMs = 0;       // System timestamp when shot was detected
  const char* deviceModel = nullptr;
  bool isFirstShot = false;       // True if this is the first shot in session
};

// Session state information
struct SessionData {
  uint32_t sessionId = 0;
  bool isActive = false;
  uint16_t totalShots = 0;
  uint32_t startTimestamp = 0;
  float startDelaySeconds = 0.0f;
};

// Device connection state
enum class DeviceConnectionState {
  DISCONNECTED,
  SCANNING,
  CONNECTING,
  CONNECTED,
  ERROR
};

// Abstract interface for all shot timer devices
class ITimerDevice {
public:
  virtual ~ITimerDevice() = default;

  // Connection management
  virtual bool initialize() = 0;
  virtual bool startScanning() = 0;
  virtual bool connect(BLEAddress address) = 0;
  virtual void disconnect() = 0;
  virtual DeviceConnectionState getConnectionState() const = 0;
  virtual bool isConnected() const = 0;

  // Device information
  virtual const char* getDeviceModel() const = 0;
  virtual const char* getDeviceName() const = 0;
  virtual BLEAddress getDeviceAddress() const = 0;

  // Callback registration
  virtual void onShotDetected(std::function<void(const NormalizedShotData&)> callback) = 0;
  virtual void onSessionStarted(std::function<void(const SessionData&)> callback) = 0;
  virtual void onCountdownComplete(std::function<void(const SessionData&)> callback) = 0;  // Called when start delay countdown ends
  virtual void onSessionStopped(std::function<void(const SessionData&)> callback) = 0;
  virtual void onSessionSuspended(std::function<void(const SessionData&)> callback) = 0;
  virtual void onSessionResumed(std::function<void(const SessionData&)> callback) = 0;
  virtual void onConnectionStateChanged(std::function<void(DeviceConnectionState)> callback) = 0;

  // Device capabilities
  virtual bool supportsRemoteStart() const = 0;
  virtual bool supportsShotList() const = 0;
  virtual bool supportsSessionControl() const = 0;

  // Advanced features (optional)
  virtual bool requestShotList(uint32_t sessionId) = 0;
  virtual bool startSession() = 0;
  virtual bool stopSession() = 0;

  // Update method to be called in main loop
  virtual void update() = 0;
};