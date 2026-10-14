/**
 * @file test_timer_event_parsing.cpp
 * @brief Native tests for showing another bridge's MQTT messages (MQTT timer mode).
 *
 * Payloads match what the firmware and ble-nodejs-bridge publish.
 *
 * Runner:  GoogleTest (native)
 * Command: pio test -e native-tests --filter test_timer_event_parsing
 */

#include <gtest/gtest.h>
#include <cstring>

#include "../../src/TimerEventParser.cpp"
#include "../../src/FollowedTimer.cpp"

namespace {

bool parse(const char* event, const char* payload, MqttEvent& out) {
  return parseTimerEvent(event, reinterpret_cast<const uint8_t*>(payload), strlen(payload), out);
}

bool send(FollowedTimer& followed, const char* deviceId, const char* event, const char* payload, MqttEvent& out) {
  return followed.handle(deviceId, event, reinterpret_cast<const uint8_t*>(payload), strlen(payload), out);
}

const char* CONNECTED_STATE = R"({"state":"CONNECTED","deviceName":"SG Timer"})";
const char* SESSION = R"({"sessionId":5})";

}  // namespace

TEST(TimerEventParsing, ShotDetected) {
  MqttEvent event;
  ASSERT_TRUE(parse("shot/detected",
    R"({"sessionId":42,"shotNumber":3,"absoluteTimeMs":4321,"splitTimeMs":250,"deviceModel":"SG Timer GO","isFirstShot":false,"timestamp":1})",
    event));

  EXPECT_EQ(event.type, MqttEventType::SHOT_DETECTED);
  EXPECT_EQ(event.shot.sessionId, 42u);
  EXPECT_EQ(event.shot.shotNumber, 3u);
  EXPECT_EQ(event.shot.absoluteTimeMs, 4321u);
  EXPECT_EQ(event.shot.splitTimeMs, 250u);
  EXPECT_STREQ(event.shot.deviceModel, "SG Timer GO");
  EXPECT_FALSE(event.shot.isFirstShot);
}

TEST(TimerEventParsing, ShotDeviceModelIsTruncatedToFit) {
  MqttEvent event;
  ASSERT_TRUE(parse("shot/detected",
    R"({"sessionId":1,"shotNumber":1,"absoluteTimeMs":1,"splitTimeMs":1,"deviceModel":"A model name that is far longer than thirty-one characters","isFirstShot":true})",
    event));

  EXPECT_EQ(strlen(event.shot.deviceModel), sizeof(event.shot.deviceModel) - 1);
  EXPECT_TRUE(event.shot.isFirstShot);
}

TEST(TimerEventParsing, SessionStartedWithCountdown) {
  MqttEvent event;
  ASSERT_TRUE(parse("session/started", R"({"sessionId":7,"startDelaySeconds":2.5,"timestamp":1})", event));

  EXPECT_EQ(event.type, MqttEventType::SESSION_STARTED);
  EXPECT_EQ(event.sessionStarted.sessionId, 7u);
  EXPECT_FLOAT_EQ(event.sessionStarted.startDelaySeconds, 2.5f);
}

TEST(TimerEventParsing, SessionStoppedWithoutLastShotTime) {
  MqttEvent event;
  ASSERT_TRUE(parse("session/stopped", R"({"sessionId":7,"totalShots":0,"timestamp":1})", event));

  EXPECT_EQ(event.type, MqttEventType::SESSION_STOPPED);
  EXPECT_EQ(event.sessionStopped.sessionId, 7u);
  EXPECT_EQ(event.sessionStopped.totalShots, 0u);
  EXPECT_EQ(event.sessionStopped.lastShotTimeMs, 0u);
}

TEST(TimerEventParsing, SessionStoppedWithLastShotTime) {
  MqttEvent event;
  ASSERT_TRUE(parse("session/stopped", R"({"sessionId":7,"totalShots":5,"lastShotTimeMs":8123})", event));

  EXPECT_EQ(event.sessionStopped.totalShots, 5u);
  EXPECT_EQ(event.sessionStopped.lastShotTimeMs, 8123u);
}

TEST(TimerEventParsing, SessionOnlyEvents) {
  MqttEvent event;

  ASSERT_TRUE(parse("session/suspended", R"({"sessionId":9})", event));
  EXPECT_EQ(event.type, MqttEventType::SESSION_SUSPENDED);
  EXPECT_EQ(event.sessionSimple.sessionId, 9u);

  ASSERT_TRUE(parse("session/resumed", R"({"sessionId":9})", event));
  EXPECT_EQ(event.type, MqttEventType::SESSION_RESUMED);

  ASSERT_TRUE(parse("countdown/complete", R"({"sessionId":9})", event));
  EXPECT_EQ(event.type, MqttEventType::COUNTDOWN_COMPLETE);
}

TEST(TimerEventParsing, ConnectionStateWithDeviceName) {
  MqttEvent event;
  ASSERT_TRUE(parse("connection/state", R"({"state":"CONNECTED","deviceName":"SG-SST4A12345","deviceModel":"SG Timer"})", event));

  EXPECT_EQ(event.type, MqttEventType::CONNECTION_STATE);
  EXPECT_EQ(event.connectionState.state, DeviceConnectionState::CONNECTED);
  ASSERT_TRUE(event.connectionState.hasDeviceName);
  EXPECT_STREQ(event.connectionState.deviceName, "SG-SST4A12345");
  ASSERT_TRUE(event.connectionState.hasDeviceModel);
  EXPECT_STREQ(event.connectionState.deviceModel, "SG Timer");
}

