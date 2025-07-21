#include "opflow/op/ohlc.hpp"
#include "gtest/gtest.h"
#include <cmath>

using namespace opflow;
using namespace opflow::op;

class OHLCTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}

  // Helper function to check if a double is NaN
  bool is_nan(double val) { return std::isnan(val); }

  // Helper function to check OHLC values
  void check_ohlc(const std::array<double, 4> &ohlc_values, double expected_open, double expected_high,
                  double expected_low, double expected_close) {
    EXPECT_DOUBLE_EQ(ohlc_values[0], expected_open) << "Open mismatch";
    EXPECT_DOUBLE_EQ(ohlc_values[1], expected_high) << "High mismatch";
    EXPECT_DOUBLE_EQ(ohlc_values[2], expected_low) << "Low mismatch";
    EXPECT_DOUBLE_EQ(ohlc_values[3], expected_close) << "Close mismatch";
  }

  // Helper function to check all values are NaN
  void check_all_nan(const std::array<double, 4> &ohlc_values) {
    EXPECT_TRUE(is_nan(ohlc_values[0])) << "Open should be NaN";
    EXPECT_TRUE(is_nan(ohlc_values[1])) << "High should be NaN";
    EXPECT_TRUE(is_nan(ohlc_values[2])) << "Low should be NaN";
    EXPECT_TRUE(is_nan(ohlc_values[3])) << "Close should be NaN";
  }
};

// Test basic construction
TEST_F(OHLCTest, BasicConstruction) {
  ohlc<int> op(10); // 10-unit window
  EXPECT_EQ(op.window_size, 10);
  EXPECT_EQ(op.next_tick, 0); // Default constructed int is 0

  std::array<double, 4> output{};
  op.value(output.data());
  check_all_nan(output);
}

TEST_F(OHLCTest, ConstructionWithPosition) {
  ohlc<int> op(10, 2); // 10-unit window, position 2
  EXPECT_EQ(op.window_size, 10);
  EXPECT_EQ(op.pos, 2);
}

// Test window alignment for different types
TEST_F(OHLCTest, WindowAlignmentInteger) {
  ohlc<int> op(10);

  // Test exact alignment
  EXPECT_EQ(op.align_to_window(0), 0);
  EXPECT_EQ(op.align_to_window(10), 10);
  EXPECT_EQ(op.align_to_window(20), 20);

  // Test non-aligned values
  EXPECT_EQ(op.align_to_window(5), 10);
  EXPECT_EQ(op.align_to_window(15), 20);
  EXPECT_EQ(op.align_to_window(1), 10);
  EXPECT_EQ(op.align_to_window(19), 20);
}

TEST_F(OHLCTest, WindowAlignmentFloat) {
  ohlc<double> op(10.0);

  // Test exact alignment
  EXPECT_DOUBLE_EQ(op.align_to_window(0.0), 0.0);
  EXPECT_DOUBLE_EQ(op.align_to_window(10.0), 10.0);
  EXPECT_DOUBLE_EQ(op.align_to_window(20.0), 20.0);

  // Test non-aligned values
  EXPECT_DOUBLE_EQ(op.align_to_window(5.5), 10.0);
  EXPECT_DOUBLE_EQ(op.align_to_window(15.7), 20.0);
  EXPECT_DOUBLE_EQ(op.align_to_window(0.1), 10.0);
  EXPECT_DOUBLE_EQ(op.align_to_window(19.9), 20.0);
}

// Test basic step operation - first data point
TEST_F(OHLCTest, FirstDataPoint) {
  ohlc<int> op(10);
  double input_val = 100.0;
  const double *input_ptr = &input_val;
  const double *const *input = &input_ptr;

  op.step(5, input);

  // Check internal state
  EXPECT_EQ(op.next_tick, 10); // Should be aligned to next window
  EXPECT_DOUBLE_EQ(op.open, 100.0);
  EXPECT_DOUBLE_EQ(op.high, 100.0);
  EXPECT_DOUBLE_EQ(op.low, 100.0);
  EXPECT_DOUBLE_EQ(op.close, 100.0);

  // Value should still return NaN as window is not complete
  std::array<double, 4> output{};
  op.value(output.data());
  check_all_nan(output);
}

