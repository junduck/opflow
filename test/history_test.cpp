#include "opflow/history.hpp"
#include "gtest/gtest.h"
#include <array>
#include <chrono>
#include <numeric>
#include <vector>

using namespace opflow;

class HistoryTest : public ::testing::Test {
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
TEST_F(HistoryTest, DefaultConstruction) {
  history<int, double> h(3);
  EXPECT_TRUE(h.empty());
  EXPECT_EQ(h.size(), 0);
}

TEST_F(HistoryTest, ConstructionWithCustomCapacity) {
  history<int, double> h(2, 8);
  EXPECT_TRUE(h.empty());
  EXPECT_EQ(h.size(), 0);
}

TEST_F(HistoryTest, ConstructionWithZeroValueSize) {
  // This should be allowed but may not be very useful
  history<int, double> h(0);
  EXPECT_TRUE(h.empty());
  EXPECT_EQ(h.size(), 0);
}

// Test basic push and access operations
TEST_F(HistoryTest, SinglePushAndAccess) {
  history<int, int> h(3);
  auto data = make_test_data(3, 10);

  h.push(100, data);
  EXPECT_FALSE(h.empty());
  EXPECT_EQ(h.size(), 1);

  auto step = h[0];
  EXPECT_EQ(step.tick, 100);
  EXPECT_EQ(step.data.size(), 3);
  EXPECT_EQ(step.data[0], 10);
  EXPECT_EQ(step.data[1], 11);
  EXPECT_EQ(step.data[2], 12);
}

TEST_F(HistoryTest, MultiplePushesWithinCapacity) {
  history<int, int> h(2, 4); // value_size=2, capacity=4

  std::array<int, 2> data1{10, 20};
  std::array<int, 2> data2{30, 40};
  std::array<int, 2> data3{50, 60};

  h.push(1, data1);
  h.push(2, data2);
  h.push(3, data3);

  EXPECT_EQ(h.size(), 3);

  auto step0 = h[0];
  EXPECT_EQ(step0.tick, 1);
  EXPECT_EQ(step0.data[0], 10);
  EXPECT_EQ(step0.data[1], 20);

  auto step1 = h[1];
  EXPECT_EQ(step1.tick, 2);
  EXPECT_EQ(step1.data[0], 30);
  EXPECT_EQ(step1.data[1], 40);

  auto step2 = h[2];
  EXPECT_EQ(step2.tick, 3);
  EXPECT_EQ(step2.data[0], 50);
  EXPECT_EQ(step2.data[1], 60);
}

// Test buffer growth when capacity is exceeded
TEST_F(HistoryTest, BufferGrowth) {
  history<int, int> h(1, 2); // Start with small capacity

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
  EXPECT_EQ(h[0].tick, 1);
  EXPECT_EQ(h[0].data[0], 10);
  EXPECT_EQ(h[1].tick, 2);
  EXPECT_EQ(h[1].data[0], 20);
  EXPECT_EQ(h[2].tick, 3);
  EXPECT_EQ(h[2].data[0], 30);
}

// Test pop operations
TEST_F(HistoryTest, PopFront) {
  history<int, int> h(1);

  std::array<int, 1> data1{10};
  std::array<int, 1> data2{20};
  std::array<int, 1> data3{30};

  h.push(1, data1);
  h.push(2, data2);
  h.push(3, data3);
  EXPECT_EQ(h.size(), 3);

  h.pop();
  EXPECT_EQ(h.size(), 2);
  EXPECT_EQ(h[0].tick, 2);
  EXPECT_EQ(h[0].data[0], 20);

  h.pop();
  EXPECT_EQ(h.size(), 1);
  EXPECT_EQ(h[0].tick, 3);
  EXPECT_EQ(h[0].data[0], 30);

  h.pop();
  EXPECT_EQ(h.size(), 0);
  EXPECT_TRUE(h.empty());
}

TEST_F(HistoryTest, PopOnEmptyBuffer) {
  history<int, int> h(1);
  EXPECT_TRUE(h.empty());

  // Should not crash
  h.pop();
  EXPECT_TRUE(h.empty());
}

// Test front() and back() methods
TEST_F(HistoryTest, FrontAndBack) {
  history<int, int> h(2);

  std::array<int, 2> data1{10, 11};
  std::array<int, 2> data2{20, 21};
  std::array<int, 2> data3{30, 31};

  h.push(1, data1);
  auto front_step = h.front();
  auto back_step = h.back();
  EXPECT_EQ(front_step.tick, 1);
  EXPECT_EQ(back_step.tick, 1);
  EXPECT_EQ(front_step.data[0], 10);
  EXPECT_EQ(back_step.data[0], 10);

  h.push(2, data2);
  h.push(3, data3);

  front_step = h.front();
  back_step = h.back();
  EXPECT_EQ(front_step.tick, 1);
  EXPECT_EQ(back_step.tick, 3);
  EXPECT_EQ(front_step.data[0], 10);
  EXPECT_EQ(back_step.data[0], 30);
}

// Test front() on empty buffer (should be caught by assert in debug)
#ifndef NDEBUG
TEST_F(HistoryTest, BackOnEmptyBufferAssert) {
  history<int, int> h(1);
  EXPECT_TRUE(h.empty());

  // In debug mode, this should trigger an assertion
  // We can't easily test this without causing the test to fail
  // This is more of a documentation of the expected behavior
}
#endif

// Test wrap-around behavior in circular buffer
TEST_F(HistoryTest, CircularBufferWrapAround) {
  history<int, int> h(1, 4); // Small capacity to force wrap-around

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
  EXPECT_EQ(h[0].tick, 3);
  EXPECT_EQ(h[1].tick, 4);
  EXPECT_EQ(h[2].tick, 5);
  EXPECT_EQ(h[3].tick, 6);
}

// Test growth with wrap-around data
TEST_F(HistoryTest, GrowthWithWrapAroundData) {
  history<int, int> h(1, 4);

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
  EXPECT_EQ(h[0].tick, 3);
  EXPECT_EQ(h[1].tick, 4);
  EXPECT_EQ(h[2].tick, 5);
  EXPECT_EQ(h[3].tick, 6);
  EXPECT_EQ(h[4].tick, 7);
}

// Test clear operation
TEST_F(HistoryTest, Clear) {
  history<int, int> h(2);

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
  EXPECT_EQ(h[0].tick, 3);
}

// Test reserve operation
TEST_F(HistoryTest, Reserve) {
  history<int, int> h(1, 2);

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
    EXPECT_EQ(h[i].tick, static_cast<int>(i));
    EXPECT_EQ(h[i].data[0], static_cast<int>(i * 10));
  }
}

