/**
 * @file test_protocol_parsing.cpp
 * @brief Native tests for BLE protocol parsing across all timer devices.
 *
 * Tests the processTimerData() methods of each device implementation by
 * feeding crafted byte arrays and verifying the NormalizedShotData output
 * via registered callbacks.
 *
 * Runner:  GoogleTest (native)
 * Command: pio test -e native-tests --filter test_protocol_parsing
 */

// ── Standard headers FIRST (before access-specifier override) ───
#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>

// ── Override access specifiers so we can reach processTimerData() ─
#define private   public
#define protected public

// ── Include real source files (stubs resolve Arduino/BLE headers) ─
#include "../../src/Logger.cpp"
#include "../../src/SGTimerDevice.cpp"
#include "../../src/SpecialPieTimerDevice.cpp"
#include "../../src/ASNTrackerDevice.cpp"
#include "../../src/SpecialPieMacTimerDevice.cpp"

#undef private
#undef protected

// ═════════════════════════════════════════════════════════════════
//  Helpers
// ═════════════════════════════════════════════════════════════════

class ProtocolTestBase : public ::testing::Test {
protected:
  void SetUp() override {
    ArduinoMock::resetMillis();
    ArduinoMock::setMillis(1000);       // Deterministic timestamp
    Logger::setLevel(LogLevel::NONE);   // Suppress log noise
    callbackCalled = false;
    sessionCallbackCalled = false;
    countdownCallbackCalled = false;
  }

  // Shared callback state used by derived fixtures
  NormalizedShotData receivedShot{};
  SessionData receivedSession{};
  bool callbackCalled = false;
  bool sessionCallbackCalled = false;
  bool countdownCallbackCalled = false;
};

// ═════════════════════════════════════════════════════════════════
//  SG Timer Protocol Tests
// ═════════════════════════════════════════════════════════════════

class SGTimerProtocolTest : public ProtocolTestBase {
protected:
  SGTimerDevice device;

  void SetUp() override {
    ProtocolTestBase::SetUp();
    device.onShotDetected([this](const NormalizedShotData& d) {
      receivedShot = d;
      callbackCalled = true;
    });
    device.onSessionStarted([this](const SessionData& d) {
      receivedSession = d;
      sessionCallbackCalled = true;
    });
    device.onSessionStopped([this](const SessionData& d) {
      receivedSession = d;
      sessionCallbackCalled = true;
    });
    device.onCountdownComplete([this](const SessionData& d) {
      receivedSession = d;
      countdownCallbackCalled = true;
    });
  }
};

// ── SESSION_STARTED ──────────────────────────────────────────────

TEST_F(SGTimerProtocolTest, SessionStarted_ParsesSessionIdAndDelay) {
  // Packet: len=7, event=0x00 (SESSION_STARTED), sessId=1, delay=30 (3.0s)
  uint8_t pkt[] = { 0x07, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x1E };
  device.processTimerData(pkt, sizeof(pkt));

  EXPECT_TRUE(sessionCallbackCalled);
  EXPECT_EQ(receivedSession.sessionId, 1u);
  EXPECT_NEAR(receivedSession.startDelaySeconds, 3.0f, 0.01f);
  EXPECT_TRUE(receivedSession.isActive);
}

TEST_F(SGTimerProtocolTest, SessionStarted_ZeroDelay) {
  uint8_t pkt[] = { 0x07, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00 };
  device.processTimerData(pkt, sizeof(pkt));

  EXPECT_TRUE(sessionCallbackCalled);
  EXPECT_EQ(receivedSession.sessionId, 2u);
  EXPECT_FLOAT_EQ(receivedSession.startDelaySeconds, 0.0f);
}

// ── SHOT_DETECTED ────────────────────────────────────────────────

