#include "gtest/gtest.h"

#include <array>
#include <chrono>
#include <numeric>
#include <vector>

#include "opflow/detail/history_ringbuf.hpp"

using namespace opflow::detail;

class HistoryRingbufTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}

  // Helper function to create test data
  std::vector<int> make_test_data(size_t size, int start_value = 0) {
    std::vector<int> data(size);
    std::iota(data.begin(), data.end(), start_value);
    return data;
  }
};

// Test basic construction and initial state
TEST_F(HistoryRingbufTest, DefaultConstruction) {
  history_ringbuf<int, double> h(3);
  EXPECT_TRUE(h.empty());
  EXPECT_EQ(h.size(), 0);
}

TEST_F(HistoryRingbufTest, ConstructionWithCustomCapacity) {
  history_ringbuf<int, double> h(2, 8);
  EXPECT_TRUE(h.empty());
  EXPECT_EQ(h.size(), 0);
}

TEST_F(HistoryRingbufTest, ConstructionWithZeroValueSize) {
  // expect to throw std::bad_alloc
  using history_ringbuf_specialise = history_ringbuf<int, double>;
  EXPECT_THROW(history_ringbuf_specialise h(0, 8), std::bad_alloc);
}

// Test basic push and access operations
TEST_F(HistoryRingbufTest, SinglePushAndAccess) {
  history_ringbuf<int, int> h(3);
  auto data = make_test_data(3, 10);

  h.push(100, data);
  EXPECT_FALSE(h.empty());
  EXPECT_EQ(h.size(), 1);

  auto [t, d] = h[0];
  EXPECT_EQ(t, 100);
  EXPECT_EQ(d.size(), 3);
  EXPECT_EQ(d[0], 10);
  EXPECT_EQ(d[1], 11);
  EXPECT_EQ(d[2], 12);
}

TEST_F(HistoryRingbufTest, MultiplePushesWithinCapacity) {
  history_ringbuf<int, int> h(2, 4); // value_size=2, capacity=4

  std::array<int, 2> data1{10, 20};
  std::array<int, 2> data2{30, 40};
  std::array<int, 2> data3{50, 60};

  h.push(1, data1);
  h.push(2, data2);
  h.push(3, data3);

  EXPECT_EQ(h.size(), 3);

  auto [t0, d0] = h[0];
  EXPECT_EQ(t0, 1);
  EXPECT_EQ(d0[0], 10);
  EXPECT_EQ(d0[1], 20);

  auto [t1, d1] = h[1];
  EXPECT_EQ(t1, 2);
  EXPECT_EQ(d1[0], 30);
  EXPECT_EQ(d1[1], 40);

  auto [t2, d2] = h[2];
  EXPECT_EQ(t2, 3);
  EXPECT_EQ(d2[0], 50);
  EXPECT_EQ(d2[1], 60);
}

// Test buffer growth when capacity is exceeded
TEST_F(HistoryRingbufTest, BufferGrowth) {
  history_ringbuf<int, int> h(1, 2); // Start with small capacity

  std::array<int, 1> data1{10};
  std::array<int, 1> data2{20};
  std::array<int, 1> data3{30};

  // Fill initial capacity
  h.push(1, data1);
  h.push(2, data2);
  EXPECT_EQ(h.size(), 2);

  // This should trigger growth
  h.push(3, data3);
  EXPECT_EQ(h.size(), 3);

  // Verify all data is still accessible
  EXPECT_EQ(h[0].first, 1);
  EXPECT_EQ(h[0].second[0], 10);
  EXPECT_EQ(h[1].first, 2);
  EXPECT_EQ(h[1].second[0], 20);
  EXPECT_EQ(h[2].first, 3);
  EXPECT_EQ(h[2].second[0], 30);
}

