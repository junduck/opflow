#include "opflow/op/ewma.hpp"
#include "gtest/gtest.h"
#include <random>
#include <span>
#include <vector>

using namespace opflow;
using namespace opflow::op;

class EWMATest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}

  // Helper function to generate random data
  std::vector<double> generate_random_data(size_t n, double min_val, double max_val, unsigned int seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> dist(min_val, max_val);
    std::vector<double> result;
    result.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      result.push_back(dist(gen));
    }
    return result;
  }

  // Helper function to simulate a rolling window with EWMA
  double simulate_rolling_window(const std::vector<double> &data, size_t window_size, double alpha) {
    if (data.size() < window_size) {
      return std::numeric_limits<double>::quiet_NaN();
    }

    ewma<int, double> op(alpha, 0);

    // Prepare input pointer array for the operator
    const double *input_ptr = data.data();
    const double *const *input = &input_ptr;

    // Initialize with first window_size values
    op.init(input);
    for (size_t i = 1; i < window_size; ++i) {
      input_ptr = &data[i];
      op.step(input);
    }

    // Roll through remaining values
    for (size_t i = window_size; i < data.size(); ++i) {
      // Remove the oldest value and add the new one
      input_ptr = &data[i - window_size];
      op.inverse(input);

      input_ptr = &data[i];
      op.step(input);
    }

    double result;
    op.value(&result);
    return result;
  }
};

// Test basic construction
TEST_F(EWMATest, BasicConstruction) {
  ewma<int, double> op(0.1, 0);
  EXPECT_DOUBLE_EQ(op.a1, 0.9);
  EXPECT_DOUBLE_EQ(op.a1_n, 1.0);
  EXPECT_DOUBLE_EQ(op.s, 0.0);
  EXPECT_EQ(op.pos, 0);
}

// Test construction with alpha conversion
TEST_F(EWMATest, ConstructionWithPeriod) {
  ewma<int, double> op(20.0, 0); // Period format, should convert to alpha = 2/(20+1)
  double expected_alpha = 2.0 / 21.0;
  double expected_a1 = 1.0 - expected_alpha;
  EXPECT_NEAR(op.a1, expected_a1, 1e-10);
}

// Test single value
TEST_F(EWMATest, SingleValue) {
  ewma<int, double> op(0.2, 0);
  double input_value = 5.0;
  const double *input_ptr = &input_value;
  const double *const *input = &input_ptr;

  op.init(input);

  double result;
  op.value(&result);

  // For a single value, result should equal the input
  EXPECT_DOUBLE_EQ(result, input_value);
}

// Test two values
TEST_F(EWMATest, TwoValues) {
  double alpha = 0.3;
  ewma<int, double> op(alpha, 0);

  // First value
  double val1 = 10.0;
  const double *input_ptr = &val1;
  const double *const *input = &input_ptr;
  op.init(input);

  // Second value
  double val2 = 20.0;
  input_ptr = &val2;
  op.step(input);

  double result;
  op.value(&result);

  // Manual calculation: weighted average with weights (1-alpha), 1
  double w1 = 1.0 - alpha; // weight for first value
  double w2 = 1.0;         // weight for second value
  double expected = (val1 * w1 + val2 * w2) / (w1 + w2);

  EXPECT_NEAR(result, expected, 1e-10);
}

// Test against naive implementation with random data
TEST_F(EWMATest, CompareWithNaiveImplementation) {
  std::vector<double> alphas = {0.1, 0.2, 0.5, 0.8};
  std::vector<size_t> data_sizes = {5, 10, 20, 50};

  for (double alpha : alphas) {
    for (size_t n : data_sizes) {
      // Generate random data
      std::vector<double> data = generate_random_data(n, -100.0, 100.0, 42);

      // Calculate using incremental EWMA
      ewma<int, double> op(alpha, 0);
      const double *input_ptr = data.data();
      const double *const *input = &input_ptr;

      op.init(input);
      for (size_t i = 1; i < data.size(); ++i) {
        input_ptr = &data[i];
        op.step(input);
      }

      double incremental_result;
      op.value(&incremental_result);

      // Calculate using naive implementation
      std::span<const double> data_span(data);
      double naive_result = ewma_naive(data_span, alpha);

      EXPECT_NEAR(incremental_result, naive_result, 1e-8) << "Alpha: " << alpha << ", Data size: " << n;
    }
  }
}