TEST_F(SGTimerProtocolTest, ShotDetected_FirstShot) {
  // len=11, event=0x04, sessId=1, shotNum=0 (0-based), timeMs=1500
  uint8_t pkt[] = {
    0x0B, 0x04,
    0x00, 0x00, 0x00, 0x01,   // session ID = 1
    0x00, 0x00,               // shot number = 0 (0-based)
    0x00, 0x00, 0x05, 0xDC    // time = 1500 ms
  };
  device.processTimerData(pkt, sizeof(pkt));

  EXPECT_TRUE(callbackCalled);
  EXPECT_EQ(receivedShot.shotNumber, 1);           // Converted to 1-based
  EXPECT_EQ(receivedShot.absoluteTimeMs, 1500u);
  EXPECT_EQ(receivedShot.splitTimeMs, 0u);          // First shot has no split
  EXPECT_TRUE(receivedShot.isFirstShot);
  EXPECT_EQ(receivedShot.sessionId, 1u);
}

TEST_F(SGTimerProtocolTest, ShotDetected_SplitTimeCalculation) {
  // First shot at 1500 ms
  uint8_t shot1[] = {
    0x0B, 0x04,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00,
    0x00, 0x00, 0x05, 0xDC    // 1500 ms
  };
  device.processTimerData(shot1, sizeof(shot1));
  ASSERT_TRUE(callbackCalled);
  EXPECT_TRUE(receivedShot.isFirstShot);

  // Reset callback
  callbackCalled = false;

  // Second shot at 2200 ms → split = 700 ms
  uint8_t shot2[] = {
    0x0B, 0x04,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x01,               // shot number = 1
    0x00, 0x00, 0x08, 0x98    // 2200 ms
  };
  device.processTimerData(shot2, sizeof(shot2));

  EXPECT_TRUE(callbackCalled);
  EXPECT_EQ(receivedShot.shotNumber, 2);
  EXPECT_EQ(receivedShot.absoluteTimeMs, 2200u);
  EXPECT_EQ(receivedShot.splitTimeMs, 700u);
  EXPECT_FALSE(receivedShot.isFirstShot);
}

TEST_F(SGTimerProtocolTest, ShotDetected_LargeTime) {
  // Shot at 65,000 ms (65 seconds) — tests multi-byte parsing
  uint8_t pkt[] = {
    0x0B, 0x04,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x05,               // shot number = 5
    0x00, 0x00, 0xFD, 0xE8    // 65000 ms
  };
  device.processTimerData(pkt, sizeof(pkt));

  EXPECT_TRUE(callbackCalled);
  EXPECT_EQ(receivedShot.shotNumber, 6);
  EXPECT_EQ(receivedShot.absoluteTimeMs, 65000u);
}

TEST_F(SGTimerProtocolTest, ShotDetected_DeviceModelStored) {
  uint8_t pkt[] = {
    0x0B, 0x04,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00,
    0x00, 0x00, 0x03, 0xE8    // 1000 ms
  };
  device.processTimerData(pkt, sizeof(pkt));

  EXPECT_TRUE(callbackCalled);
  // SGTimerDevice initializes deviceModel via BaseTimerDevice("SG Timer")
  EXPECT_STREQ(receivedShot.deviceModel, "SG Timer");
}

// ── SESSION_STOPPED ──────────────────────────────────────────────

TEST_F(SGTimerProtocolTest, SessionStopped_ParsesTotalShots) {
  // len=7, event=0x03, sessId=1, totalShots=5
  uint8_t pkt[] = { 0x07, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x05 };
  device.processTimerData(pkt, sizeof(pkt));

  EXPECT_TRUE(sessionCallbackCalled);
  EXPECT_EQ(receivedSession.totalShots, 5);
  EXPECT_FALSE(receivedSession.isActive);
}

// ── SESSION_SET_BEGIN (countdown complete) ────────────────────────

TEST_F(SGTimerProtocolTest, SessionSetBegin_TriggersCountdownComplete) {
  // len=5, event=0x05, sessId=1
  uint8_t pkt[] = { 0x05, 0x05, 0x00, 0x00, 0x00, 0x01 };
  device.processTimerData(pkt, sizeof(pkt));

  EXPECT_TRUE(countdownCallbackCalled);
}

// ── Validation / rejection ───────────────────────────────────────

TEST_F(SGTimerProtocolTest, RejectsNullData) {
  device.processTimerData(nullptr, 10);
  EXPECT_FALSE(callbackCalled);
}

