#include "opflow/detail/history_deque.hpp"
#include "opflow/detail/history_ringbuf.hpp"
#include "gtest/gtest.h"
#include <array>
#include <numeric>
#include <vector>

using namespace opflow::detail;

class HistoryComparisonTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}

  // Helper function to create test data
  std::vector<int> make_test_data(size_t size, int start_value = 0) {
    std::vector<int> data(size);
    std::iota(data.begin(), data.end(), start_value);
    return data;
  }

  // Helper to compare two step views
  template <typename StepView1, typename StepView2>
  void compare_step_views(const StepView1 &s1, const StepView2 &s2) {
    EXPECT_EQ(s1.first, s2.first);
    EXPECT_EQ(s1.second.size(), s2.second.size());
    for (size_t i = 0; i < s1.second.size(); ++i) {
      EXPECT_EQ(s1.second[i], s2.second[i]);
    }
  }

  // Helper to compare two history containers
  template <typename Hist1, typename Hist2>
  void compare_histories(const Hist1 &h1, const Hist2 &h2) {
    EXPECT_EQ(h1.size(), h2.size());
    EXPECT_EQ(h1.empty(), h2.empty());

    if (!h1.empty() && !h2.empty()) {
      compare_step_views(h1.front(), h2.front());
      compare_step_views(h1.back(), h2.back());

      for (size_t i = 0; i < h1.size(); ++i) {
        compare_step_views(h1[i], h2[i]);
      }
    }
  }
};

// Test that both implementations behave identically for basic operations
TEST_F(HistoryComparisonTest, IdenticalBehaviorBasicOperations) {
  constexpr size_t value_size = 3;
  history_deque<int, int> deque_hist(value_size);
  history_ringbuf<int, int> ringbuf_hist(value_size);

  // Both should start empty
  compare_histories(deque_hist, ringbuf_hist);

  // Add some data
  for (int i = 0; i < 10; ++i) {
    auto data = make_test_data(value_size, i * 10);

    auto deque_step = deque_hist.push(i, data);
    auto ringbuf_step = ringbuf_hist.push(i, data);

    compare_step_views(deque_step, ringbuf_step);
    compare_histories(deque_hist, ringbuf_hist);
  }

  // Pop some elements
  for (int i = 0; i < 5; ++i) {
    deque_hist.pop();
    ringbuf_hist.pop();
    compare_histories(deque_hist, ringbuf_hist);
  }

  // Add more data
  for (int i = 10; i < 15; ++i) {
    auto data = make_test_data(value_size, i * 10);

    auto deque_step = deque_hist.push(i, data);
    auto ringbuf_step = ringbuf_hist.push(i, data);

    compare_step_views(deque_step, ringbuf_step);
    compare_histories(deque_hist, ringbuf_hist);
  }

  // Clear both
  deque_hist.clear();
  ringbuf_hist.clear();
  compare_histories(deque_hist, ringbuf_hist);
}

// Test iterator behavior is identical
TEST_F(HistoryComparisonTest, IdenticalIteratorBehavior) {
  constexpr size_t value_size = 2;
  history_deque<int, int> deque_hist(value_size);
  history_ringbuf<int, int> ringbuf_hist(value_size);

  // Add test data
  for (int i = 0; i < 5; ++i) {
    std::array<int, 2> data{i * 2, i * 2 + 1};
    deque_hist.push(i, data);
    ringbuf_hist.push(i, data);
  }

  // Test forward iteration
  auto deque_it = deque_hist.begin();
  auto ringbuf_it = ringbuf_hist.begin();

  while (deque_it != deque_hist.end() && ringbuf_it != ringbuf_hist.end()) {
    compare_step_views(*deque_it, *ringbuf_it);
    ++deque_it;
    ++ringbuf_it;
  }

  EXPECT_EQ(deque_it == deque_hist.end(), ringbuf_it == ringbuf_hist.end());

  // Test reverse iteration
  auto deque_rit = deque_hist.rbegin();
  auto ringbuf_rit = ringbuf_hist.rbegin();

  while (deque_rit != deque_hist.rend() && ringbuf_rit != ringbuf_hist.rend()) {
    compare_step_views(*deque_rit, *ringbuf_rit);
    ++deque_rit;
    ++ringbuf_rit;
  }

  EXPECT_EQ(deque_rit == deque_hist.rend(), ringbuf_rit == ringbuf_hist.rend());
}

