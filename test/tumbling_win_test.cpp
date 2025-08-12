#include "opflow/win/tumbling.hpp"
#include "gtest/gtest.h"

#include <chrono>

using namespace opflow;
using namespace opflow::win;

namespace {
template <typename Time>
using Win = tumbling<Time, int>; // Data type is unused by window; choose int
}

TEST(TumblingWindow_IntTime, BasicEmissionAndAlignment) {
  Win<int> w(10); // window size 10

  // Initial points 1,2,3 accumulate in [0,10) no emit
  EXPECT_FALSE(w.process(1, nullptr));
  EXPECT_FALSE(w.process(2, nullptr));
  EXPECT_FALSE(w.process(3, nullptr));

  // At t=11, reaches next_tick=10 -> emit window [0,10), size=3, evict=3 (tumbling), 11 accumulates in [10, 20)
  ASSERT_TRUE(w.process(11, nullptr));
  auto s0 = w.emit();
  EXPECT_EQ(s0.timestamp, 10);
  EXPECT_EQ(s0.size, 3u);
  EXPECT_EQ(s0.evict, 3u);

  // 12,13 are in [10,20) -> accumulate
  EXPECT_FALSE(w.process(12, nullptr));
  EXPECT_FALSE(w.process(13, nullptr));

  // At 20, emit [10,20) with 3 points (11, 12, 13), 20 accumulates in [20, 30)
  ASSERT_TRUE(w.process(20, nullptr));
  auto s1 = w.emit();
  EXPECT_EQ(s1.timestamp, 20);
  EXPECT_EQ(s1.size, 3u);
  EXPECT_EQ(s1.evict, 3u);

  // 23 accumulates in [20,30)
  EXPECT_FALSE(w.process(23, nullptr));

  // Jump to 60, should emit [20,30) with 2 points (20, 23), and skip to next_tick=70 afterwards
  ASSERT_TRUE(w.process(60, nullptr));
  auto s2 = w.emit();
  EXPECT_EQ(s2.timestamp, 30);
  EXPECT_EQ(s2.size, 2u);
  EXPECT_EQ(s2.evict, 2u);

  // 62 in [60,70): accumulate
  EXPECT_FALSE(w.process(62, nullptr));

  // 70 triggers emit [60,70) with 2 points (60, 62)
  ASSERT_TRUE(w.process(70, nullptr));
  auto s3 = w.emit();
  EXPECT_EQ(s3.timestamp, 70);
  EXPECT_EQ(s3.size, 2u);
  EXPECT_EQ(s3.evict, 2u);

  EXPECT_EQ(w.next_tick, 80);
}

TEST(TumblingWindow_IntTime, MultipleWindowSkipNoDataInBetween) {
  Win<int> w(5);
  // First datum aligns next_tick to 5
  EXPECT_FALSE(w.process(0, nullptr)); // size=1

  // Jump forward over several windows: 0 -> 26 (windows at 5,10,15,20,25)
  ASSERT_TRUE(w.process(26, nullptr));
  auto s = w.emit();
  // Emit window ending at 5 with size 1
  EXPECT_EQ(s.timestamp, 5);
  EXPECT_EQ(s.size, 1u);
  EXPECT_EQ(s.evict, 1u);

  // Ensure next_tick advanced beyond 26 (to 30)
  // Now 27 should be in (25,30)
  EXPECT_FALSE(w.process(27, nullptr));
  ASSERT_TRUE(w.process(30, nullptr));
  auto s2 = w.emit();
  EXPECT_EQ(s2.timestamp, 30);
  EXPECT_EQ(s2.size, 2u);
  EXPECT_EQ(s2.evict, 2u);
}

TEST(TumblingWindow_IntTime, BoundaryAtExactTick) {
  Win<int> w(10);
  // First tick at 10: next_tick will be 20 (aligned_next_window_begin returns tick+window when aligned)
  // But process() considers tick < next_tick for accumulation, else emits.
  EXPECT_FALSE(w.process(10, nullptr)); // tick=10 < next_tick=20 -> accumulate
  EXPECT_FALSE(w.process(19, nullptr));
  ASSERT_TRUE(w.process(20, nullptr)); // emit [10,20) with size=2
  auto s = w.emit();
  EXPECT_EQ(s.timestamp, 20);
  EXPECT_EQ(s.size, 2u);
  EXPECT_EQ(s.evict, 2u);
}