TEST_F(SGTimerProtocolTest, RejectsZeroLength) {
  uint8_t pkt[] = { 0x00 };
  device.processTimerData(pkt, 0);
  EXPECT_FALSE(callbackCalled);
}

TEST_F(SGTimerProtocolTest, RejectsTooShortPacket) {
  uint8_t pkt[] = { 0x01 };
  device.processTimerData(pkt, 1);
  EXPECT_FALSE(callbackCalled);
}

TEST_F(SGTimerProtocolTest, RejectsLengthFieldMismatch) {
  // Length field says 5 bytes follow, but packet is 8 bytes total (field says 5, actual=7)
  uint8_t pkt[] = { 0x05, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00 };
  device.processTimerData(pkt, sizeof(pkt));
  EXPECT_FALSE(callbackCalled);
}

TEST_F(SGTimerProtocolTest, ShotDetected_TooShortForShot) {
  // Valid length field but not enough data for SHOT_DETECTED (needs >=12)
  uint8_t pkt[] = { 0x07, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00 };
  device.processTimerData(pkt, sizeof(pkt));
  EXPECT_FALSE(callbackCalled);  // Not enough data to parse shot
}


// ═════════════════════════════════════════════════════════════════
//  Special Pie Timer Protocol Tests
// ═════════════════════════════════════════════════════════════════

class SpecialPieProtocolTest : public ProtocolTestBase {
protected:
  SpecialPieTimerDevice device;

  void SetUp() override {
    ProtocolTestBase::SetUp();
    device.onShotDetected([this](const NormalizedShotData& d) {
      receivedShot = d;
      callbackCalled = true;
    });
    device.onSessionStarted([this](const SessionData& d) {
      receivedSession = d;
      sessionCallbackCalled = true;
    });
    device.onSessionStopped([this](const SessionData& d) {
      receivedSession = d;
      sessionCallbackCalled = true;
    });
    device.onCountdownComplete([this](const SessionData& d) {
      countdownCallbackCalled = true;
    });
  }
};

// ── SESSION_START ────────────────────────────────────────────────

TEST_F(SpecialPieProtocolTest, SessionStart_ParsesSessionId) {
  // F8 F9 34 <sessId> XX XX F9 F8
  uint8_t pkt[] = { 0xF8, 0xF9, 0x34, 0x05, 0x00, 0x00, 0xF9, 0xF8 };
  device.processTimerData(pkt, sizeof(pkt));

  EXPECT_TRUE(sessionCallbackCalled);
  EXPECT_EQ(receivedSession.sessionId, 5u);
  EXPECT_TRUE(receivedSession.isActive);
  EXPECT_FLOAT_EQ(receivedSession.startDelaySeconds, 0.0f);  // SP doesn't report delay
}

TEST_F(SpecialPieProtocolTest, SessionStart_AlsoTriggersCountdownComplete) {
  uint8_t pkt[] = { 0xF8, 0xF9, 0x34, 0x01, 0x00, 0x00, 0xF9, 0xF8 };
  device.processTimerData(pkt, sizeof(pkt));

  EXPECT_TRUE(countdownCallbackCalled);  // SP fires countdown immediately
}

// ── SHOT_DETECTED ────────────────────────────────────────────────

TEST_F(SpecialPieProtocolTest, ShotDetected_TimeConversion) {
  // F8 F9 36 00 <sec> <cs> <shot#> <checksum?> F9 F8
  // Shot 0 at 3.45 seconds (3 sec, 45 centiseconds)
  uint8_t pkt[] = { 0xF8, 0xF9, 0x36, 0x00, 0x03, 0x2D, 0x00, 0x00, 0xF9, 0xF8 };
  device.processTimerData(pkt, sizeof(pkt));

  EXPECT_TRUE(callbackCalled);
  EXPECT_EQ(receivedShot.shotNumber, 1);              // 0-based → 1-based
  EXPECT_EQ(receivedShot.absoluteTimeMs, 3450u);      // 3*1000 + 45*10
  EXPECT_TRUE(receivedShot.isFirstShot);
}