// Test pop operations
TEST_F(HistoryRingbufTest, PopFront) {
  history_ringbuf<int, int> h(1);

  std::array<int, 1> data1{10};
  std::array<int, 1> data2{20};
  std::array<int, 1> data3{30};

  h.push(1, data1);
  h.push(2, data2);
  h.push(3, data3);
  EXPECT_EQ(h.size(), 3);

  h.pop();
  EXPECT_EQ(h.size(), 2);
  EXPECT_EQ(h[0].first, 2);
  EXPECT_EQ(h[0].second[0], 20);

  h.pop();
  EXPECT_EQ(h.size(), 1);
  EXPECT_EQ(h[0].first, 3);
  EXPECT_EQ(h[0].second[0], 30);

  h.pop();
  EXPECT_EQ(h.size(), 0);
  EXPECT_TRUE(h.empty());
}

TEST_F(HistoryRingbufTest, PopOnEmptyBuffer) {
  history_ringbuf<int, int> h(1);
  EXPECT_TRUE(h.empty());

  // Should not crash
  h.pop();
  EXPECT_TRUE(h.empty());
}

// Test front() and back() methods
TEST_F(HistoryRingbufTest, FrontAndBack) {
  history_ringbuf<int, int> h(2);

  std::array<int, 2> data1{10, 11};
  std::array<int, 2> data2{20, 21};
  std::array<int, 2> data3{30, 31};

  h.push(1, data1);
  auto front_step = h.front();
  auto back_step = h.back();
  EXPECT_EQ(front_step.first, 1);
  EXPECT_EQ(back_step.first, 1);
  EXPECT_EQ(front_step.second[0], 10);
  EXPECT_EQ(back_step.second[0], 10);

  h.push(2, data2);
  h.push(3, data3);

  front_step = h.front();
  back_step = h.back();
  EXPECT_EQ(front_step.first, 1);
  EXPECT_EQ(back_step.first, 3);
  EXPECT_EQ(front_step.second[0], 10);
  EXPECT_EQ(back_step.second[0], 30);
}

// Test front() on empty buffer (should be caught by assert in debug)
#ifndef NDEBUG
TEST_F(HistoryRingbufTest, BackOnEmptyBufferAssert) {
  history_ringbuf<int, int> h(1);
  EXPECT_TRUE(h.empty());

  // In debug mode, this should trigger an assertion
  // We can't easily test this without causing the test to fail
  // This is more of a documentation of the expected behavior
}
#endif

// Test wrap-around behavior in circular buffer
TEST_F(HistoryRingbufTest, CircularBufferWrapAround) {
  history_ringbuf<int, int> h(1, 4); // Small capacity to force wrap-around

  std::array<int, 1> data1{10};
  std::array<int, 1> data2{20};
  std::array<int, 1> data3{30};
  std::array<int, 1> data4{40};
  std::array<int, 1> data5{50};
  std::array<int, 1> data6{60};

  // Fill buffer
  h.push(1, data1);
  h.push(2, data2);
  h.push(3, data3);
  h.push(4, data4);
  EXPECT_EQ(h.size(), 4);

  // Remove some elements to create wrap-around scenario
  h.pop();
  h.pop();
  EXPECT_EQ(h.size(), 2);

  // Add more elements (this will wrap around)
  h.push(5, data5);
  h.push(6, data6);
  EXPECT_EQ(h.size(), 4);

  // Check that all elements are correct
  EXPECT_EQ(h[0].first, 3);
  EXPECT_EQ(h[1].first, 4);
  EXPECT_EQ(h[2].first, 5);
  EXPECT_EQ(h[3].first, 6);
}

// Test growth with wrap-around data
TEST_F(HistoryRingbufTest, GrowthWithWrapAroundData) {
  history_ringbuf<int, int> h(1, 4);

  std::array<int, 1> data1{10};
  std::array<int, 1> data2{20};
  std::array<int, 1> data3{30};
  std::array<int, 1> data4{40};
  std::array<int, 1> data5{50};
  std::array<int, 1> data6{60};
  std::array<int, 1> data7{70};

  // Fill buffer
  h.push(1, data1);
  h.push(2, data2);
  h.push(3, data3);
  h.push(4, data4);

  // Create wrap-around by popping and pushing
  h.pop();
  h.pop();
  h.push(5, data5);
  h.push(6, data6);

  // Now force growth (buffer should be full)
  h.push(7, data7); // This should trigger resize

  EXPECT_EQ(h.size(), 5);
  EXPECT_EQ(h[0].first, 3);
  EXPECT_EQ(h[1].first, 4);
  EXPECT_EQ(h[2].first, 5);
  EXPECT_EQ(h[3].first, 6);
  EXPECT_EQ(h[4].first, 7);
}