TEST(TimerEventParsing, ConnectionStateWithoutDeviceName) {
  MqttEvent event;
  ASSERT_TRUE(parse("connection/state", R"({"state":"SCANNING"})", event));

  EXPECT_EQ(event.connectionState.state, DeviceConnectionState::SCANNING);
  EXPECT_FALSE(event.connectionState.hasDeviceName);
  EXPECT_FALSE(event.connectionState.hasDeviceModel);
}

TEST(TimerEventParsing, UnknownConnectionStateIsIgnored) {
  MqttEvent event;
  EXPECT_FALSE(parse("connection/state", R"({"state":"SLEEPING"})", event));
}

TEST(TimerEventParsing, UnusedEventsAreIgnored) {
  MqttEvent event;
  EXPECT_FALSE(parse("presence", "online", event));
  EXPECT_FALSE(parse("device/info", R"({"deviceName":"x"})", event));
  EXPECT_FALSE(parse("session/up-next", R"({"sessionId":1,"next":"Alex"})", event));
}

TEST(TimerEventParsing, MalformedJsonIsIgnored) {
  MqttEvent event;
  EXPECT_FALSE(parse("shot/detected", "{not json", event));
}

TEST(FollowedTimer, ShowsEventsOnceTheBridgeIsOnline) {
  FollowedTimer followed;
  MqttEvent event;

  EXPECT_FALSE(send(followed, "BRIDGE", "session/started", SESSION, event));
  EXPECT_FALSE(send(followed, "BRIDGE", "presence", "online", event));
  ASSERT_TRUE(send(followed, "BRIDGE", "session/started", SESSION, event));
  EXPECT_EQ(event.type, MqttEventType::SESSION_STARTED);
  EXPECT_TRUE(followed.isShowing("BRIDGE"));
}

TEST(FollowedTimer, HoldsAConnectionStateThatArrivesBeforePresence) {
  FollowedTimer followed;
  MqttEvent event;

  EXPECT_FALSE(send(followed, "BRIDGE", "connection/state", CONNECTED_STATE, event));
  ASSERT_TRUE(send(followed, "BRIDGE", "presence", "online", event));
  EXPECT_EQ(event.type, MqttEventType::CONNECTION_STATE);
  EXPECT_EQ(event.connectionState.state, DeviceConnectionState::CONNECTED);
}

TEST(FollowedTimer, CrashedBridgeShowsAsDisconnectedDespiteRetainedState) {
  FollowedTimer followed;
  MqttEvent event;

  // A killed bridge leaves connection/state CONNECTED; its last will sets presence offline.
  EXPECT_FALSE(send(followed, "BRIDGE", "connection/state", CONNECTED_STATE, event));
  EXPECT_FALSE(send(followed, "BRIDGE", "presence", "offline", event));
  EXPECT_FALSE(followed.isShowing("BRIDGE"));
}

TEST(FollowedTimer, BridgeGoingOfflineShowsDisconnected) {
  FollowedTimer followed;
  MqttEvent event;
  send(followed, "BRIDGE", "presence", "online", event);

  ASSERT_TRUE(send(followed, "BRIDGE", "presence", "offline", event));
  EXPECT_EQ(event.type, MqttEventType::CONNECTION_STATE);
  EXPECT_EQ(event.connectionState.state, DeviceConnectionState::DISCONNECTED);
  EXPECT_FALSE(send(followed, "BRIDGE", "session/started", SESSION, event));
}

TEST(FollowedTimer, FollowsOneBridgeAtATime) {
  FollowedTimer followed;
  MqttEvent event;
  send(followed, "FIRST", "presence", "online", event);

  EXPECT_FALSE(send(followed, "SECOND", "presence", "online", event));
  EXPECT_FALSE(send(followed, "SECOND", "session/started", SESSION, event));
  EXPECT_FALSE(send(followed, "SECOND", "presence", "offline", event));
  EXPECT_TRUE(followed.isShowing("FIRST"));
}

TEST(FollowedTimer, NextBridgeIsShownAfterTheFollowedOneLeaves) {
  FollowedTimer followed;
  MqttEvent event;
  send(followed, "FIRST", "presence", "online", event);
  send(followed, "FIRST", "presence", "offline", event);

  send(followed, "SECOND", "presence", "online", event);
  EXPECT_TRUE(followed.isShowing("SECOND"));
}

TEST(FollowedTimer, WaitsForPresenceAgainAfterABrokerReconnect) {
  FollowedTimer followed;
  MqttEvent event;
  send(followed, "BRIDGE", "presence", "online", event);

  followed.onBrokerReconnect();
  EXPECT_FALSE(send(followed, "BRIDGE", "connection/state", CONNECTED_STATE, event));
  ASSERT_TRUE(send(followed, "BRIDGE", "presence", "offline", event));
  EXPECT_EQ(event.connectionState.state, DeviceConnectionState::DISCONNECTED);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