TEST_F(SpecialPieProtocolTest, ShotDetected_SplitCalculation) {
  // First shot at 2.30 seconds
  uint8_t shot1[] = { 0xF8, 0xF9, 0x36, 0x00,  0x02, 0x1E, 0x00, 0x00,  0xF9, 0xF8 };
  device.processTimerData(shot1, sizeof(shot1));
  ASSERT_TRUE(callbackCalled);
  EXPECT_EQ(receivedShot.absoluteTimeMs, 2300u);  // 2s, 30cs

  callbackCalled = false;

  // Second shot at 3.05 seconds → split = 750 ms
  uint8_t shot2[] = { 0xF8, 0xF9, 0x36, 0x00,  0x03, 0x05, 0x01, 0x00,  0xF9, 0xF8 };
  device.processTimerData(shot2, sizeof(shot2));

  EXPECT_TRUE(callbackCalled);
  EXPECT_EQ(receivedShot.shotNumber, 2);
  EXPECT_EQ(receivedShot.absoluteTimeMs, 3050u);     // 3*1000 + 5*10
  EXPECT_EQ(receivedShot.splitTimeMs, 750u);          // 3050 - 2300
  EXPECT_FALSE(receivedShot.isFirstShot);
}

TEST_F(SpecialPieProtocolTest, ShotDetected_CentisecondsBorrowFromSeconds) {
  // First shot at 2.80 seconds
  uint8_t shot1[] = { 0xF8, 0xF9, 0x36, 0x00,  0x02, 0x50, 0x00, 0x00,  0xF9, 0xF8 };
  device.processTimerData(shot1, sizeof(shot1));
  ASSERT_TRUE(callbackCalled);
  EXPECT_EQ(receivedShot.absoluteTimeMs, 2800u);  // 2s, 80cs

  callbackCalled = false;

  // Second shot at 3.15 seconds (cs < previous cs → borrow)
  // split via delta: seconds 3-2=1, cs 15-80=-65 → borrow → 0 sec, 35 cs = 350 ms
  uint8_t shot2[] = { 0xF8, 0xF9, 0x36, 0x00,  0x03, 0x0F, 0x01, 0x00,  0xF9, 0xF8 };
  device.processTimerData(shot2, sizeof(shot2));

  EXPECT_TRUE(callbackCalled);
  EXPECT_EQ(receivedShot.absoluteTimeMs, 3150u);     // 3*1000 + 15*10
  EXPECT_EQ(receivedShot.splitTimeMs, 350u);          // 3150 - 2800
}

// ── SESSION_STOP ─────────────────────────────────────────────────

TEST_F(SpecialPieProtocolTest, SessionStop_SetsInactive) {
  // Start a session first
  uint8_t start[] = { 0xF8, 0xF9, 0x34, 0x01, 0x00, 0x00, 0xF9, 0xF8 };
  device.processTimerData(start, sizeof(start));

  sessionCallbackCalled = false;

  // Stop the session
  uint8_t stop[] = { 0xF8, 0xF9, 0x18, 0x01, 0x00, 0x00, 0xF9, 0xF8 };
  device.processTimerData(stop, sizeof(stop));

  EXPECT_TRUE(sessionCallbackCalled);
  EXPECT_FALSE(receivedSession.isActive);
}

// ── Validation / rejection ───────────────────────────────────────

TEST_F(SpecialPieProtocolTest, RejectsInvalidFrameMarkers) {
  uint8_t pkt[] = { 0xAA, 0xBB, 0x36, 0x00, 0x03, 0x2D, 0x00, 0x00, 0xF9, 0xF8 };
  device.processTimerData(pkt, sizeof(pkt));
  EXPECT_FALSE(callbackCalled);
}

TEST_F(SpecialPieProtocolTest, RejectsInvalidEndMarkers) {
  uint8_t pkt[] = { 0xF8, 0xF9, 0x36, 0x00, 0x03, 0x2D, 0x00, 0x00, 0xAA, 0xBB };
  device.processTimerData(pkt, sizeof(pkt));
  EXPECT_FALSE(callbackCalled);
}