TEST(TumblingWindow_IntTime, ExactBoundaryJumping) {
  Win<int> w(10);

  EXPECT_FALSE(w.process(10, nullptr));

  EXPECT_TRUE(w.process(40, nullptr)); // Jump to 40, should emit [10, 20) for data point 10
  auto s1 = w.emit();
  EXPECT_EQ(s1.timestamp, 20);
  EXPECT_EQ(s1.size, 1u);
  EXPECT_EQ(s1.evict, 1u);

  EXPECT_TRUE(w.process(60, nullptr)); // Jump to 60, should emit [40, 50) for data point 40
  auto s2 = w.emit();
  EXPECT_EQ(s2.timestamp, 50);
  EXPECT_EQ(s2.size, 1u);
  EXPECT_EQ(s2.evict, 1u);

  EXPECT_TRUE(w.process(70, nullptr)); // Jump to 70, should emit [60, 70) for data point 60
  auto s3 = w.emit();
  EXPECT_EQ(s3.timestamp, 70);
  EXPECT_EQ(s3.size, 1u);
  EXPECT_EQ(s3.evict, 1u);
}

TEST(TumblingWindow_DoubleTime, FloatingPointTimeFmodPath) {
  Win<double> w(0.5); // window 0.5
  EXPECT_FALSE(w.process(0.1, nullptr));
  EXPECT_FALSE(w.process(0.2, nullptr));
  EXPECT_FALSE(w.process(0.49, nullptr));
  ASSERT_TRUE(w.process(0.5, nullptr));
  auto s0 = w.emit();
  EXPECT_DOUBLE_EQ(s0.timestamp, 0.5);
  EXPECT_EQ(s0.size, 3u);
  EXPECT_EQ(s0.evict, 3u); // 0.1, 0.2, 0.49

  EXPECT_FALSE(w.process(0.51, nullptr));
  EXPECT_FALSE(w.process(0.99, nullptr));
  ASSERT_TRUE(w.process(1.0, nullptr));
  auto s1 = w.emit();
  EXPECT_DOUBLE_EQ(s1.timestamp, 1.0);
  EXPECT_EQ(s1.size, 3u);
  EXPECT_EQ(s1.evict, 3u); // 0.5, 0.51, 0.99
}

TEST(TumblingWindow_ChronoTime, UsingChronoMilliseconds) {
  using clock = std::chrono::system_clock;
  using time_point = clock::time_point;
  using dura = std::chrono::milliseconds;
  tumbling<time_point, int> w(dura{100});

  time_point t0{}; // epoch
  // next_tick will align to 100ms
  EXPECT_FALSE(w.process(t0 + dura{10}, nullptr));
  EXPECT_FALSE(w.process(t0 + dura{20}, nullptr));
  EXPECT_FALSE(w.process(t0 + dura{99}, nullptr));
  ASSERT_TRUE(w.process(t0 + dura{100}, nullptr));
  auto s0 = w.emit();
  EXPECT_EQ(s0.timestamp, t0 + dura{100});
  EXPECT_EQ(s0.size, 3u);
  EXPECT_EQ(s0.evict, 3u); // 10, 20, 99

  EXPECT_FALSE(w.process(t0 + dura{101}, nullptr));
  EXPECT_FALSE(w.process(t0 + dura{150}, nullptr));
  ASSERT_TRUE(w.process(t0 + dura{200}, nullptr));
  auto s1 = w.emit();
  EXPECT_EQ(s1.timestamp, t0 + dura{200});
  EXPECT_EQ(s1.size, 3u);
  EXPECT_EQ(s1.evict, 3u); // 100, 101, 150
}

TEST(TumblingWindow_Reset, ClearsState) {
  Win<int> w(10);
  EXPECT_FALSE(w.process(1, nullptr));
  EXPECT_FALSE(w.process(2, nullptr));
  w.reset();
  // After reset, first call realigns from scratch; previous pending size must be cleared
  EXPECT_FALSE(w.process(5, nullptr)); // size becomes 1
  ASSERT_TRUE(w.process(15, nullptr)); // emit [0,10) with size=1
  auto s = w.emit();
  EXPECT_EQ(s.timestamp, 10);
  EXPECT_EQ(s.size, 1u);
  EXPECT_EQ(s.evict, 1u);
}