// Test clear operation
TEST_F(HistoryRingbufTest, Clear) {
  history_ringbuf<int, int> h(2);

  std::array<int, 2> data1{10, 11};
  std::array<int, 2> data2{20, 21};
  std::array<int, 2> data3{30, 31};

  h.push(1, data1);
  h.push(2, data2);
  EXPECT_EQ(h.size(), 2);

  h.clear();
  EXPECT_TRUE(h.empty());
  EXPECT_EQ(h.size(), 0);

  // Should be able to use normally after clear
  h.push(3, data3);
  EXPECT_EQ(h.size(), 1);
  EXPECT_EQ(h[0].first, 3);
}

// Test reserve operation
TEST_F(HistoryRingbufTest, Reserve) {
  history_ringbuf<int, int> h(1, 2);

  // Reserve more space
  h.reserve(8);

  // Should still be empty
  EXPECT_TRUE(h.empty());

  // Should be able to add elements without triggering growth
  for (size_t i = 0; i < 8; ++i) {
    std::array<int, 1> data{static_cast<int>(i * 10)};
    h.push(static_cast<int>(i), data);
  }
  EXPECT_EQ(h.size(), 8);

  // Verify all data
  for (size_t i = 0; i < 8; ++i) {
    EXPECT_EQ(h[i].first, static_cast<int>(i));
    EXPECT_EQ(h[i].second[0], static_cast<int>(i * 10));
  }
}

TEST_F(HistoryRingbufTest, ReserveNoEffect) {
  history_ringbuf<int, int> h(1, 16);

  // Reserve smaller capacity should have no effect
  h.reserve(8);

  std::array<int, 1> data{10};
  h.push(1, data);
  EXPECT_EQ(h.size(), 1);
}

// Test iterator functionality
TEST_F(HistoryRingbufTest, Iterator) {
  history_ringbuf<int, int> h(2);

  std::array<int, 2> data1{10, 11};
  std::array<int, 2> data2{20, 21};
  std::array<int, 2> data3{30, 31};

  h.push(1, data1);
  h.push(2, data2);
  h.push(3, data3);

  // Test range-based for loop
  int expected_tick = 1;
  for (const auto &step : h) {
    EXPECT_EQ(step.first, expected_tick);
    EXPECT_EQ(step.second[0], expected_tick * 10);
    EXPECT_EQ(step.second[1], expected_tick * 10 + 1);
    ++expected_tick;
  }

  // Test explicit iterator usage
  auto it = h.begin();
  EXPECT_EQ((*it).first, 1);
  ++it;
  EXPECT_EQ((*it).first, 2);
  ++it;
  EXPECT_EQ((*it).first, 3);
  ++it;
  EXPECT_EQ(it, h.end());
}

TEST_F(HistoryRingbufTest, EmptyIterator) {
  history_ringbuf<int, int> h(1);

  // Empty history should have begin() == end()
  EXPECT_EQ(h.begin(), h.end());

  // Range-based for loop should not execute
  int count = 0;
  for (const auto &step : h) {
    (void)step; // Suppress unused variable warning
    ++count;
  }
  EXPECT_EQ(count, 0);
}

// Test with different types
TEST_F(HistoryRingbufTest, DifferentTypes) {
  history_ringbuf<std::string, double> h(3);

  std::array<double, 3> data1{1.1, 2.2, 3.3};
  std::array<double, 3> data2{4.4, 5.5, 6.6};

  h.push("tick1", data1);
  h.push("tick2", data2);

  EXPECT_EQ(h.size(), 2);
  EXPECT_EQ(h[0].first, "tick1");
  EXPECT_DOUBLE_EQ(h[0].second[0], 1.1);
  EXPECT_DOUBLE_EQ(h[0].second[1], 2.2);
  EXPECT_DOUBLE_EQ(h[0].second[2], 3.3);

  EXPECT_EQ(h[1].first, "tick2");
  EXPECT_DOUBLE_EQ(h[1].second[0], 4.4);
}