TEST_F(SpecialPieProtocolTest, RejectsTooShortFrame) {
  uint8_t pkt[] = { 0xF8, 0xF9, 0x36, 0xF9, 0xF8 };
  device.processTimerData(pkt, sizeof(pkt));
  EXPECT_FALSE(callbackCalled);
}

TEST_F(SpecialPieProtocolTest, RejectsNullData) {
  device.processTimerData(nullptr, 10);
  EXPECT_FALSE(callbackCalled);
}


// ═════════════════════════════════════════════════════════════════
//  ASN Tracker Protocol Tests
// ═════════════════════════════════════════════════════════════════
//
// ASN Tracker uses the same frame protocol as Special Pie Timer
// (F8 F9 ... F9 F8) with different UUIDs. These tests verify the
// ASNTrackerDevice implementation handles it correctly.
// ═════════════════════════════════════════════════════════════════

class ASNTrackerProtocolTest : public ProtocolTestBase {
protected:
  ASNTrackerDevice device;

  void SetUp() override {
    ProtocolTestBase::SetUp();
    device.onShotDetected([this](const NormalizedShotData& d) {
      receivedShot = d;
      callbackCalled = true;
    });
    device.onSessionStarted([this](const SessionData& d) {
      receivedSession = d;
      sessionCallbackCalled = true;
    });
    device.onSessionStopped([this](const SessionData& d) {
      receivedSession = d;
      sessionCallbackCalled = true;
    });
  }
};

TEST_F(ASNTrackerProtocolTest, ShotDetected_TimeConversion) {
  // Same frame format as Special Pie
  uint8_t pkt[] = { 0xF8, 0xF9, 0x36, 0x00,  0x05, 0x32, 0x02, 0x00,  0xF9, 0xF8 };
  //                                           5s    50cs   shot#2
  device.processTimerData(pkt, sizeof(pkt));

  EXPECT_TRUE(callbackCalled);
  EXPECT_EQ(receivedShot.shotNumber, 3);            // 2 → 3 (1-based)
  EXPECT_EQ(receivedShot.absoluteTimeMs, 5500u);    // 5*1000 + 50*10
  EXPECT_TRUE(receivedShot.isFirstShot);             // First shot in session
}

TEST_F(ASNTrackerProtocolTest, SessionStart) {
  uint8_t pkt[] = { 0xF8, 0xF9, 0x34, 0x0A, 0x00, 0x00, 0xF9, 0xF8 };
  device.processTimerData(pkt, sizeof(pkt));

  EXPECT_TRUE(sessionCallbackCalled);
  EXPECT_EQ(receivedSession.sessionId, 0x0Au);
  EXPECT_TRUE(receivedSession.isActive);
}

TEST_F(ASNTrackerProtocolTest, SessionStop) {
  // Start first
  uint8_t start[] = { 0xF8, 0xF9, 0x34, 0x01, 0x00, 0x00, 0xF9, 0xF8 };
  device.processTimerData(start, sizeof(start));
  sessionCallbackCalled = false;

  // Stop
  uint8_t stop[] = { 0xF8, 0xF9, 0x18, 0x01, 0x00, 0x00, 0xF9, 0xF8 };
  device.processTimerData(stop, sizeof(stop));

  EXPECT_TRUE(sessionCallbackCalled);
  EXPECT_FALSE(receivedSession.isActive);
}

TEST_F(ASNTrackerProtocolTest, DeviceModelSetCorrectly) {
  uint8_t pkt[] = { 0xF8, 0xF9, 0x36, 0x00,  0x01, 0x00, 0x00, 0x00,  0xF9, 0xF8 };
  device.processTimerData(pkt, sizeof(pkt));

  EXPECT_TRUE(callbackCalled);
  EXPECT_STREQ(receivedShot.deviceModel, "ASN Tracker");
}