// Test multiple data points within same window
TEST_F(OHLCTest, MultiplePointsSameWindow) {
  ohlc<int> op(10);
  std::array<double, 4> output{};

  // First point
  double input_val = 100.0;
  const double *input_ptr = &input_val;
  const double *const *input = &input_ptr;
  op.step(5, input);

  // Second point - higher
  input_val = 110.0;
  op.step(7, input);
  EXPECT_DOUBLE_EQ(op.high, 110.0);
  EXPECT_DOUBLE_EQ(op.close, 110.0);

  // Third point - lower
  input_val = 90.0;
  op.step(8, input);
  EXPECT_DOUBLE_EQ(op.low, 90.0);
  EXPECT_DOUBLE_EQ(op.close, 90.0);
  EXPECT_DOUBLE_EQ(op.high, 110.0); // Should remain unchanged
  EXPECT_DOUBLE_EQ(op.open, 100.0); // Should remain unchanged

  // Value should still return NaN as window is not complete
  op.value(output.data());
  check_all_nan(output);
}

// Test window completion - tick reaches next window
TEST_F(OHLCTest, WindowCompletion) {
  ohlc<int> op(10);
  std::array<double, 4> output{};

  // Add data points within window [0, 10)
  double input_val = 100.0;
  const double *input_ptr = &input_val;
  const double *const *input = &input_ptr;
  op.step(5, input);

  input_val = 110.0;
  op.step(7, input);

  input_val = 90.0;
  op.step(8, input);

  // Now complete the window
  input_val = 105.0;
  op.step(10, input); // This should trigger window completion

  // Get the output - should contain OHLC from previous window
  op.value(output.data());
  check_ohlc(output, 100.0, 110.0, 90.0, 90.0); // Last close was 90.0

  // After reading value, it should reset to NaN
  op.value(output.data());
  check_all_nan(output);

  // Check new window state
  EXPECT_EQ(op.next_tick, 20);
  EXPECT_DOUBLE_EQ(op.open, 105.0);
  EXPECT_DOUBLE_EQ(op.high, 105.0);
  EXPECT_DOUBLE_EQ(op.low, 105.0);
  EXPECT_DOUBLE_EQ(op.close, 105.0);
}

// Test exact boundary condition - tick exactly at boundary
TEST_F(OHLCTest, ExactBoundary) {
  ohlc<int> op(10);
  std::array<double, 4> output{};

  // Start with tick 0 (exact boundary)
  double input_val = 100.0;
  const double *input_ptr = &input_val;
  const double *const *input = &input_ptr;
  op.step(0, input);

  EXPECT_EQ(op.next_tick, 10);

  // Add more data in the same window
  input_val = 120.0;
  op.step(5, input);

  // Complete window at exact boundary
  input_val = 110.0;
  op.step(10, input);

  op.value(output.data());
  check_ohlc(output, 100.0, 120.0, 100.0, 120.0);

  // New window should start
  EXPECT_EQ(op.next_tick, 20);
  EXPECT_DOUBLE_EQ(op.open, 110.0);
}

// Test sparse data - large gaps between ticks
TEST_F(OHLCTest, SparseData) {
  ohlc<int> op(10);
  std::array<double, 4> output{};

  // First data point
  double input_val = 100.0;
  const double *input_ptr = &input_val;
  const double *const *input = &input_ptr;
  op.step(5, input);

  // Large gap - skip multiple windows
  input_val = 200.0;
  op.step(35, input); // Should trigger window completion for [0,10) and start new window at [30,40)

  op.value(output.data());
  check_ohlc(output, 100.0, 100.0, 100.0, 100.0);

  // Should now be in window [30, 40)
  EXPECT_EQ(op.next_tick, 40);
  EXPECT_DOUBLE_EQ(op.open, 200.0);
  EXPECT_DOUBLE_EQ(op.high, 200.0);
  EXPECT_DOUBLE_EQ(op.low, 200.0);
  EXPECT_DOUBLE_EQ(op.close, 200.0);
}

// Test very sparse data - extreme gaps
TEST_F(OHLCTest, VerySparseData) {
  ohlc<int> op(10);
  std::array<double, 4> output{};

  double input_val = 100.0;
  const double *input_ptr = &input_val;
  const double *const *input = &input_ptr;
  op.step(1, input);

  // Extremely large gap
  input_val = 300.0;
  op.step(1000, input);

  op.value(output.data());
  check_ohlc(output, 100.0, 100.0, 100.0, 100.0);

  // Should be in window [1000, 1010)
  EXPECT_EQ(op.next_tick, 1010);
  EXPECT_DOUBLE_EQ(op.open, 300.0);
}

// Test floating point precision with small window sizes
TEST_F(OHLCTest, FloatingPointPrecision) {
  ohlc<double> op(0.1);

  double input_val = 100.0;
  const double *input_ptr = &input_val;
  const double *const *input = &input_ptr;

  op.step(0.05, input);
  EXPECT_DOUBLE_EQ(op.next_tick, 0.1);

  input_val = 110.0;
  op.step(0.1, input);

  std::array<double, 4> output{};
  op.value(output.data());
  check_ohlc(output, 100.0, 100.0, 100.0, 100.0);

  EXPECT_DOUBLE_EQ(op.next_tick, 0.2);
}

