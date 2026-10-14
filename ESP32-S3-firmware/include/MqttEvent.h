#pragma once

#include "ITimerDevice.h"

// Queued between tasks: only MqttManager's task may touch PubSubClient, which is not thread-safe.
enum class MqttEventType : uint8_t {
  SHOT_DETECTED,
  SESSION_STARTED,
  SESSION_STOPPED,
  SESSION_SUSPENDED,
  SESSION_RESUMED,
  COUNTDOWN_COMPLETE,
  CONNECTION_STATE,
  DEVICE_INFO,
};

struct MqttEvent {
  MqttEventType type;

  union {
    NormalizedShotData shot;

    struct {
      uint32_t sessionId;
      float startDelaySeconds;
    } sessionStarted;

    struct {
      uint32_t sessionId;
      uint16_t totalShots;
      uint32_t lastShotTimeMs;
    } sessionStopped;

    // Reused for SESSION_SUSPENDED / SESSION_RESUMED / COUNTDOWN_COMPLETE,
    // which all carry only a session ID.
    struct {
      uint32_t sessionId;
    } sessionSimple;

    struct {
      DeviceConnectionState state;
      bool hasDeviceName;
      bool hasDeviceModel;
      char deviceName[64];
      char deviceModel[32];
    } connectionState;

    struct {
      bool hasDeviceName;
      bool hasDeviceModel;
      bool hasFirmwareVersion;
      char deviceName[64];
      char deviceModel[32];
      char firmwareVersion[16];
    } deviceInfo;
  };

  MqttEvent() : type(MqttEventType::SHOT_DETECTED) {}
};
