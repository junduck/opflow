#include <gtest/gtest.h>

#include "opflow/graph.hpp"
#include "opflow/op/input.hpp"
#include "opflow/op/math.hpp"
#include "opflow/op/sum.hpp"
#include "opflow/pipeline.hpp"

namespace {
using namespace opflow;

using Time = int;
using Data = double;

class PipelineTest : public ::testing::Test {
protected:
  using op_type = op_base<Time, Data>;
  using node_type = std::shared_ptr<op_type>;
  using pipeline_type = opflow::pipeline<Time, Data>;
  using sum_type = op::sum<Time>;
  using add_type = op::add<Time>;
  using vect = std::vector<node_type>;

  void SetUp() override {
    auto input = std::make_shared<op::root_input<Time>>(1); // Single input value
    auto sum_left = std::make_shared<sum_type>();
    auto sum_right = std::make_shared<sum_type>();
    auto add_final = std::make_shared<add_type>();

    g.add_vertex(input);
    g.add_vertex(sum_left, vect{input});
    g.add_vertex(sum_right, vect{input});
    g.add_vertex(add_final, vect{sum_left, sum_right});

    win[sum_left] = window_descriptor<Time>(false, 10);
    win[sum_right] = window_descriptor<Time>(false, 7);
    win[add_final] = window_descriptor<Time>(false, 5);

    p = std::make_unique<pipeline_type>(g, sliding::time, win);
  }

  graph<node_type> g;
  std::unordered_map<node_type, window_descriptor<Time>> win;

  std::unique_ptr<pipeline_type> p;
};

TEST_F(PipelineTest, BasicStepFunctionality) {
  // Test basic pipeline step with single input value
  std::vector<Data> input_data = {5.0};

  // First step
  p->step(1, input_data);

  // Verify outputs - sum_left and sum_right should both have value 5.0
  // add_final should have value 10.0 (5.0 + 5.0)
  auto sum_left_output = p->get_output(1);
  auto sum_right_output = p->get_output(2);
  auto add_final_output = p->get_output(3);

  EXPECT_EQ(sum_left_output.size(), 1);
  EXPECT_EQ(sum_right_output.size(), 1);
  EXPECT_EQ(add_final_output.size(), 1);

  EXPECT_DOUBLE_EQ(sum_left_output[0], 5.0);
  EXPECT_DOUBLE_EQ(sum_right_output[0], 5.0);
  EXPECT_DOUBLE_EQ(add_final_output[0], 10.0);
}

TEST_F(PipelineTest, MultipleStepsAccumulation) {
  // Test accumulation over multiple steps
  std::vector<Data> input_data = {3.0};

  // Step 1: input = 3.0
  p->step(1, input_data);
  auto add_final_output = p->get_output(3);
  EXPECT_DOUBLE_EQ(add_final_output[0], 6.0); // 3.0 + 3.0

  // Step 2: input = 3.0 again
  p->step(2, input_data);
  add_final_output = p->get_output(3);
  EXPECT_DOUBLE_EQ(add_final_output[0], 12.0); // (3.0+3.0) + (3.0+3.0)

  // Step 3: input = 2.0
  input_data[0] = 2.0;
  p->step(3, input_data);
  add_final_output = p->get_output(3);
  EXPECT_DOUBLE_EQ(add_final_output[0], 16.0); // (3.0+3.0+2.0) + (3.0+3.0+2.0)
}

TEST_F(PipelineTest, SlidingWindowBehavior) {
  // Test sliding window behavior with different window sizes
  std::vector<Data> input_data = {1.0};

  // Add multiple data points to trigger window sliding
  for (int i = 1; i <= 12; ++i) {
    p->step(i, input_data);
  }

  // sum_left has window size 10, so should have sum of last 10 values = 10.0
  // sum_right has window size 7, so should have sum of last 7 values = 7.0
  // add_final has window size 5, should consider last 5 outputs from left+right
  auto sum_left_output = p->get_output(1);
  auto sum_right_output = p->get_output(2);
  auto add_final_output = p->get_output(3);

  EXPECT_DOUBLE_EQ(sum_left_output[0], 10.0);
  EXPECT_DOUBLE_EQ(sum_right_output[0], 7.0);
  // add_final should add the last values of sum_left and sum_right
  EXPECT_DOUBLE_EQ(add_final_output[0], 17.0); // 10.0 + 7.0
}

TEST_F(PipelineTest, MonotonicTimestampValidation) {
  // Test that non-monotonic timestamps are rejected
  std::vector<Data> input_data = {1.0};

  p->step(5, input_data);

  // Trying to step with an earlier timestamp should throw
  EXPECT_THROW(p->step(3, input_data), std::runtime_error);
}

TEST_F(PipelineTest, InputSizeValidation) {
  // Test that wrong input size is rejected
  std::vector<Data> wrong_size_input = {1.0, 2.0}; // Should be size 1

  EXPECT_THROW(p->step(1, wrong_size_input), std::runtime_error);

  std::vector<Data> empty_input; // Should be size 1
  EXPECT_THROW(p->step(1, empty_input), std::runtime_error);
}

// Simple linear pipeline test
class SimplePipelineTest : public ::testing::Test {
protected:
  using op_type = op_base<Time, Data>;
  using node_type = std::shared_ptr<op_type>;
  using pipeline_type = opflow::pipeline<Time, Data>;
  using sum_type = op::sum<Time>;
  using vect = std::vector<node_type>;

  void SetUp() override {
    auto input = std::make_shared<op::root_input<Time>>(1);
    auto sum1 = std::make_shared<sum_type>();
    auto sum2 = std::make_shared<sum_type>();

    g.add_vertex(input);
    g.add_vertex(sum1, vect{input});
    g.add_vertex(sum2, vect{sum1});

    win[sum1] = window_descriptor<Time>(false, 3);
    win[sum2] = window_descriptor<Time>(false, 2);

    p = std::make_unique<pipeline_type>(g, sliding::time, win);
  }

  graph<node_type> g;
  std::unordered_map<node_type, window_descriptor<Time>> win;
  std::unique_ptr<pipeline_type> p;
};

TEST_F(SimplePipelineTest, LinearAccumulation) {
  // Test simple linear accumulation
  std::vector<Data> input_data = {2.0};

  p->step(1, input_data);
  auto sum1_output = p->get_output(1);
  auto sum2_output = p->get_output(2);

  EXPECT_DOUBLE_EQ(sum1_output[0], 2.0); // First value
  EXPECT_DOUBLE_EQ(sum2_output[0], 2.0); // First value

  p->step(2, input_data);
  sum1_output = p->get_output(1);
  sum2_output = p->get_output(2);

  EXPECT_DOUBLE_EQ(sum1_output[0], 4.0); // 2.0 + 2.0
  EXPECT_DOUBLE_EQ(sum2_output[0], 6.0); // 2.0 + 4.0
}

} // namespace
