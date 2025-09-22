#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "opflow/graph_node.hpp"
#include "opflow/op/sum.hpp"
#include "opflow/op_exec.hpp"

namespace {
using namespace opflow;

class GraphExecFanoutTest : public ::testing::Test {
protected:
  using exec_type = op_exec<double>;
  using op_type = typename exec_type::op_type;
  using graph_node_type = std::shared_ptr<op_type>;
  using sum_type = op::sum<double>;
  using add2_type = op::add2<double>;

  void SetUp() override {
    // Create a simple DAG: root -> sum_left, sum_right -> add2
    root = g.root(1);
    sum_left = g.add<op::sum>(root | 0, 2);   // 2-period rolling sum
    sum_right = g.add<sum_type>(root | 0, 5); // 5-period rolling sum
    add2 = g.add<add2_type>(sum_left | 0, sum_right | 0);
    g.set_output(sum_left, sum_right, add2);

    // Create executor with 3 groups
    num_groups = 3;
    exec = std::make_unique<exec_type>(g, num_groups);
  }

  graph_node<op_type> g;
  graph_node_type root, sum_left, sum_right, add2;
  std::unique_ptr<exec_type> exec;
  size_t num_groups;
};

TEST_F(GraphExecFanoutTest, BasicConstructor) {
  EXPECT_EQ(exec->num_groups(), num_groups);
  EXPECT_EQ(exec->num_inputs(), 1);
  EXPECT_EQ(exec->num_outputs(), 3); // sum_left + sum_right + add2
}

TEST_F(GraphExecFanoutTest, InitializerListConstructor) {
  g.set_output(sum_left, sum_right, add2);
  auto exec2 = std::make_unique<exec_type>(g, 2);
  EXPECT_EQ(exec2->num_groups(), 2);
  EXPECT_EQ(exec2->num_inputs(), 1);
  EXPECT_EQ(exec2->num_outputs(), 3);
}

TEST_F(GraphExecFanoutTest, SingleGroupBasicFunctionality) {
  // Test basic functionality with single group (group 0)
  std::vector<double> input = {5.0};
  std::vector<double> output(3);

  // First step
  exec->on_data(1.0, input.data(), 0);
  exec->value(output.data(), 0);

  // Verify outputs - sum_left and sum_right should both have value 5.0
  // add2 should have value 10.0 (5.0 + 5.0)
  EXPECT_DOUBLE_EQ(output[0], 5.0);  // sum_left
  EXPECT_DOUBLE_EQ(output[1], 5.0);  // sum_right
  EXPECT_DOUBLE_EQ(output[2], 10.0); // add2
}

TEST_F(GraphExecFanoutTest, MultipleGroupsIndependentState) {
  // Test that different groups maintain independent state
  std::vector<double> input1 = {10.0};
  std::vector<double> input2 = {20.0};
  std::vector<double> input3 = {30.0};
  std::vector<double> output(3);

  // Send different values to different groups
  exec->on_data(1.0, input1.data(), 0);
  exec->on_data(1.0, input2.data(), 1);
  exec->on_data(1.0, input3.data(), 2);

  // Check group 0
  exec->value(output.data(), 0);
  EXPECT_DOUBLE_EQ(output[0], 10.0); // sum_left
  EXPECT_DOUBLE_EQ(output[1], 10.0); // sum_right
  EXPECT_DOUBLE_EQ(output[2], 20.0); // add2

  // Check group 1
  exec->value(output.data(), 1);
  EXPECT_DOUBLE_EQ(output[0], 20.0); // sum_left
  EXPECT_DOUBLE_EQ(output[1], 20.0); // sum_right
  EXPECT_DOUBLE_EQ(output[2], 40.0); // add2

  // Check group 2
  exec->value(output.data(), 2);
  EXPECT_DOUBLE_EQ(output[0], 30.0); // sum_left
  EXPECT_DOUBLE_EQ(output[1], 30.0); // sum_right
  EXPECT_DOUBLE_EQ(output[2], 60.0); // add2
}

TEST_F(GraphExecFanoutTest, SlidingWindowBehavior) {
  // Test sliding window behavior with different window sizes
  std::vector<double> input = {1.0};
  std::vector<double> output(3);

  // Add multiple data points to trigger window sliding for group 0
  for (int i = 1; i <= 12; ++i) {
    exec->on_data(static_cast<double>(i), input.data(), 0);
  }

  exec->value(output.data(), 0);

  // sum_left has window size 2, so should have sum of last 2 values = 2.0
  // sum_right has window size 5, so should have sum of last 5 values = 5.0
  // add2 is stateless == sum_left + sum_right
  EXPECT_DOUBLE_EQ(output[0], 2.0); // sum_left
  EXPECT_DOUBLE_EQ(output[1], 5.0); // sum_right
  EXPECT_DOUBLE_EQ(output[2], 7.0); // add2
}

TEST_F(GraphExecFanoutTest, IndependentSlidingWindows) {
  // Test that sliding windows work independently across groups
  std::vector<double> input = {1.0};
  std::vector<double> output(3);

  // Group 0: Add 12 data points
  for (int i = 1; i <= 12; ++i) {
    exec->on_data(static_cast<double>(i), input.data(), 0);
  }

  // Group 1: Add only 3 data points
  for (int i = 1; i <= 3; ++i) {
    exec->on_data(static_cast<double>(i), input.data(), 1);
  }

  // Check group 0 (should have sliding window behavior)
  exec->value(output.data(), 0);
  EXPECT_DOUBLE_EQ(output[0], 2.0); // sum_left: last 2 values
  EXPECT_DOUBLE_EQ(output[1], 5.0); // sum_right: last 5 values
  EXPECT_DOUBLE_EQ(output[2], 7.0); // add2

  // Check group 1 (should have all 3 values)
  exec->value(output.data(), 1);
  EXPECT_DOUBLE_EQ(output[0], 2.0); // sum_left: last 2 values (1+1)
  EXPECT_DOUBLE_EQ(output[1], 3.0); // sum_right: all 3 values (1+1+1)
  EXPECT_DOUBLE_EQ(output[2], 5.0); // add2
}

TEST_F(GraphExecFanoutTest, TimeBasedWindowing) {
  // Create executor with time-based windows
  auto time_sum = g.add<op::sum<double>>(root | 0, 5.0); // 5-second time window
  g.set_output(time_sum);
  auto time_exec = std::make_unique<exec_type>(g, 1);

  std::vector<double> input = {1.0};
  std::vector<double> output(1);

  // Add data points at different times
  time_exec->on_data(1.0, input.data(), 0); // t=1
  time_exec->on_data(3.0, input.data(), 0); // t=3
  time_exec->on_data(7.0, input.data(), 0); // t=7 (should evict t=1)

  time_exec->value(output.data(), 0);

  // Should have values from t=3 and t=7 (within 5-second window from t=7)
  EXPECT_DOUBLE_EQ(output[0], 2.0);
}

TEST_F(GraphExecFanoutTest, StressTestMultipleGroups) {
  // Stress test with many groups and data points
  const size_t large_num_groups = 10;
  std::vector<graph_node_type> outputs = {add2};
  g.set_output(outputs);
  auto stress_exec = std::make_unique<exec_type>(g, large_num_groups);

  std::vector<double> input = {1.0};
  std::vector<double> output(1);

  // Send 100 data points to each group
  for (size_t group = 0; group < large_num_groups; ++group) {
    for (int i = 1; i <= 100; ++i) {
      stress_exec->on_data(static_cast<double>(i), input.data(), group);
    }
  }

  // Verify all groups have consistent results
  for (size_t group = 0; group < large_num_groups; ++group) {
    stress_exec->value(output.data(), group);
    EXPECT_DOUBLE_EQ(output[0], 7.0); // Expected result for sliding windows
  }
}

TEST_F(GraphExecFanoutTest, BoundaryConditions) {
  // Test boundary conditions
  std::vector<double> input = {0.0};
  std::vector<double> output(3);

  // Test with zero input
  exec->on_data(1.0, input.data(), 0);
  exec->value(output.data(), 0);

  EXPECT_DOUBLE_EQ(output[0], 0.0); // sum_left
  EXPECT_DOUBLE_EQ(output[1], 0.0); // sum_right
  EXPECT_DOUBLE_EQ(output[2], 0.0); // add2

  // Test with negative input
  input[0] = -5.0;
  exec->on_data(2.0, input.data(), 0);
  exec->value(output.data(), 0);

  EXPECT_DOUBLE_EQ(output[0], -5.0);  // sum_left (0 + (-5))
  EXPECT_DOUBLE_EQ(output[1], -5.0);  // sum_right (0 + (-5))
  EXPECT_DOUBLE_EQ(output[2], -10.0); // add2
}

TEST_F(GraphExecFanoutTest, GroupIndexBounds) {
  std::vector<double> input = {1.0};
  std::vector<double> output(3);

  // Test valid group indices
  for (size_t i = 0; i < num_groups; ++i) {
    EXPECT_NO_THROW(exec->on_data(1.0, input.data(), i));
    EXPECT_NO_THROW(exec->value(output.data(), i));
  }

  // Note: Invalid group indices would typically be caught by assertions
  // in debug builds or cause undefined behavior in release builds
  // We can't easily test this without modifying the implementation
}

} // namespace
