/**
 * @file test_time_formatting.cpp
 * @brief Native tests for time formatting functions.
 *
 * Tests the formatTime() and formatSplitTime() logic used by
 * DisplayManager. Functions are replicated here (see note below)
 * because DisplayManager.cpp has heavy hardware dependencies
 * (HUB75, U8g2) that cannot be stubbed economically.
 *
 * NOTE: If the formatting logic in DisplayManager.cpp changes,
 * update these replicated functions to match. The production
 * functions are: DisplayManager::formatTime() and
 * DisplayManager::formatSplitTime() in DisplayManager.cpp.
 *
 * Runner:  GoogleTest (native)
 * Command: pio test -e native-tests --filter test_time_formatting
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

// ═════════════════════════════════════════════════════════════════
//  Replicated from DisplayManager.cpp — keep in sync!
// ═════════════════════════════════════════════════════════════════

static void formatTime(uint32_t timeMs, char* buffer, size_t bufferSize) {
  const uint32_t totalSeconds = timeMs / 1000;
  const uint32_t centiseconds = (timeMs % 1000) / 10;
  snprintf(buffer, bufferSize, "%02lu:%02lu",
           (unsigned long)totalSeconds, (unsigned long)centiseconds);
}

static void formatSplitTime(uint32_t timeMs, char* buffer, size_t bufferSize) {
  const uint32_t totalSeconds = timeMs / 1000;
  const uint32_t centiseconds = (timeMs % 1000) / 10;
  if (totalSeconds < 10) {
    snprintf(buffer, bufferSize, "%lu:%02lu",
             (unsigned long)totalSeconds, (unsigned long)centiseconds);
  } else {
    snprintf(buffer, bufferSize, "%02lu:%02lu",
             (unsigned long)totalSeconds, (unsigned long)centiseconds);
  }
}

// ═════════════════════════════════════════════════════════════════
//  formatTime tests
// ═════════════════════════════════════════════════════════════════

TEST(FormatTime, ZeroMilliseconds) {
  char buf[16];
  formatTime(0, buf, sizeof(buf));
  EXPECT_STREQ(buf, "00:00");
}

TEST(FormatTime, ExactOneSecond) {
  char buf[16];
  formatTime(1000, buf, sizeof(buf));
  EXPECT_STREQ(buf, "01:00");
}

TEST(FormatTime, MillisecondsConvertToCentiseconds) {
  char buf[16];
  // 1500 ms = 1s, 50cs → "01:50"
  formatTime(1500, buf, sizeof(buf));
  EXPECT_STREQ(buf, "01:50");
}

TEST(FormatTime, SubTenMillisecondsRoundDown) {
  char buf[16];
  // 1009 ms → 1s, 0cs (9ms is < 10ms = 0 centiseconds)
  formatTime(1009, buf, sizeof(buf));
  EXPECT_STREQ(buf, "01:00");
}

TEST(FormatTime, NinetyNineCentiseconds) {
  char buf[16];
  // 990 ms → 0s, 99cs → "00:99"
  formatTime(990, buf, sizeof(buf));
  EXPECT_STREQ(buf, "00:99");
}

TEST(FormatTime, TenSeconds) {
  char buf[16];
  formatTime(10000, buf, sizeof(buf));
  EXPECT_STREQ(buf, "10:00");
}

TEST(FormatTime, LargeTime) {
  char buf[16];
  // 65,340 ms → 65s, 34cs → "65:34"
  formatTime(65340, buf, sizeof(buf));
  EXPECT_STREQ(buf, "65:34");
}

TEST(FormatTime, TypicalShot_3_45) {
  char buf[16];
  // 3450 ms → 3s, 45cs → "03:45"
  formatTime(3450, buf, sizeof(buf));
  EXPECT_STREQ(buf, "03:45");
}

TEST(FormatTime, EdgeCase_999ms) {
  char buf[16];
  // 999 ms → 0s, 99cs → "00:99"
  formatTime(999, buf, sizeof(buf));
  EXPECT_STREQ(buf, "00:99");
}

TEST(FormatTime, EdgeCase_10ms) {
  char buf[16];
  // 10 ms → 0s, 1cs → "00:01"
  formatTime(10, buf, sizeof(buf));
  EXPECT_STREQ(buf, "00:01");
}

TEST(FormatTime, EdgeCase_5ms) {
  char buf[16];
  // 5 ms → 0s, 0cs (truncated) → "00:00"
  formatTime(5, buf, sizeof(buf));
  EXPECT_STREQ(buf, "00:00");
}

TEST(FormatTime, SmallBufferTruncates) {
  char buf[4];  // Too small for "SS:CC\0"
  formatTime(1500, buf, sizeof(buf));
  // snprintf should null-terminate within bufferSize
  EXPECT_EQ(buf[3], '\0');
}

// ═════════════════════════════════════════════════════════════════
//  formatSplitTime tests
// ═════════════════════════════════════════════════════════════════

TEST(FormatSplitTime, ZeroMs) {
  char buf[16];
  formatSplitTime(0, buf, sizeof(buf));
  EXPECT_STREQ(buf, "0:00");  // Single-digit format when < 10s
}

TEST(FormatSplitTime, SubTenSeconds_SingleDigit) {
  char buf[16];
  // 5340 ms → 5s, 34cs → "5:34" (single-digit seconds)
  formatSplitTime(5340, buf, sizeof(buf));
  EXPECT_STREQ(buf, "5:34");
}

TEST(FormatSplitTime, ExactlyTenSeconds_DoubleDigit) {
  char buf[16];
  formatSplitTime(10000, buf, sizeof(buf));
  EXPECT_STREQ(buf, "10:00");  // Double-digit format at >= 10s
}

TEST(FormatSplitTime, NinePointNineNine) {
  char buf[16];
  // 9990 ms → 9s, 99cs → "9:99"
  formatSplitTime(9990, buf, sizeof(buf));
  EXPECT_STREQ(buf, "9:99");
}

TEST(FormatSplitTime, TypicalSplit_0_35) {
  char buf[16];
  // 350 ms → 0s, 35cs → "0:35"
  formatSplitTime(350, buf, sizeof(buf));
  EXPECT_STREQ(buf, "0:35");
}

TEST(FormatSplitTime, TypicalSplit_1_20) {
  char buf[16];
  // 1200 ms → 1s, 20cs → "1:20"
  formatSplitTime(1200, buf, sizeof(buf));
  EXPECT_STREQ(buf, "1:20");
}

TEST(FormatSplitTime, LargeSplit) {
  char buf[16];
  // 25,780 ms → 25s, 78cs → "25:78"
  formatSplitTime(25780, buf, sizeof(buf));
  EXPECT_STREQ(buf, "25:78");
}

// GoogleTest entry point
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