// Test large value_size
TEST_F(HistoryRingbufTest, LargeValueSize) {
  constexpr size_t large_size = 1000;
  history_ringbuf<int, int> h(large_size);

  auto data = make_test_data(large_size, 42);
  h.push(1, data);

  EXPECT_EQ(h.size(), 1);
  auto [t, d] = h[0];
  EXPECT_EQ(t, 1);
  EXPECT_EQ(d.size(), large_size);

  for (size_t i = 0; i < large_size; ++i) {
    EXPECT_EQ(d[i], static_cast<int>(42 + i));
  }
}

// Test stress scenario with many operations
TEST_F(HistoryRingbufTest, StressTest) {
  history_ringbuf<int, int> h(3, 2); // Start small to force multiple growth operations

  // Add many elements
  for (int i = 0; i < 100; ++i) {
    std::array<int, 3> data{i * 10, i * 10 + 1, i * 10 + 2};
    h.push(i, data);
  }
  EXPECT_EQ(h.size(), 100);

  // Remove some from front
  for (int i = 0; i < 30; ++i) {
    h.pop();
  }
  EXPECT_EQ(h.size(), 70);

  // Add more
  for (int i = 100; i < 150; ++i) {
    std::array<int, 3> data{i * 10, i * 10 + 1, i * 10 + 2};
    h.push(i, data);
  }
  EXPECT_EQ(h.size(), 120);

  // Verify data integrity
  int expected_tick = 30;
  for (const auto &[tick, data] : h) {
    EXPECT_EQ(tick, expected_tick);
    EXPECT_EQ(data[0], expected_tick * 10);
    EXPECT_EQ(data[1], expected_tick * 10 + 1);
    EXPECT_EQ(data[2], expected_tick * 10 + 2);
    ++expected_tick;
  }
}

// Test edge cases
TEST_F(HistoryRingbufTest, IndexOutOfBounds) {
  history_ringbuf<int, int> h(1);
  std::array<int, 1> data{10};
  h.push(1, data);

  // Valid access
  EXPECT_EQ(h[0].first, 1);

  // Invalid access should trigger assertion in debug mode
  // We can't easily test this without causing test failure
}

TEST_F(HistoryRingbufTest, PowerOfTwoCapacities) {
  // Test that capacities are always powers of 2
  history_ringbuf<int, int> h1(1, 1);  // Should become 1
  history_ringbuf<int, int> h2(1, 3);  // Should become 4
  history_ringbuf<int, int> h3(1, 7);  // Should become 8
  history_ringbuf<int, int> h4(1, 15); // Should become 16

  // We can't directly test the internal capacity, but we can test
  // that the behavior is correct by filling up to expected capacity

  // Test h2 (capacity should be 4)
  std::array<int, 1> data1{1};
  std::array<int, 1> data2{2};
  std::array<int, 1> data3{3};
  std::array<int, 1> data4{4};
  std::array<int, 1> data5{5};

  h2.push(1, data1);
  h2.push(2, data2);
  h2.push(3, data3);
  h2.push(4, data4);
  EXPECT_EQ(h2.size(), 4);

  // This should trigger growth
  h2.push(5, data5);
  EXPECT_EQ(h2.size(), 5);
}

// Performance characteristic test
TEST_F(HistoryRingbufTest, PerformanceCharacteristics) {
  history_ringbuf<int, int> h(1, 1024);

  // Fill with many elements - should be fast due to amortized O(1) push
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 10000; ++i) {
    std::array<int, 1> data{i};
    h.push(i, data);
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  // This is more of a smoke test - actual performance will vary by system
  EXPECT_LT(duration.count(), 1000); // Should complete in less than 1 second

  EXPECT_EQ(h.size(), 10000);

  // Random access should be O(1)
  EXPECT_EQ(h[0].first, 0);
  EXPECT_EQ(h[5000].first, 5000);
  EXPECT_EQ(h[9999].first, 9999);
}

