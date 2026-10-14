#include "TimerEventParser.h"
#include <ArduinoJson.h>
#include <cstring>

namespace {

void copyText(char* destination, size_t destinationSize, const char* source) {
  strncpy(destination, source, destinationSize - 1);
  destination[destinationSize - 1] = '\0';
}

bool parseConnectionState(const char* state, DeviceConnectionState& out) {
  if (strcmp(state, "DISCONNECTED") == 0) {
    out = DeviceConnectionState::DISCONNECTED;
  } else if (strcmp(state, "SCANNING") == 0) {
    out = DeviceConnectionState::SCANNING;
  } else if (strcmp(state, "CONNECTING") == 0) {
    out = DeviceConnectionState::CONNECTING;
  } else if (strcmp(state, "CONNECTED") == 0) {
    out = DeviceConnectionState::CONNECTED;
  } else if (strcmp(state, "ERROR") == 0) {
    out = DeviceConnectionState::ERROR;
  } else {
    return false;
  }
  return true;
}

}  // namespace

bool parseTimerEvent(const char* event, const uint8_t* payload, size_t length, MqttEvent& out) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) {
    return false;
  }

  if (strcmp(event, "shot/detected") == 0) {
    out.type = MqttEventType::SHOT_DETECTED;
    out.shot = NormalizedShotData();
    out.shot.sessionId = doc["sessionId"] | 0u;
    out.shot.shotNumber = doc["shotNumber"] | 0u;
    out.shot.absoluteTimeMs = doc["absoluteTimeMs"] | 0u;
    out.shot.splitTimeMs = doc["splitTimeMs"] | 0u;
    out.shot.isFirstShot = doc["isFirstShot"] | false;
    copyText(out.shot.deviceModel, sizeof(out.shot.deviceModel), doc["deviceModel"] | "");
    return true;
  }

  if (strcmp(event, "session/started") == 0) {
    out.type = MqttEventType::SESSION_STARTED;
    out.sessionStarted.sessionId = doc["sessionId"] | 0u;
    out.sessionStarted.startDelaySeconds = doc["startDelaySeconds"] | 0.0f;
    return true;
  }

  if (strcmp(event, "session/stopped") == 0) {
    out.type = MqttEventType::SESSION_STOPPED;
    out.sessionStopped.sessionId = doc["sessionId"] | 0u;
    out.sessionStopped.totalShots = doc["totalShots"] | 0u;
    out.sessionStopped.lastShotTimeMs = doc["lastShotTimeMs"] | 0u;
    return true;
  }

  if (strcmp(event, "connection/state") == 0) {
    DeviceConnectionState state;
    if (!parseConnectionState(doc["state"] | "", state)) {
      return false;
    }
    const char* deviceName = doc["deviceName"];
    const char* deviceModel = doc["deviceModel"];

    out.type = MqttEventType::CONNECTION_STATE;
    out.connectionState.state = state;
    out.connectionState.hasDeviceName = deviceName != nullptr;
    out.connectionState.hasDeviceModel = deviceModel != nullptr;
    if (deviceName) {
      copyText(out.connectionState.deviceName, sizeof(out.connectionState.deviceName), deviceName);
    }
    if (deviceModel) {
      copyText(out.connectionState.deviceModel, sizeof(out.connectionState.deviceModel), deviceModel);
    }
    return true;
  }

  if (strcmp(event, "session/suspended") == 0) {
    out.type = MqttEventType::SESSION_SUSPENDED;
  } else if (strcmp(event, "session/resumed") == 0) {
    out.type = MqttEventType::SESSION_RESUMED;
  } else if (strcmp(event, "countdown/complete") == 0) {
    out.type = MqttEventType::COUNTDOWN_COMPLETE;
  } else {
    return false;
  }

  out.sessionSimple.sessionId = doc["sessionId"] | 0u;
  return true;
}
