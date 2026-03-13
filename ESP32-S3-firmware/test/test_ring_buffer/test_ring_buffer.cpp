/**
 * @file test_ring_buffer.cpp
 * @brief Native tests for the shot event ring buffer logic.
 *
 * Tests the power-of-2 ring buffer used in TimerApplication for
 * lock-free BLE→MQTT shot event queuing. The formulas are taken
 * directly from TimerApplication.h (AppConfig namespace).
 *
 * We test the math standalone rather than including TimerApplication.h
 * (which pulls in DisplayManager, MqttManager, and many hardware
 * dependencies). This keeps the test lightweight and isolates the
 * ring buffer logic.
 *
 * Runner:  GoogleTest (native)
 * Command: pio test -e native-tests --filter test_ring_buffer
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>

// ═════════════════════════════════════════════════════════════════
//  Ring buffer logic — mirrors TimerApplication.h exactly
// ═════════════════════════════════════════════════════════════════

namespace RingBuffer {
  constexpr uint16_t SIZE = 32;         // Must be power of 2
  constexpr uint16_t MASK = SIZE - 1;   // 0x1F

  inline uint16_t queueSize(uint16_t head, uint16_t tail) {
    return (head - tail) & MASK;
  }

  inline bool queueEmpty(uint16_t head, uint16_t tail) {
    return head == tail;
  }

  inline bool queueFull(uint16_t head, uint16_t tail) {
    return queueSize(head, tail) == (SIZE - 1);
  }

  inline uint16_t advance(uint16_t index) {
    return (index + 1) & MASK;
  }
}

// Lightweight data struct for buffer testing
struct ShotEvent {
  uint16_t shotNumber;
  uint32_t timeMs;
};

// ═════════════════════════════════════════════════════════════════
//  Empty / Initial state tests
// ═════════════════════════════════════════════════════════════════

TEST(RingBuffer, InitiallyEmpty) {
  uint16_t head = 0, tail = 0;
  EXPECT_TRUE(RingBuffer::queueEmpty(head, tail));
  EXPECT_EQ(RingBuffer::queueSize(head, tail), 0);
  EXPECT_FALSE(RingBuffer::queueFull(head, tail));
}

// ═════════════════════════════════════════════════════════════════
//  Enqueue tests
// ═════════════════════════════════════════════════════════════════

TEST(RingBuffer, SingleEnqueue) {
  uint16_t head = 0, tail = 0;
  head = RingBuffer::advance(head);

  EXPECT_FALSE(RingBuffer::queueEmpty(head, tail));
  EXPECT_EQ(RingBuffer::queueSize(head, tail), 1);
}

TEST(RingBuffer, MultipleEnqueues) {
  uint16_t head = 0, tail = 0;
  for (int i = 0; i < 10; i++) {
    head = RingBuffer::advance(head);
  }

  EXPECT_EQ(RingBuffer::queueSize(head, tail), 10);
}

TEST(RingBuffer, FillToCapacity) {
  uint16_t head = 0, tail = 0;
  // Maximum capacity is SIZE - 1 (one slot is always unused)
  for (int i = 0; i < RingBuffer::SIZE - 1; i++) {
    EXPECT_FALSE(RingBuffer::queueFull(head, tail)) << "Should not be full at i=" << i;
    head = RingBuffer::advance(head);
  }

  EXPECT_TRUE(RingBuffer::queueFull(head, tail));
  EXPECT_EQ(RingBuffer::queueSize(head, tail), RingBuffer::SIZE - 1);
}

// ═════════════════════════════════════════════════════════════════
//  Dequeue tests
// ═════════════════════════════════════════════════════════════════

TEST(RingBuffer, SingleDequeue) {
  uint16_t head = 0, tail = 0;
  head = RingBuffer::advance(head);

  tail = RingBuffer::advance(tail);

  EXPECT_TRUE(RingBuffer::queueEmpty(head, tail));
  EXPECT_EQ(RingBuffer::queueSize(head, tail), 0);
}

TEST(RingBuffer, PartialDequeue) {
  uint16_t head = 0, tail = 0;

  // Enqueue 5 items
  for (int i = 0; i < 5; i++) {
    head = RingBuffer::advance(head);
  }

  // Dequeue 3 items
  for (int i = 0; i < 3; i++) {
    tail = RingBuffer::advance(tail);
  }

  EXPECT_EQ(RingBuffer::queueSize(head, tail), 2);
  EXPECT_FALSE(RingBuffer::queueEmpty(head, tail));
}

// ═════════════════════════════════════════════════════════════════
//  Wraparound tests
// ═════════════════════════════════════════════════════════════════

TEST(RingBuffer, HeadWrapsAround) {
  uint16_t head = RingBuffer::SIZE - 1;  // One before wrap
  uint16_t tail = RingBuffer::SIZE - 1;

  head = RingBuffer::advance(head);

  EXPECT_EQ(head, 0);  // Wrapped to 0
  EXPECT_EQ(RingBuffer::queueSize(head, tail), 1);
}

TEST(RingBuffer, SizeCorrectAcrossWrap) {
  // Head has wrapped past 0, tail is near end of buffer
  uint16_t tail = 28;
  uint16_t head = 3;  // Wrapped: 3 items past 0

  // Items: 28→29→30→31→0→1→2 = 7 items
  // Formula: (3 - 28) & 0x1F = (-25) & 31 = (65536-25) & 31 = 7
  EXPECT_EQ(RingBuffer::queueSize(head, tail), 7);
}

TEST(RingBuffer, FullAcrossWrap) {
  // Head = tail + SIZE - 1 (modulo SIZE) → full
  uint16_t tail = 20;
  uint16_t head = (tail + RingBuffer::SIZE - 1) & RingBuffer::MASK;  // 19

  EXPECT_TRUE(RingBuffer::queueFull(head, tail));
}

TEST(RingBuffer, ProducerConsumerSimulation) {
  // Simulate alternating produce/consume with wraparound
  uint16_t head = 0, tail = 0;
  ShotEvent buffer[RingBuffer::SIZE];

  int produced = 0, consumed = 0;

  // Produce 50 items, consume after every 3
  for (int i = 0; i < 50; i++) {
    if (!RingBuffer::queueFull(head, tail)) {
      buffer[head].shotNumber = i + 1;
      buffer[head].timeMs = i * 100;
      head = RingBuffer::advance(head);
      produced++;
    }

    if ((i % 3 == 0) && !RingBuffer::queueEmpty(head, tail)) {
      EXPECT_EQ(buffer[tail].shotNumber, consumed + 1);
      tail = RingBuffer::advance(tail);
      consumed++;
    }
  }

  // Drain remaining
  while (!RingBuffer::queueEmpty(head, tail)) {
    tail = RingBuffer::advance(tail);
    consumed++;
  }

  EXPECT_EQ(produced, consumed);
  EXPECT_TRUE(RingBuffer::queueEmpty(head, tail));
}

// ═════════════════════════════════════════════════════════════════
//  FIFO ordering tests
// ═════════════════════════════════════════════════════════════════

TEST(RingBuffer, FIFOOrder) {
  uint16_t head = 0, tail = 0;
  ShotEvent buffer[RingBuffer::SIZE];

  // Enqueue shots 1..10
  for (uint16_t i = 1; i <= 10; i++) {
    buffer[head].shotNumber = i;
    buffer[head].timeMs = i * 100;
    head = RingBuffer::advance(head);
  }

  // Dequeue and verify FIFO order
  for (uint16_t expected = 1; expected <= 10; expected++) {
    ASSERT_FALSE(RingBuffer::queueEmpty(head, tail));
    EXPECT_EQ(buffer[tail].shotNumber, expected);
    EXPECT_EQ(buffer[tail].timeMs, expected * 100u);
    tail = RingBuffer::advance(tail);
  }

  EXPECT_TRUE(RingBuffer::queueEmpty(head, tail));
}

// ═════════════════════════════════════════════════════════════════
//  Mask / power-of-2 invariant tests
// ═════════════════════════════════════════════════════════════════

TEST(RingBuffer, MaskIsSizeMinusOne) {
  EXPECT_EQ(RingBuffer::MASK, RingBuffer::SIZE - 1);
}

TEST(RingBuffer, SizeIsPowerOfTwo) {
  EXPECT_TRUE((RingBuffer::SIZE & (RingBuffer::SIZE - 1)) == 0);
  EXPECT_GT(RingBuffer::SIZE, 0);
}

TEST(RingBuffer, AdvanceNeverExceedsSize) {
  for (uint16_t i = 0; i < RingBuffer::SIZE * 2; i++) {
    uint16_t idx = i & RingBuffer::MASK;
    uint16_t next = RingBuffer::advance(idx);
    EXPECT_LT(next, RingBuffer::SIZE) << "advance(" << idx << ") returned " << next;
  }
}

// ═════════════════════════════════════════════════════════════════
//  Edge case: clear queue by setting tail = head
// ═════════════════════════════════════════════════════════════════

TEST(RingBuffer, ClearByMovingTailToHead) {
  uint16_t head = 0, tail = 0;

  // Fill some items
  for (int i = 0; i < 15; i++) {
    head = RingBuffer::advance(head);
  }
  EXPECT_EQ(RingBuffer::queueSize(head, tail), 15);

  // Clear queue (as done in TimerApplication::onSessionStopped)
  tail = head;

  EXPECT_TRUE(RingBuffer::queueEmpty(head, tail));
  EXPECT_EQ(RingBuffer::queueSize(head, tail), 0);
}

// GoogleTest entry point
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