// Test rolling window functionality
TEST_F(EWMATest, RollingWindow) {
  double alpha = 0.3;
  size_t window_size = 5;

  // Generate test data
  std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0};

  // Use the rolling window simulation
  double rolling_result = simulate_rolling_window(data, window_size, alpha);

  // Calculate expected result using naive implementation on the last window
  std::vector<double> last_window(data.end() - static_cast<ptrdiff_t>(window_size), data.end());
  std::span<const double> window_span(last_window);
  double expected = ewma_naive(window_span, alpha);

  EXPECT_NEAR(rolling_result, expected, 1e-8);
}

// Test rolling window with random data
TEST_F(EWMATest, RollingWindowRandomData) {
  std::vector<double> alphas = {0.1, 0.4, 0.7};
  std::vector<size_t> window_sizes = {3, 7, 15};

  for (double alpha : alphas) {
    for (size_t window_size : window_sizes) {
      // Generate random data with more points than window size
      size_t total_size = window_size + 10;
      std::vector<double> data = generate_random_data(total_size, -50.0, 50.0, 123);

      // Use the rolling window simulation
      double rolling_result = simulate_rolling_window(data, window_size, alpha);

      // Calculate expected result using naive implementation on the last window
      std::vector<double> last_window(data.end() - static_cast<ptrdiff_t>(window_size), data.end());
      std::span<const double> window_span(last_window);
      double expected = ewma_naive(window_span, alpha);

      EXPECT_NEAR(rolling_result, expected, 1e-8) << "Alpha: " << alpha << ", Window size: " << window_size;
    }
  }
}

// Test inverse operation
TEST_F(EWMATest, InverseOperation) {
  double alpha = 0.2;
  ewma<int, double> op(alpha, 0);

  std::vector<double> values = {10.0, 20.0, 30.0};
  const double *input_ptr;
  const double *const *input = &input_ptr;

  // Add all values
  input_ptr = &values[0];
  op.init(input);

  for (size_t i = 1; i < values.size(); ++i) {
    input_ptr = &values[i];
    op.step(input);
  }

  double result_before;
  op.value(&result_before);

  // Remove the first value
  input_ptr = &values[0];
  op.inverse(input);

  double result_after;
  op.value(&result_after);

  // Calculate expected result (values[1] and values[2] only)
  std::vector<double> remaining_values(values.begin() + 1, values.end());
  std::span<const double> remaining_span(remaining_values);
  double expected = ewma_naive(remaining_span, alpha);

  EXPECT_NEAR(result_after, expected, 1e-8);
}

// Test edge case: very small alpha
TEST_F(EWMATest, SmallAlpha) {
  double alpha = 1e-6;
  ewma<int, double> op(alpha, 0);

  std::vector<double> data = {1.0, 100.0, 1.0};
  const double *input_ptr;
  const double *const *input = &input_ptr;

  input_ptr = &data[0];
  op.init(input);

  for (size_t i = 1; i < data.size(); ++i) {
    input_ptr = &data[i];
    op.step(input);
  }

  double result;
  op.value(&result);

  std::span<const double> data_span(data);
  double expected = ewma_naive(data_span, alpha);

  EXPECT_NEAR(result, expected, 1e-6);
}

// Test edge case: alpha close to 1
TEST_F(EWMATest, LargeAlpha) {
  double alpha = 0.99;
  ewma<int, double> op(alpha, 0);

  std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
  const double *input_ptr;
  const double *const *input = &input_ptr;

  input_ptr = &data[0];
  op.init(input);

  for (size_t i = 1; i < data.size(); ++i) {
    input_ptr = &data[i];
    op.step(input);
  }

  double result;
  op.value(&result);

  std::span<const double> data_span(data);
  double expected = ewma_naive(data_span, alpha);

  EXPECT_NEAR(result, expected, 1e-8);
}

// Test with different positions
TEST_F(EWMATest, DifferentPositions) {
  double alpha = 0.3;
  size_t pos = 2; // Use position 2 instead of 0
  ewma<int, double> op(alpha, pos);

  // Create multi-column data
  std::vector<std::vector<double>> multi_data = {
      {10.0, 20.0, 100.0, 40.0}, {15.0, 25.0, 200.0, 45.0}, {20.0, 30.0, 300.0, 50.0}};

  // Extract column at position 2 for comparison
  std::vector<double> expected_data = {100.0, 200.0, 300.0};

  // Run EWMA on multi-column data
  for (size_t i = 0; i < multi_data.size(); ++i) {
    const double *input_ptr = multi_data[i].data();
    const double *const *input = &input_ptr;

    if (i == 0) {
      op.init(input);
    } else {
      op.step(input);
    }
  }

  double result;
  op.value(&result);

  // Compare with naive implementation on extracted column
  std::span<const double> expected_span(expected_data);
  double expected = ewma_naive(expected_span, alpha);

  EXPECT_NEAR(result, expected, 1e-8);
}