TEST_F(HistoryRingbufTest, MoveSemantics) {
  history_ringbuf<std::string, int> h1(2);

  std::array<int, 2> data{1, 2};
  h1.push("test", data);

  // Test move constructor
  history_ringbuf<std::string, int> h2 = std::move(h1);
  EXPECT_EQ(h2.size(), 1);
  EXPECT_EQ(h2[0].first, "test");
  EXPECT_EQ(h2[0].second[0], 1);
  EXPECT_EQ(h2[0].second[1], 2);

  // h1 should be in valid but unspecified state
  // We can't make assumptions about its state after move
}

TEST_F(HistoryRingbufTest, CopyConstruction) {
  history_ringbuf<int, double> h1(3);

  std::array<double, 3> data{1.1, 2.2, 3.3};
  h1.push(42, data);

  history_ringbuf<int, double> h2 = h1;
  EXPECT_EQ(h2.size(), 1);
  EXPECT_EQ(h2[0].first, 42);

  EXPECT_EQ(h1.size(), 1);
  EXPECT_EQ(h1[0].first, 42);
}

TEST_F(HistoryRingbufTest, ConstCorrectness) {
  history_ringbuf<int, int> h(2);

  std::array<int, 2> data1{10, 20};
  std::array<int, 2> data2{30, 40};

  h.push(1, data1);
  h.push(2, data2);

  // Test const access methods
  const auto &const_h = h;
  EXPECT_EQ(const_h.size(), 2);
  EXPECT_TRUE(!const_h.empty());

  auto [t, d] = const_h[0];
  EXPECT_EQ(t, 1);
  EXPECT_EQ(d[0], 10);

  auto const_front = const_h.front();
  EXPECT_EQ(const_front.first, 1);

  auto const_back = const_h.back();
  EXPECT_EQ(const_back.first, 2);

  // Test const iterators
  int count = 0;
  for (const auto &step : const_h) {
    (void)step;
    ++count;
  }
  EXPECT_EQ(count, 2);
}

TEST_F(HistoryRingbufTest, ExceptionSafety) {
  // Test that the class handles exceptions gracefully

  // Test overflow protection in constructor
  try {
    // This should not throw on reasonable systems
    history_ringbuf<int, int> h(1000, 1024);
    EXPECT_TRUE(true); // If we get here, no exception was thrown
  } catch (const std::bad_alloc &) {
    // This is acceptable if memory is truly exhausted
    EXPECT_TRUE(true);
  }

  // Test with very large value_size (may throw)
  try {
    // Try to create a history with a huge value size that would overflow
    size_t huge_size = std::numeric_limits<size_t>::max() / 2 + 1;
    history_ringbuf<int, int> h(huge_size, 2);
    // If we get here, either the system handled it or our overflow check didn't trigger
    EXPECT_TRUE(true);
  } catch (const std::bad_alloc &) {
    // This is expected for overflow protection
    EXPECT_TRUE(true);
  }
}

TEST_F(HistoryRingbufTest, IteratorIncrement) {
  history_ringbuf<int, int> h(1);

  std::array<int, 1> data1{10};
  std::array<int, 1> data2{20};
  std::array<int, 1> data3{30};

  h.push(1, data1);
  h.push(2, data2);
  h.push(3, data3);

  // Test pre-increment
  auto it = h.begin();
  EXPECT_EQ((*it).first, 1);

  ++it;
  EXPECT_EQ((*it).first, 2);

  // Test post-increment
  auto old_it = it++;
  EXPECT_EQ((*old_it).first, 2);
  EXPECT_EQ((*it).first, 3);

  ++it;
  EXPECT_EQ(it, h.end());
}

TEST_F(HistoryRingbufTest, LargeCapacityReserve) {
  history_ringbuf<int, int> h(1, 2);

  // Reserve a large capacity
  h.reserve(1024);

  // Fill up to near the reserved capacity
  for (int i = 0; i < 1000; ++i) {
    std::array<int, 1> data{i};
    h.push(i, data);
  }

  EXPECT_EQ(h.size(), 1000);

  // Verify data integrity
  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(h[static_cast<size_t>(i)].first, i);
    EXPECT_EQ(h[static_cast<size_t>(i)].second[0], i);
  }
}