TEST_F(ASNTrackerProtocolTest, MultiShotSplitAccumulation) {
  // Shot 0 at 1.50s
  uint8_t s1[] = { 0xF8, 0xF9, 0x36, 0x00,  0x01, 0x32, 0x00, 0x00,  0xF9, 0xF8 };
  device.processTimerData(s1, sizeof(s1));
  EXPECT_EQ(receivedShot.absoluteTimeMs, 1500u);
  EXPECT_EQ(receivedShot.splitTimeMs, 0u);

  // Shot 1 at 2.10s → split 600 ms
  uint8_t s2[] = { 0xF8, 0xF9, 0x36, 0x00,  0x02, 0x0A, 0x01, 0x00,  0xF9, 0xF8 };
  device.processTimerData(s2, sizeof(s2));
  EXPECT_EQ(receivedShot.absoluteTimeMs, 2100u);
  EXPECT_EQ(receivedShot.splitTimeMs, 600u);

  // Shot 2 at 2.75s → split 650 ms
  uint8_t s3[] = { 0xF8, 0xF9, 0x36, 0x00,  0x02, 0x4B, 0x02, 0x00,  0xF9, 0xF8 };
  device.processTimerData(s3, sizeof(s3));
  EXPECT_EQ(receivedShot.absoluteTimeMs, 2750u);
  EXPECT_EQ(receivedShot.splitTimeMs, 650u);
}


// ═════════════════════════════════════════════════════════════════
//  Special Pie MAC Timer Protocol Tests
// ═════════════════════════════════════════════════════════════════

class SpecialPieMacProtocolTest : public ProtocolTestBase {
protected:
  SpecialPieMacTimerDevice device;

  void SetUp() override {
    ProtocolTestBase::SetUp();
    device.onShotDetected([this](const NormalizedShotData& d) {
      receivedShot = d;
      callbackCalled = true;
    });
    device.onSessionStarted([this](const SessionData& d) {
      receivedSession = d;
      sessionCallbackCalled = true;
    });
    device.onSessionStopped([this](const SessionData& d) {
      receivedSession = d;
      sessionCallbackCalled = true;
    });
  }
};

TEST_F(SpecialPieMacProtocolTest, ShotDetected_FirstShot) {
  uint8_t pkt[] = { 0xF8, 0xF9, 0x36, 0x00,  0x04, 0x19, 0x00, 0x00,  0xF9, 0xF8 };
  //                                           4s    25cs   shot#0
  device.processTimerData(pkt, sizeof(pkt));

  EXPECT_TRUE(callbackCalled);
  EXPECT_EQ(receivedShot.shotNumber, 1);
  EXPECT_EQ(receivedShot.absoluteTimeMs, 4250u);    // 4*1000 + 25*10
  EXPECT_TRUE(receivedShot.isFirstShot);
}

TEST_F(SpecialPieMacProtocolTest, ShotDetected_SplitCalculation) {
  // Shot 0 at 2.50s
  uint8_t s1[] = { 0xF8, 0xF9, 0x36, 0x00,  0x02, 0x32, 0x00, 0x00,  0xF9, 0xF8 };
  device.processTimerData(s1, sizeof(s1));
  EXPECT_EQ(receivedShot.absoluteTimeMs, 2500u);

  // Shot 1 at 3.20s → split = 700 ms
  uint8_t s2[] = { 0xF8, 0xF9, 0x36, 0x00,  0x03, 0x14, 0x01, 0x00,  0xF9, 0xF8 };
  device.processTimerData(s2, sizeof(s2));
  EXPECT_EQ(receivedShot.absoluteTimeMs, 3200u);
  EXPECT_EQ(receivedShot.splitTimeMs, 700u);
  EXPECT_FALSE(receivedShot.isFirstShot);
}

TEST_F(SpecialPieMacProtocolTest, SessionStart_SetsState) {
  uint8_t pkt[] = { 0xF8, 0xF9, 0x34, 0x07, 0x00, 0x00, 0xF9, 0xF8 };
  device.processTimerData(pkt, sizeof(pkt));

  EXPECT_TRUE(sessionCallbackCalled);
  EXPECT_EQ(receivedSession.sessionId, 7u);
  EXPECT_TRUE(receivedSession.isActive);
}

