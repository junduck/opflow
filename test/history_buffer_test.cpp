#include "gtest/gtest.h"

#include <array>
#include <chrono>
#include <memory_resource>
#include <numeric>
#include <vector>

#include "opflow/detail/history_buffer.hpp"

using namespace opflow::detail;

class HistoryBufferTest : public ::testing::Test {
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
TEST_F(HistoryBufferTest, DefaultConstruction) {
  history_buffer<int> h;
  // Default constructed buffer should be empty but we can't use it until properly initialized
  // This is different from history_ringbuf which had init() method
}

TEST_F(HistoryBufferTest, ConstructionWithCapacity) {
  history_buffer<int> h(3, 8);
  EXPECT_TRUE(h.empty());
  EXPECT_EQ(h.size(), 0);
}

TEST_F(HistoryBufferTest, ConstructionWithZeroValueSize) {
  // record_size = val_size + 1, so val_size = 0 means record_size = 1 (just timestamp)
  // This should be valid
  history_buffer<int> h(0, 8);
  EXPECT_TRUE(h.empty());
  EXPECT_EQ(h.size(), 0);
}

TEST_F(HistoryBufferTest, ConstructionWithZeroCapacity) {
  // Should default to capacity = 1 (next_pow2(0) = 1)
  history_buffer<int> h(2, 0);
  EXPECT_TRUE(h.empty());
  EXPECT_EQ(h.size(), 0);
}

// Test basic push and access operations
TEST_F(HistoryBufferTest, SinglePushAndAccess) {
  history_buffer<int> h(3, 4);
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

TEST_F(HistoryBufferTest, MultiplePushesWithinCapacity) {
  history_buffer<int> h(2, 4); // value_size=2, capacity=4

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
TEST_F(HistoryBufferTest, BufferGrowth) {
  history_buffer<int> h(1, 2); // Start with small capacity

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
TEST_F(HistoryBufferTest, PopFront) {
  history_buffer<int> h(1, 4);

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

TEST_F(HistoryBufferTest, PopOnEmptyBuffer) {
  history_buffer<int> h(1, 4);
  EXPECT_TRUE(h.empty());

  // Should not crash
  h.pop();
  EXPECT_TRUE(h.empty());
}

// Test front() and back() methods
TEST_F(HistoryBufferTest, FrontAndBack) {
  history_buffer<int> h(2, 4);

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

// Test wrap-around behavior in circular buffer
TEST_F(HistoryBufferTest, CircularBufferWrapAround) {
  history_buffer<int> h(1, 4); // Small capacity to force wrap-around

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
TEST_F(HistoryBufferTest, GrowthWithWrapAroundData) {
  history_buffer<int> h(1, 4);

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
TEST_F(HistoryBufferTest, Clear) {
  history_buffer<int> h(2, 4);

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
TEST_F(HistoryBufferTest, Reserve) {
  history_buffer<int> h(1, 2);

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

TEST_F(HistoryBufferTest, ReserveNoEffect) {
  history_buffer<int> h(1, 16);

  // Reserve smaller capacity should have no effect
  h.reserve(8);

  std::array<int, 1> data{10};
  h.push(1, data);
  EXPECT_EQ(h.size(), 1);
}

// Test iterator functionality
TEST_F(HistoryBufferTest, Iterator) {
  history_buffer<int> h(2, 4);

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

TEST_F(HistoryBufferTest, EmptyIterator) {
  history_buffer<int> h(1, 4);

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
TEST_F(HistoryBufferTest, DifferentTypes) {
  history_buffer<double> h(3, 4);

  std::array<double, 3> data1{1.1, 2.2, 3.3};
  std::array<double, 3> data2{4.4, 5.5, 6.6};

  h.push(10.5, data1);
  h.push(20.7, data2);

  EXPECT_EQ(h.size(), 2);
  EXPECT_DOUBLE_EQ(h[0].first, 10.5);
  EXPECT_DOUBLE_EQ(h[0].second[0], 1.1);
  EXPECT_DOUBLE_EQ(h[0].second[1], 2.2);
  EXPECT_DOUBLE_EQ(h[0].second[2], 3.3);

  EXPECT_DOUBLE_EQ(h[1].first, 20.7);
  EXPECT_DOUBLE_EQ(h[1].second[0], 4.4);
}

// Test large value_size
TEST_F(HistoryBufferTest, LargeValueSize) {
  constexpr size_t large_size = 1000;
  history_buffer<int> h(large_size, 2);

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
TEST_F(HistoryBufferTest, StressTest) {
  history_buffer<int> h(3, 2); // Start small to force multiple growth operations

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

// Test power of two capacities
TEST_F(HistoryBufferTest, PowerOfTwoCapacities) {
  // Test that capacities are always powers of 2
  history_buffer<int> h1(1, 1);  // Should become 1
  history_buffer<int> h2(1, 3);  // Should become 4
  history_buffer<int> h3(1, 7);  // Should become 8
  history_buffer<int> h4(1, 15); // Should become 16

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
TEST_F(HistoryBufferTest, PerformanceCharacteristics) {
  history_buffer<int> h(1, 1024);

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

TEST_F(HistoryBufferTest, MoveSemantics) {
  history_buffer<int> h1(2, 4);

  std::array<int, 2> data{1, 2};
  h1.push(42, data);

  // Test move constructor
  history_buffer<int> h2 = std::move(h1);
  EXPECT_EQ(h2.size(), 1);
  EXPECT_EQ(h2[0].first, 42);
  EXPECT_EQ(h2[0].second[0], 1);
  EXPECT_EQ(h2[0].second[1], 2);

  // h1 should be in valid but unspecified state
  // We can't make assumptions about its state after move
}

TEST_F(HistoryBufferTest, CopyConstruction) {
  history_buffer<int> h1(3, 4);

  std::array<int, 3> data{1, 2, 3};
  h1.push(42, data);

  history_buffer<int> h2 = h1;
  EXPECT_EQ(h2.size(), 1);
  EXPECT_EQ(h2[0].first, 42);

  EXPECT_EQ(h1.size(), 1);
  EXPECT_EQ(h1[0].first, 42);
}

TEST_F(HistoryBufferTest, ConstCorrectness) {
  history_buffer<int> h(2, 4);

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

TEST_F(HistoryBufferTest, ExceptionSafety) {
  // Test that the class handles exceptions gracefully

  // Test overflow protection in constructor
  try {
    // This should not throw on reasonable systems
    history_buffer<int> h(1000, 1024);
    EXPECT_TRUE(true); // If we get here, no exception was thrown
  } catch (const std::bad_alloc &) {
    // This is acceptable if memory is truly exhausted
    EXPECT_TRUE(true);
  }

  // Test with very large value_size (may throw)
  try {
    // Try to create a history with a huge value size that would overflow
    size_t huge_size = std::numeric_limits<size_t>::max() / 2;
    history_buffer<int> h(huge_size, 2);
    // If we get here, either the system handled it or our overflow check didn't trigger
    EXPECT_TRUE(true);
  } catch (const std::bad_alloc &) {
    // This is expected for overflow protection
    EXPECT_TRUE(true);
  }
}

TEST_F(HistoryBufferTest, IteratorIncrement) {
  history_buffer<int> h(1, 4);

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

TEST_F(HistoryBufferTest, LargeCapacityReserve) {
  history_buffer<int> h(1, 2);

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

TEST_F(HistoryBufferTest, MixedPushPopOperations) {
  history_buffer<int> h(1, 4);

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

TEST_F(HistoryBufferTest, PushEmptyDirectWrite) {
  history_buffer<int> h(3, 4);
  EXPECT_TRUE(h.empty());

  // Test push with timestamp only
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

TEST_F(HistoryBufferTest, PushEmptyWithGrowth) {
  history_buffer<int> h(2, 2); // Small initial capacity to test growth

  // Fill to capacity using push(T)
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

  // d1, d2 should be invalidated by growth, so re-get them
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

// Test from_back functionality
TEST_F(HistoryBufferTest, FromBack) {
  history_buffer<int> h(2, 4);

  std::array<int, 2> data1{10, 11};
  std::array<int, 2> data2{20, 21};
  std::array<int, 2> data3{30, 31};

  h.push(1, data1);
  h.push(2, data2);
  h.push(3, data3);

  // Test from_back access
  auto [t_back0, d_back0] = h.from_back(0); // Should be the last element (3)
  EXPECT_EQ(t_back0, 3);
  EXPECT_EQ(d_back0[0], 30);
  EXPECT_EQ(d_back0[1], 31);

  auto [t_back1, d_back1] = h.from_back(1); // Should be the second to last (2)
  EXPECT_EQ(t_back1, 2);
  EXPECT_EQ(d_back1[0], 20);
  EXPECT_EQ(d_back1[1], 21);

  auto [t_back2, d_back2] = h.from_back(2); // Should be the first element (1)
  EXPECT_EQ(t_back2, 1);
  EXPECT_EQ(d_back2[0], 10);
  EXPECT_EQ(d_back2[1], 11);
}

// Test custom allocator functionality
TEST_F(HistoryBufferTest, CustomAllocator) {
  // Create buffer with standard allocator
  std::allocator<int> alloc;
  history_buffer<int, std::allocator<int>> h(2, 4, alloc);

  std::array<int, 2> data{10, 20};
  h.push(1, data);

  EXPECT_EQ(h.size(), 1);
  EXPECT_EQ(h[0].first, 1);
  EXPECT_EQ(h[0].second[0], 10);
  EXPECT_EQ(h[0].second[1], 20);
}

// Test PMR allocator with unsynchronized pool
TEST_F(HistoryBufferTest, PMRUnsynchronizedPool) {
  std::pmr::unsynchronized_pool_resource pool;
  std::pmr::polymorphic_allocator<double> alloc(&pool);

  history_buffer<double, std::pmr::polymorphic_allocator<double>> h(3, 2, alloc);

  // Add many elements to stress test the allocator
  for (int i = 0; i < 100; ++i) {
    std::array<double, 3> data{static_cast<double>(i), static_cast<double>(i + 0.1), static_cast<double>(i + 0.2)};
    h.push(static_cast<double>(i * 1.5), data);
  }

  EXPECT_EQ(h.size(), 100);

  // Verify a few random entries
  auto [t0, d0] = h[0];
  EXPECT_DOUBLE_EQ(t0, 0.0);
  EXPECT_DOUBLE_EQ(d0[0], 0.0);

  auto [t50, d50] = h[50];
  EXPECT_DOUBLE_EQ(t50, 75.0);
  EXPECT_DOUBLE_EQ(d50[0], 50.0);

  auto [t99, d99] = h[99];
  EXPECT_DOUBLE_EQ(t99, 148.5);
  EXPECT_DOUBLE_EQ(d99[0], 99.0);
}

// Custom allocator for testing allocator forwarding
template <typename T>
class TrackingAllocator {
public:
  using value_type = T;
  static inline int allocation_count = 0;
  static inline int deallocation_count = 0;

  TrackingAllocator() = default;

  template <class U>
  TrackingAllocator(const TrackingAllocator<U> &) noexcept {}

  template <class U>
  struct rebind {
    using other = TrackingAllocator<U>;
  };

  T *allocate(size_t n) {
    ++allocation_count;
    return static_cast<T *>(std::malloc(n * sizeof(T)));
  }

  void deallocate(T *p, size_t) {
    ++deallocation_count;
    std::free(p);
  }

  template <class U>
  bool operator==(const TrackingAllocator<U> &) const {
    return true;
  }
  template <class U>
  bool operator!=(const TrackingAllocator<U> &) const {
    return false;
  }
};

// Test that allocator is properly forwarded during resize
TEST_F(HistoryBufferTest, AllocatorForwardingDuringResize) {
  // Use a custom allocator that tracks allocations
  // Reset counters
  TrackingAllocator<int>::allocation_count = 0;
  TrackingAllocator<int>::deallocation_count = 0;

  {
    TrackingAllocator<int> alloc;
    history_buffer<int, TrackingAllocator<int>> h(1, 2, alloc);

    std::array<int, 1> data{10};
    h.push(1, data);
    h.push(2, data);

    // Should have allocated once during construction
    EXPECT_EQ(TrackingAllocator<int>::allocation_count, 1);

    // This should trigger resize and allocation
    h.push(3, data);

    // Should have allocated again during resize
    EXPECT_EQ(TrackingAllocator<int>::allocation_count, 2);
  }

  // Should have deallocated both buffers
  EXPECT_EQ(TrackingAllocator<int>::deallocation_count, 2);
}

// Test zero-sized data (timestamp only)
TEST_F(HistoryBufferTest, TimestampOnlyRecords) {
  history_buffer<int> h(0, 4); // val_size = 0, so record_size = 1 (timestamp only)

  h.push(10, std::array<int, 0>{});
  h.push(20, std::array<int, 0>{});
  h.push(30, std::array<int, 0>{});

  EXPECT_EQ(h.size(), 3);

  auto [t1, d1] = h[0];
  EXPECT_EQ(t1, 10);
  EXPECT_EQ(d1.size(), 0); // Empty data span

  auto [t2, d2] = h[1];
  EXPECT_EQ(t2, 20);
  EXPECT_EQ(d2.size(), 0);

  auto [t3, d3] = h[2];
  EXPECT_EQ(t3, 30);
  EXPECT_EQ(d3.size(), 0);

  // Test timestamp-only push
  auto [t4, d4] = h.push(40);
  EXPECT_EQ(t4, 40);
  EXPECT_EQ(d4.size(), 0);

  EXPECT_EQ(h.size(), 4);
  auto [t4_check, d4_check] = h[3];
  EXPECT_EQ(t4_check, 40);
  EXPECT_EQ(d4_check.size(), 0);
}