// Test push with empty data behaves identically
TEST_F(HistoryComparisonTest, IdenticalPushEmptyBehavior) {
  constexpr size_t value_size = 4;
  history_deque<int, int> deque_hist(value_size);
  history_ringbuf<int, int> ringbuf_hist(value_size);

  // Push empty entries
  auto deque_step1 = deque_hist.push(100);
  auto ringbuf_step1 = ringbuf_hist.push(100);

  EXPECT_EQ(deque_step1.first, ringbuf_step1.first);
  EXPECT_EQ(deque_step1.second.size(), ringbuf_step1.second.size());

  // Write identical data
  for (size_t i = 0; i < value_size; ++i) {
    int value = static_cast<int>(i * 5);
    deque_step1.second[i] = value;
    ringbuf_step1.second[i] = value;
  }

  // Both should have identical content
  compare_histories(deque_hist, ringbuf_hist);

  // Add another empty entry
  auto deque_step2 = deque_hist.push(200);
  auto ringbuf_step2 = ringbuf_hist.push(200);

  // Write different patterns to verify independence
  for (size_t i = 0; i < value_size; ++i) {
    int value = static_cast<int>(i * 7 + 1);
    deque_step2.second[i] = value;
    ringbuf_step2.second[i] = value;
  }

  compare_histories(deque_hist, ringbuf_hist);
}

// Test that ringbuf matches deque behavior even after buffer growth
TEST_F(HistoryComparisonTest, RingbufMatchesDequeAfterGrowth) {
  constexpr size_t value_size = 2;
  history_deque<int, int> deque_hist(value_size);
  // Start ringbuf with small capacity to force growth
  history_ringbuf<int, int> ringbuf_hist(value_size, 4);

  // Add enough data to force ringbuf growth
  for (int i = 0; i < 20; ++i) {
    std::array<int, 2> data{i * 3, i * 3 + 1};
    deque_hist.push(i, data);
    ringbuf_hist.push(i, data);

    // Should be identical at every step
    compare_histories(deque_hist, ringbuf_hist);
  }

  // Pop some and add more to test growth with existing data
  for (int i = 0; i < 10; ++i) {
    deque_hist.pop();
    ringbuf_hist.pop();
    compare_histories(deque_hist, ringbuf_hist);
  }

  // Add more to potentially trigger another growth
  for (int i = 20; i < 35; ++i) {
    std::array<int, 2> data{i * 3, i * 3 + 1};
    deque_hist.push(i, data);
    ringbuf_hist.push(i, data);
    compare_histories(deque_hist, ringbuf_hist);
  }
}

// Test const correctness is identical
TEST_F(HistoryComparisonTest, IdenticalConstBehavior) {
  constexpr size_t value_size = 3;
  history_deque<int, int> deque_hist(value_size);
  history_ringbuf<int, int> ringbuf_hist(value_size);

  // Add some data
  for (int i = 0; i < 5; ++i) {
    auto data = make_test_data(value_size, i * 4);
    deque_hist.push(i, data);
    ringbuf_hist.push(i, data);
  }

  const auto &const_deque = deque_hist;
  const auto &const_ringbuf = ringbuf_hist;

  // Test const operations
  compare_histories(const_deque, const_ringbuf);

  // Test const iterators
  auto deque_cit = const_deque.cbegin();
  auto ringbuf_cit = const_ringbuf.cbegin();

  while (deque_cit != const_deque.cend() && ringbuf_cit != const_ringbuf.cend()) {
    compare_step_views(*deque_cit, *ringbuf_cit);
    ++deque_cit;
    ++ringbuf_cit;
  }
}

// Stress test to ensure identical behavior under heavy load
TEST_F(HistoryComparisonTest, StressTestIdenticalBehavior) {
  constexpr size_t value_size = 5;
  history_deque<int, int> deque_hist(value_size);
  history_ringbuf<int, int> ringbuf_hist(value_size, 8); // Small initial capacity

  // Perform many mixed operations
  for (int round = 0; round < 10; ++round) {
    // Add many elements
    for (int i = 0; i < 50; ++i) {
      auto data = make_test_data(value_size, round * 1000 + i * 10);
      deque_hist.push(round * 100 + i, data);
      ringbuf_hist.push(round * 100 + i, data);
    }

    // Verify they're still identical
    compare_histories(deque_hist, ringbuf_hist);

    // Remove some elements
    for (int i = 0; i < 20; ++i) {
      deque_hist.pop();
      ringbuf_hist.pop();
    }

    // Verify again
    compare_histories(deque_hist, ringbuf_hist);
  }

  // Final verification
  EXPECT_GT(deque_hist.size(), 0);
  compare_histories(deque_hist, ringbuf_hist);
}