TEST_F(SpecialPieMacProtocolTest, SessionStop_ClearsState) {
  // Start session
  uint8_t start[] = { 0xF8, 0xF9, 0x34, 0x01, 0x00, 0x00, 0xF9, 0xF8 };
  device.processTimerData(start, sizeof(start));
  sessionCallbackCalled = false;

  // Stop session
  uint8_t stop[] = { 0xF8, 0xF9, 0x18, 0x01, 0x00, 0x00, 0xF9, 0xF8 };
  device.processTimerData(stop, sizeof(stop));

  EXPECT_TRUE(sessionCallbackCalled);
  EXPECT_FALSE(receivedSession.isActive);
}

TEST_F(SpecialPieMacProtocolTest, RejectsInvalidFrameMarkers) {
  uint8_t pkt[] = { 0x00, 0x00, 0x36, 0x00, 0x03, 0x2D, 0x00, 0x00, 0x00, 0x00 };
  device.processTimerData(pkt, sizeof(pkt));
  EXPECT_FALSE(callbackCalled);
}

TEST_F(SpecialPieMacProtocolTest, ShotResetsOnNewSession) {
  // Session with a shot
  uint8_t start[] = { 0xF8, 0xF9, 0x34, 0x01, 0x00, 0x00, 0xF9, 0xF8 };
  device.processTimerData(start, sizeof(start));

  uint8_t s1[] = { 0xF8, 0xF9, 0x36, 0x00,  0x02, 0x32, 0x00, 0x00,  0xF9, 0xF8 };
  device.processTimerData(s1, sizeof(s1));
  EXPECT_FALSE(receivedShot.isFirstShot == false);   // It IS the first shot

  // Stop and restart
  uint8_t stop[] = { 0xF8, 0xF9, 0x18, 0x01, 0x00, 0x00, 0xF9, 0xF8 };
  device.processTimerData(stop, sizeof(stop));

  uint8_t start2[] = { 0xF8, 0xF9, 0x34, 0x02, 0x00, 0x00, 0xF9, 0xF8 };
  device.processTimerData(start2, sizeof(start2));

  callbackCalled = false;

  // New first shot after restart
  uint8_t s2[] = { 0xF8, 0xF9, 0x36, 0x00,  0x01, 0x00, 0x00, 0x00,  0xF9, 0xF8 };
  device.processTimerData(s2, sizeof(s2));
  EXPECT_TRUE(callbackCalled);
  EXPECT_TRUE(receivedShot.isFirstShot);
  EXPECT_EQ(receivedShot.splitTimeMs, 0u);  // First shot — no split
}


// ═════════════════════════════════════════════════════════════════
//  Cross-device consistency tests
// ═════════════════════════════════════════════════════════════════

TEST(CrossDevice, AllDevicesProduceNonEmptyModel) {
  Logger::setLevel(LogLevel::NONE);

  SGTimerDevice sg;
  SpecialPieTimerDevice sp;
  ASNTrackerDevice asn;
  SpecialPieMacTimerDevice spMac;

  EXPECT_STRNE(sg.getDeviceModel(), "");
  EXPECT_STRNE(sp.getDeviceModel(), "");
  EXPECT_STRNE(asn.getDeviceModel(), "");
  EXPECT_STRNE(spMac.getDeviceModel(), "");
}

TEST(CrossDevice, InitializeReturnsTrue) {
  Logger::setLevel(LogLevel::NONE);

  SGTimerDevice sg;
  SpecialPieTimerDevice sp;
  ASNTrackerDevice asn;
  SpecialPieMacTimerDevice spMac;

  EXPECT_TRUE(sg.initialize());
  EXPECT_TRUE(sp.initialize());
  EXPECT_TRUE(asn.initialize());
  EXPECT_TRUE(spMac.initialize());
}

TEST(CrossDevice, DefaultCapabilities) {
  Logger::setLevel(LogLevel::NONE);

  SGTimerDevice sg;
  EXPECT_FALSE(sg.supportsRemoteStart());
  EXPECT_FALSE(sg.supportsShotList());
  EXPECT_FALSE(sg.supportsSessionControl());
}

// GoogleTest entry point
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
