#include "FollowedTimer.h"
#include "TimerEventParser.h"
#include <cstring>

namespace {

bool payloadEquals(const uint8_t* payload, size_t length, const char* text) {
  return length == strlen(text) && memcmp(payload, text, length) == 0;
}

void copyDeviceId(char* destination, const char* deviceId) {
  strncpy(destination, deviceId, FollowedTimer::DEVICE_ID_SIZE - 1);
  destination[FollowedTimer::DEVICE_ID_SIZE - 1] = '\0';
}

}  // namespace

FollowedTimer::FollowedTimer()
  : isFollowedDeviceOnline(false),
    hasHeldConnectionState(false) {
  followedDeviceId[0] = '\0';
  heldDeviceId[0] = '\0';
}

bool FollowedTimer::isShowing(const char* deviceId) const {
  return isFollowedDeviceOnline && strcmp(deviceId, followedDeviceId) == 0;
}

void FollowedTimer::onBrokerReconnect() {
  isFollowedDeviceOnline = false;
  hasHeldConnectionState = false;
}

bool FollowedTimer::handle(const char* deviceId, const char* event, const uint8_t* payload, size_t length, MqttEvent& out) {
  if (strcmp(event, "presence") == 0) {
    return handlePresence(deviceId, payload, length, out);
  }

  if (!parseTimerEvent(event, payload, length, out)) {
    return false;
  }
  if (isShowing(deviceId)) {
    return true;
  }

  if (out.type == MqttEventType::CONNECTION_STATE) {
    hasHeldConnectionState = true;
    copyDeviceId(heldDeviceId, deviceId);
    heldConnectionState = out;
  }
  return false;
}

bool FollowedTimer::handlePresence(const char* deviceId, const uint8_t* payload, size_t length, MqttEvent& out) {
  if (payloadEquals(payload, length, "online")) {
    if (followedDeviceId[0] == '\0') {
      copyDeviceId(followedDeviceId, deviceId);
    }
    if (strcmp(deviceId, followedDeviceId) != 0) {
      return false;
    }

    isFollowedDeviceOnline = true;
    if (hasHeldConnectionState && strcmp(heldDeviceId, deviceId) == 0) {
      hasHeldConnectionState = false;
      out = heldConnectionState;
      return true;
    }
    return false;
  }

  bool isFollowedDeviceGone = payloadEquals(payload, length, "offline") && strcmp(deviceId, followedDeviceId) == 0;
  if (!isFollowedDeviceGone) {
    return false;
  }

  // Free the slot so the next bridge to come online is shown.
  followedDeviceId[0] = '\0';
  isFollowedDeviceOnline = false;

  out.type = MqttEventType::CONNECTION_STATE;
  out.connectionState.state = DeviceConnectionState::DISCONNECTED;
  out.connectionState.hasDeviceName = false;
  out.connectionState.hasDeviceModel = false;
  return true;
}