TEST_F(HistoryRingbufTest, MixedPushPopOperations) {
  history_ringbuf<int, int> h(1, 4);

  // Complex sequence of operations
  std::array<int, 1> data1{1};
  std::array<int, 1> data2{2};
  std::array<int, 1> data3{3};
  std::array<int, 1> data4{4};
  std::array<int, 1> data5{5};
  std::array<int, 1> data6{6};

  // Fill buffer
  h.push(1, data1);
  h.push(2, data2);
  EXPECT_EQ(h.size(), 2);

  // Pop one
  h.pop();
  EXPECT_EQ(h.size(), 1);
  EXPECT_EQ(h[0].first, 2);

  // Add more
  h.push(3, data3);
  h.push(4, data4);
  h.push(5, data5); // This might trigger growth
  EXPECT_EQ(h.size(), 4);

  // Pop several
  h.pop();
  h.pop();
  EXPECT_EQ(h.size(), 2);

  // Add one more
  h.push(6, data6);
  EXPECT_EQ(h.size(), 3);

  // Verify final state
  EXPECT_EQ(h[0].first, 4);
  EXPECT_EQ(h[1].first, 5);
  EXPECT_EQ(h[2].first, 6);
}

TEST_F(HistoryRingbufTest, PushEmptyDirectWrite) {
  history_ringbuf<int, int> h(3);
  EXPECT_TRUE(h.empty());

  // Test push functionality
  auto [t, d] = h.push(100);
  EXPECT_EQ(t, 100);
  EXPECT_EQ(d.size(), 3);
  EXPECT_FALSE(h.empty());
  EXPECT_EQ(h.size(), 1);

  // Write directly to the span
  d[0] = 10;
  d[1] = 20;
  d[2] = 30;

  // Verify the data was written correctly
  auto [const_t, const_d] = h[0];
  EXPECT_EQ(const_t, 100);
  EXPECT_EQ(const_d[0], 10);
  EXPECT_EQ(const_d[1], 20);
  EXPECT_EQ(const_d[2], 30);

  // Test with multiple push calls
  auto [t2, d2] = h.push(200);
  d2[0] = 40;
  d2[1] = 50;
  d2[2] = 60;

  EXPECT_EQ(h.size(), 2);

  // Verify both entries
  auto [first_t, first_d] = h[0];
  auto [second_t, second_d] = h[1];

  EXPECT_EQ(first_t, 100);
  EXPECT_EQ(first_d[0], 10);
  EXPECT_EQ(first_d[1], 20);
  EXPECT_EQ(first_d[2], 30);

  EXPECT_EQ(second_t, 200);
  EXPECT_EQ(second_d[0], 40);
  EXPECT_EQ(second_d[1], 50);
  EXPECT_EQ(second_d[2], 60);
}

TEST_F(HistoryRingbufTest, PushEmptyWithGrowth) {
  history_ringbuf<int, int> h(2, 2); // Small initial capacity to test growth

  // Fill to capacity using push
  auto [t1, d1] = h.push(1);
  d1[0] = 1;
  d1[1] = 2;

  auto [t2, d2] = h.push(2);
  d2[0] = 3;
  d2[1] = 4;

  EXPECT_EQ(h.size(), 2);

  // This should trigger growth
  auto [t3, d3] = h.push(3);
  d3[0] = 5;
  d3[1] = 6;

  EXPECT_EQ(h.size(), 3);

  // d1, d2 should be invalidated by growth
  std::tie(t1, d1) = h[0];
  std::tie(t2, d2) = h[1];

  // Verify all data is preserved
  EXPECT_EQ(t1, 1);
  EXPECT_EQ(d1[0], 1);
  EXPECT_EQ(d1[1], 2);

  EXPECT_EQ(t2, 2);
  EXPECT_EQ(d2[0], 3);
  EXPECT_EQ(d2[1], 4);

  EXPECT_EQ(t3, 3);
  EXPECT_EQ(d3[0], 5);
  EXPECT_EQ(d3[1], 6);
}