// Test with different input positions
TEST_F(OHLCTest, DifferentInputPositions) {
  ohlc<int> op(10, 1); // Position 1

  // Create input array with multiple values
  std::array<double, 3> input_vals = {50.0, 100.0, 150.0};
  const double *input_ptr = input_vals.data();
  const double *const *input = &input_ptr;

  op.step(5, input);

  // Should use input_vals[1] = 100.0 due to pos=1
  EXPECT_DOUBLE_EQ(op.open, 100.0);
  EXPECT_DOUBLE_EQ(op.high, 100.0);
  EXPECT_DOUBLE_EQ(op.low, 100.0);
  EXPECT_DOUBLE_EQ(op.close, 100.0);
}

// Test continuous windows - multiple complete windows
TEST_F(OHLCTest, ContinuousWindows) {
  ohlc<int> op(5); // Smaller window for easier testing
  std::array<double, 4> output{};

  double input_val;
  const double *input_ptr = &input_val;
  const double *const *input = &input_ptr;

  // First window [0, 5)
  input_val = 100.0;
  op.step(1, input);
  input_val = 110.0;
  op.step(2, input);
  input_val = 90.0;
  op.step(3, input);
  input_val = 105.0;
  op.step(4, input);

  // Complete first window
  input_val = 120.0;
  op.step(5, input);
  op.value(output.data());
  check_ohlc(output, 100.0, 110.0, 90.0, 105.0);

  // Continue in second window [5, 10)
  input_val = 130.0;
  op.step(7, input);
  input_val = 115.0;
  op.step(8, input);

  // Complete second window
  input_val = 125.0;
  op.step(10, input);
  op.value(output.data());
  check_ohlc(output, 120.0, 130.0, 115.0, 115.0);

  EXPECT_EQ(op.next_tick, 15);
  EXPECT_DOUBLE_EQ(op.open, 125.0);
}

// Test single data point per window
TEST_F(OHLCTest, SingleDataPointPerWindow) {
  ohlc<int> op(10);
  std::array<double, 4> output{};

  double input_val;
  const double *input_ptr = &input_val;
  const double *const *input = &input_ptr;

  // Single point in first window
  input_val = 100.0;
  op.step(5, input);

  // Jump to next window with single point
  input_val = 200.0;
  op.step(15, input);

  op.value(output.data());
  check_ohlc(output, 100.0, 100.0, 100.0, 100.0);

  // Jump to another window
  input_val = 300.0;
  op.step(25, input);

  op.value(output.data());
  check_ohlc(output, 200.0, 200.0, 200.0, 200.0);
}

// Test with negative values
TEST_F(OHLCTest, NegativeValues) {
  ohlc<int> op(10);
  std::array<double, 4> output{};

  double input_val;
  const double *input_ptr = &input_val;
  const double *const *input = &input_ptr;

  input_val = -100.0;
  op.step(1, input);
  input_val = -50.0;
  op.step(2, input);
  input_val = -150.0;
  op.step(3, input);
  input_val = -75.0;
  op.step(4, input);

  input_val = 0.0;
  op.step(10, input);

  op.value(output.data());
  check_ohlc(output, -100.0, -50.0, -150.0, -75.0);
}

// Test with zero values
TEST_F(OHLCTest, ZeroValues) {
  ohlc<int> op(10);
  std::array<double, 4> output{};

  double input_val;
  const double *input_ptr = &input_val;
  const double *const *input = &input_ptr;

  input_val = 0.0;
  op.step(1, input);
  input_val = 0.0;
  op.step(2, input);
  input_val = 0.0;
  op.step(3, input);

  input_val = 1.0;
  op.step(10, input);

  op.value(output.data());
  check_ohlc(output, 0.0, 0.0, 0.0, 0.0);
}

// Test window size of 1 - edge case
TEST_F(OHLCTest, WindowSizeOne) {
  ohlc<int> op(1);
  std::array<double, 4> output{};

  double input_val;
  const double *input_ptr = &input_val;
  const double *const *input = &input_ptr;

  input_val = 100.0;
  op.step(0, input);

  // Next tick should immediately complete the window
  input_val = 200.0;
  op.step(1, input);

  op.value(output.data());
  check_ohlc(output, 100.0, 100.0, 100.0, 100.0);

  EXPECT_EQ(op.next_tick, 2);
  EXPECT_DOUBLE_EQ(op.open, 200.0);
}
