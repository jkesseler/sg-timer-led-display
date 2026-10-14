#pragma once

#include "MqttEvent.h"
#include <cstddef>
#include <cstdint>

// Picks the one bridge a follower shows: the first with presence "online", until it goes "offline".
class FollowedTimer {
public:
  static constexpr size_t DEVICE_ID_SIZE = 33;

  FollowedTimer();

  // True when out holds an event to show.
  bool handle(const char* deviceId, const char* event, const uint8_t* payload, size_t length, MqttEvent& out);

  bool isShowing(const char* deviceId) const;

  // The broker replays retained messages after a reconnect; wait for the bridge's presence again.
  void onBrokerReconnect();

private:
  char followedDeviceId[DEVICE_ID_SIZE];
  bool isFollowedDeviceOnline;

  // A bridge's retained connection/state can arrive before its presence.
  bool hasHeldConnectionState;
  char heldDeviceId[DEVICE_ID_SIZE];
  MqttEvent heldConnectionState;

  bool handlePresence(const char* deviceId, const uint8_t* payload, size_t length, MqttEvent& out);
};
