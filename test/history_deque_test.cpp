#include "opflow/history_deque.hpp"
#include "gtest/gtest.h"
#include <array>
#include <numeric>
#include <vector>

using namespace opflow;

class HistoryDequeTest : public ::testing::Test {
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
TEST_F(HistoryDequeTest, DefaultConstruction) {
  history_deque<int, double> h(3);
  EXPECT_TRUE(h.empty());
  EXPECT_EQ(h.size(), 0);
}

TEST_F(HistoryDequeTest, ConstructionWithCustomCapacity) {
  history_deque<int, double> h(2, 8);
  EXPECT_TRUE(h.empty());
  EXPECT_EQ(h.size(), 0);
}

TEST_F(HistoryDequeTest, ConstructionWithZeroValueSize) {
  // This should be allowed but may not be very useful
  history_deque<int, double> h(0);
  EXPECT_TRUE(h.empty());
  EXPECT_EQ(h.size(), 0);
}

// Test basic push and access operations
TEST_F(HistoryDequeTest, SinglePushAndAccess) {
  history_deque<int, int> h(3);
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

TEST_F(HistoryDequeTest, MultiplePushesWithinCapacity) {
  history_deque<int, int> h(2);

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

// Test pop operations
TEST_F(HistoryDequeTest, PopFront) {
  history_deque<int, int> h(1);

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

TEST_F(HistoryDequeTest, PopOnEmptyBuffer) {
  history_deque<int, int> h(1);
  EXPECT_TRUE(h.empty());

  // Should not crash
  h.pop();
  EXPECT_TRUE(h.empty());
}

// Test front() and back() methods
TEST_F(HistoryDequeTest, FrontAndBack) {
  history_deque<int, int> h(2);

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

// Test clear operation
TEST_F(HistoryDequeTest, Clear) {
  history_deque<int, int> h(2);

  std::array<int, 2> data1{10, 11};
  std::array<int, 2> data2{20, 21};

  h.push(1, data1);
  h.push(2, data2);
  EXPECT_EQ(h.size(), 2);

  h.clear();
  EXPECT_TRUE(h.empty());
  EXPECT_EQ(h.size(), 0);

  // Should be able to use normally after clear
  h.push(3, data1);
  EXPECT_EQ(h.size(), 1);
  EXPECT_EQ(h[0].tick, 3);
}

// Test iterator functionality
TEST_F(HistoryDequeTest, Iterator) {
  history_deque<int, int> h(2);

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

TEST_F(HistoryDequeTest, EmptyIterator) {
  history_deque<int, int> h(1);

  // Empty history should have begin() == end()
  EXPECT_EQ(h.begin(), h.end());

  // Range-based for loop should not execute
  int count = 0;
  for ([[maybe_unused]] const auto &step : h) {
    ++count;
  }
  EXPECT_EQ(count, 0);
}

// Test iterator arithmetic
TEST_F(HistoryDequeTest, IteratorArithmetic) {
  history_deque<int, int> h(1);

  std::array<int, 1> data1{10};
  std::array<int, 1> data2{20};
  std::array<int, 1> data3{30};

  h.push(1, data1);
  h.push(2, data2);
  h.push(3, data3);

  auto it = h.begin();

  // Test operator+=
  it += 2;
  EXPECT_EQ((*it).tick, 3);

  // Test operator-=
  it -= 1;
  EXPECT_EQ((*it).tick, 2);

  // Test operator+
  auto it2 = it + 1;
  EXPECT_EQ((*it2).tick, 3);

  // Test operator-
  auto it3 = it2 - 1;
  EXPECT_EQ((*it3).tick, 2);

  // Test operator[]
  EXPECT_EQ(it[0].tick, 2);
  EXPECT_EQ(it[1].tick, 3);

  // Test iterator difference
  EXPECT_EQ(it2 - it, 1);
}

// Test reverse iterators
TEST_F(HistoryDequeTest, ReverseIterator) {
  history_deque<int, int> h(1);

  std::array<int, 1> data1{10};
  std::array<int, 1> data2{20};
  std::array<int, 1> data3{30};

  h.push(1, data1);
  h.push(2, data2);
  h.push(3, data3);

  // Test reverse iteration
  int expected_tick = 3;
  for (auto it = h.rbegin(); it != h.rend(); ++it) {
    EXPECT_EQ((*it).tick, expected_tick);
    --expected_tick;
  }
}

// Test const correctness
TEST_F(HistoryDequeTest, ConstCorrectness) {
  history_deque<int, int> h(2);

  std::array<int, 2> data1{10, 11};
  std::array<int, 2> data2{20, 21};

  h.push(1, data1);
  h.push(2, data2);

  const auto &const_h = h;

  // Test const accessors
  EXPECT_EQ(const_h.size(), 2);
  EXPECT_FALSE(const_h.empty());

  auto const_step = const_h[0];
  EXPECT_EQ(const_step.tick, 1);
  EXPECT_EQ(const_step.data[0], 10);

  auto const_front = const_h.front();
  EXPECT_EQ(const_front.tick, 1);

  auto const_back = const_h.back();
  EXPECT_EQ(const_back.tick, 2);

  // Test const iterators
  auto const_it = const_h.cbegin();
  EXPECT_EQ((*const_it).tick, 1);

  // Test const iterator conversion
  auto non_const_it = h.begin();
  history_deque<int, int>::const_iterator const_converted_it = non_const_it;
  EXPECT_EQ((*const_converted_it).tick, 1);
}

// Test with different types
TEST_F(HistoryDequeTest, DifferentTypes) {
  history_deque<std::string, double> h(3);

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
TEST_F(HistoryDequeTest, LargeValueSize) {
  constexpr size_t large_size = 1000;
  history_deque<int, int> h(large_size);

  auto data = make_test_data(large_size, 42);
  h.push(1, data);

  EXPECT_EQ(h.size(), 1);
  auto step = h[0];
  EXPECT_EQ(step.tick, 1);
  EXPECT_EQ(step.data.size(), large_size);

  for (size_t i = 0; i < large_size; ++i) {
    EXPECT_EQ(step.data[i], 42 + static_cast<int>(i));
  }
}

// Test push with empty data (in-place writing)
TEST_F(HistoryDequeTest, PushEmptyDirectWrite) {
  history_deque<int, int> h(3);

  // Push empty entry and write data directly
  auto step = h.push(100);
  EXPECT_EQ(step.tick, 100);
  EXPECT_EQ(step.data.size(), 3);

  // Write data directly to the span
  step.data[0] = 10;
  step.data[1] = 20;
  step.data[2] = 30;

  // Verify the data was written correctly
  auto retrieved_step = h[0];
  EXPECT_EQ(retrieved_step.tick, 100);
  EXPECT_EQ(retrieved_step.data[0], 10);
  EXPECT_EQ(retrieved_step.data[1], 20);
  EXPECT_EQ(retrieved_step.data[2], 30);
}

// Test stress scenario with many operations
TEST_F(HistoryDequeTest, StressTest) {
  history_deque<int, int> h(3);

  // Add many elements
  for (int i = 0; i < 100; ++i) {
    std::array<int, 3> data{i * 3, i * 3 + 1, i * 3 + 2};
    h.push(i, data);
  }
  EXPECT_EQ(h.size(), 100);

  // Remove some from front
  for (int i = 0; i < 30; ++i) {
    h.pop();
  }
  EXPECT_EQ(h.size(), 70);

  // Verify remaining data is correct
  for (size_t i = 0; i < h.size(); ++i) {
    auto step = h[i];
    int expected_tick = 30 + static_cast<int>(i);
    EXPECT_EQ(step.tick, expected_tick);
    EXPECT_EQ(step.data[0], expected_tick * 3);
    EXPECT_EQ(step.data[1], expected_tick * 3 + 1);
    EXPECT_EQ(step.data[2], expected_tick * 3 + 2);
  }

  // Add more
  for (int i = 100; i < 150; ++i) {
    std::array<int, 3> data{i * 3, i * 3 + 1, i * 3 + 2};
    h.push(i, data);
  }
  EXPECT_EQ(h.size(), 120);
}

// Test max_size
TEST_F(HistoryDequeTest, MaxSize) {
  history_deque<int, int> h(1);
  EXPECT_GT(h.max_size(), 0);
}

// Test mixed push/pop operations
TEST_F(HistoryDequeTest, MixedPushPopOperations) {
  history_deque<int, int> h(2);

  std::array<int, 2> data1{10, 11};
  std::array<int, 2> data2{20, 21};
  std::array<int, 2> data3{30, 31};
  std::array<int, 2> data4{40, 41};

  // Push some data
  h.push(1, data1);
  h.push(2, data2);
  EXPECT_EQ(h.size(), 2);

  // Pop one
  h.pop();
  EXPECT_EQ(h.size(), 1);
  EXPECT_EQ(h.front().tick, 2);

  // Push more
  h.push(3, data3);
  h.push(4, data4);
  EXPECT_EQ(h.size(), 3);

  // Pop all
  h.pop();
  h.pop();
  h.pop();
  EXPECT_TRUE(h.empty());

  // Push again
  h.push(5, data1);
  EXPECT_EQ(h.size(), 1);
  EXPECT_EQ(h.front().tick, 5);
}

// Test zero value size operations
TEST_F(HistoryDequeTest, ZeroValueSizeOperations) {
  history_deque<int, int> h(0);

  std::vector<int> empty_data;
  h.push(1, empty_data);

  EXPECT_EQ(h.size(), 1);
  auto step = h[0];
  EXPECT_EQ(step.tick, 1);
  EXPECT_EQ(step.data.size(), 0);

  // Test push without data
  auto step2 = h.push(2);
  EXPECT_EQ(step2.tick, 2);
  EXPECT_EQ(step2.data.size(), 0);
}