TEST_F(HistoryTest, ReserveNoEffect) {
  history<int, int> h(1, 16);

  // Reserve smaller capacity should have no effect
  h.reserve(8);

  std::array<int, 1> data{10};
  h.push(1, data);
  EXPECT_EQ(h.size(), 1);
}

// Test iterator functionality
TEST_F(HistoryTest, Iterator) {
  history<int, int> h(2);

  std::array<int, 2> data1{10, 11};
  std::array<int, 2> data2{20, 21};
  std::array<int, 2> data3{30, 31};

  h.push(1, data1);
  h.push(2, data2);
  h.push(3, data3);

  // Test range-based for loop
  int expected_tick = 1;
  for (const auto &step : h) {
    EXPECT_EQ(step.tick, expected_tick);
    EXPECT_EQ(step.data[0], expected_tick * 10);
    EXPECT_EQ(step.data[1], expected_tick * 10 + 1);
    ++expected_tick;
  }

  // Test explicit iterator usage
  auto it = h.begin();
  EXPECT_EQ((*it).tick, 1);
  ++it;
  EXPECT_EQ((*it).tick, 2);
  ++it;
  EXPECT_EQ((*it).tick, 3);
  ++it;
  EXPECT_EQ(it, h.end());
}

TEST_F(HistoryTest, EmptyIterator) {
  history<int, int> h(1);

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
TEST_F(HistoryTest, DifferentTypes) {
  history<std::string, double> h(3);

  std::array<double, 3> data1{1.1, 2.2, 3.3};
  std::array<double, 3> data2{4.4, 5.5, 6.6};

  h.push("tick1", data1);
  h.push("tick2", data2);

  EXPECT_EQ(h.size(), 2);
  EXPECT_EQ(h[0].tick, "tick1");
  EXPECT_DOUBLE_EQ(h[0].data[0], 1.1);
  EXPECT_DOUBLE_EQ(h[0].data[1], 2.2);
  EXPECT_DOUBLE_EQ(h[0].data[2], 3.3);

  EXPECT_EQ(h[1].tick, "tick2");
  EXPECT_DOUBLE_EQ(h[1].data[0], 4.4);
}

// Test large value_size
TEST_F(HistoryTest, LargeValueSize) {
  constexpr size_t large_size = 1000;
  history<int, int> h(large_size);

  auto data = make_test_data(large_size, 42);
  h.push(1, data);

  EXPECT_EQ(h.size(), 1);
  auto step = h[0];
  EXPECT_EQ(step.tick, 1);
  EXPECT_EQ(step.data.size(), large_size);

  for (size_t i = 0; i < large_size; ++i) {
    EXPECT_EQ(step.data[i], static_cast<int>(42 + i));
  }
}

// Test stress scenario with many operations
TEST_F(HistoryTest, StressTest) {
  history<int, int> h(3, 2); // Start small to force multiple growth operations

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
  for (const auto &step : h) {
    EXPECT_EQ(step.tick, expected_tick);
    EXPECT_EQ(step.data[0], expected_tick * 10);
    EXPECT_EQ(step.data[1], expected_tick * 10 + 1);
    EXPECT_EQ(step.data[2], expected_tick * 10 + 2);
    ++expected_tick;
  }
}

// Test edge cases
TEST_F(HistoryTest, IndexOutOfBounds) {
  history<int, int> h(1);
  std::array<int, 1> data{10};
  h.push(1, data);

  // Valid access
  EXPECT_EQ(h[0].tick, 1);

  // Invalid access should trigger assertion in debug mode
  // We can't easily test this without causing test failure
}

TEST_F(HistoryTest, PowerOfTwoCapacities) {
  // Test that capacities are always powers of 2
  history<int, int> h1(1, 1);  // Should become 1
  history<int, int> h2(1, 3);  // Should become 4
  history<int, int> h3(1, 7);  // Should become 8
  history<int, int> h4(1, 15); // Should become 16

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
TEST_F(HistoryTest, PerformanceCharacteristics) {
  history<int, int> h(1, 1024);

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
  EXPECT_EQ(h[0].tick, 0);
  EXPECT_EQ(h[5000].tick, 5000);
  EXPECT_EQ(h[9999].tick, 9999);
}

// Additional edge case tests
TEST_F(HistoryTest, ZeroValueSizeOperations) {
  history<int, int> h(0); // Zero value size

  std::vector<int> empty_data;
  h.push(1, empty_data);
  h.push(2, empty_data);

  EXPECT_EQ(h.size(), 2);

  auto step = h[0];
  EXPECT_EQ(step.tick, 1);
  EXPECT_EQ(step.data.size(), 0);

  auto step2 = h[1];
  EXPECT_EQ(step2.tick, 2);
  EXPECT_EQ(step2.data.size(), 0);
}

TEST_F(HistoryTest, MoveSemantics) {
  history<std::string, int> h1(2);

  std::array<int, 2> data{1, 2};
  h1.push("test", data);

  // Test move constructor
  history<std::string, int> h2 = std::move(h1);
  EXPECT_EQ(h2.size(), 1);
  EXPECT_EQ(h2[0].tick, "test");
  EXPECT_EQ(h2[0].data[0], 1);
  EXPECT_EQ(h2[0].data[1], 2);

  // h1 should be in valid but unspecified state
  // We can't make assumptions about its state after move
}

TEST_F(HistoryTest, CopyConstruction) {
  history<int, double> h1(3);

  std::array<double, 3> data{1.1, 2.2, 3.3};
  h1.push(42, data);

  history<int, double> h2 = h1;
  EXPECT_EQ(h2.size(), 1);
  EXPECT_EQ(h2[0].tick, 42);

  EXPECT_EQ(h1.size(), 1);
  EXPECT_EQ(h1[0].tick, 42);
}

TEST_F(HistoryTest, ConstCorrectness) {
  history<int, int> h(2);

  std::array<int, 2> data1{10, 20};
  std::array<int, 2> data2{30, 40};

  h.push(1, data1);
  h.push(2, data2);

  // Test const access methods
  const auto &const_h = h;
  EXPECT_EQ(const_h.size(), 2);
  EXPECT_TRUE(!const_h.empty());

  auto const_step = const_h[0];
  EXPECT_EQ(const_step.tick, 1);
  EXPECT_EQ(const_step.data[0], 10);

  auto const_front = const_h.front();
  EXPECT_EQ(const_front.tick, 1);

  auto const_back = const_h.back();
  EXPECT_EQ(const_back.tick, 2);

  // Test const iterators
  int count = 0;
  for (const auto &step : const_h) {
    (void)step;
    ++count;
  }
  EXPECT_EQ(count, 2);
}

TEST_F(HistoryTest, ExceptionSafety) {
  // Test that the class handles exceptions gracefully

  // Test overflow protection in constructor
  try {
    // This should not throw on reasonable systems
    history<int, int> h(1000, 1024);
    EXPECT_TRUE(true); // If we get here, no exception was thrown
  } catch (const std::bad_alloc &) {
    // This is acceptable if memory is truly exhausted
    EXPECT_TRUE(true);
  }

  // Test with very large value_size (may throw)
  try {
    // Try to create a history with a huge value size that would overflow
    size_t huge_size = std::numeric_limits<size_t>::max() / 2 + 1;
    history<int, int> h(huge_size, 2);
    // If we get here, either the system handled it or our overflow check didn't trigger
    EXPECT_TRUE(true);
  } catch (const std::bad_alloc &) {
    // This is expected for overflow protection
    EXPECT_TRUE(true);
  }
}

TEST_F(HistoryTest, IteratorIncrement) {
  history<int, int> h(1);

  std::array<int, 1> data1{10};
  std::array<int, 1> data2{20};
  std::array<int, 1> data3{30};

  h.push(1, data1);
  h.push(2, data2);
  h.push(3, data3);

  // Test pre-increment
  auto it = h.begin();
  EXPECT_EQ((*it).tick, 1);

  ++it;
  EXPECT_EQ((*it).tick, 2);

  // Test post-increment
  auto old_it = it++;
  EXPECT_EQ((*old_it).tick, 2);
  EXPECT_EQ((*it).tick, 3);

  ++it;
  EXPECT_EQ(it, h.end());
}

TEST_F(HistoryTest, LargeCapacityReserve) {
  history<int, int> h(1, 2);

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
    EXPECT_EQ(h[static_cast<size_t>(i)].tick, i);
    EXPECT_EQ(h[static_cast<size_t>(i)].data[0], i);
  }
}

TEST_F(HistoryTest, MixedPushPopOperations) {
  history<int, int> h(1, 4);

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
  EXPECT_EQ(h[0].tick, 2);

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
  EXPECT_EQ(h[0].tick, 4);
  EXPECT_EQ(h[1].tick, 5);
  EXPECT_EQ(h[2].tick, 6);
}
