#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "opflow/graph_node.hpp"
#include "opflow/op/sum.hpp"
#include "opflow/op_exec.hpp"
#include "opflow/pipeline.hpp"

namespace {
using namespace opflow;

class PipelineBasicTest : public ::testing::Test {
protected:
  using op_type = op_base<double>;
  using op_exec_type = op_exec<double>;
  using pipeline_type = pipeline<double>;

  void SetUp() override { num_groups = 2; }

  size_t num_groups;
};

TEST_F(PipelineBasicTest, EmptyPipeline) {
  pipeline_type p(num_groups);
  EXPECT_EQ(p.num_groups(), num_groups);
  EXPECT_EQ(p.num_stages(), 0);
  EXPECT_EQ(p.num_inputs(), 0);
  EXPECT_EQ(p.num_outputs(), 0);
}

TEST_F(PipelineBasicTest, SingleOpExecStage) {
  // Create a simple graph with rolling sum
  graph_node<op_type, double> g;
  auto root = g.root<dag_root_type<op_type>>(1);
  auto sum_node = g.add<op::sum>(3).depends(root | 0); // 3-period rolling sum
  g.add_output(sum_node);

  op_exec_type exec(g, num_groups);
  pipeline_type p(num_groups);
  p.add_stage(&exec);

  EXPECT_EQ(p.num_stages(), 1);
  EXPECT_EQ(p.num_inputs(), 1);
  EXPECT_EQ(p.num_outputs(), 1);

  // Test data processing
  std::vector<double> input = {10.0};
  std::vector<double> output(1);

  auto result1 = p.on_data(1.0, input.data(), output.data(), 0);
  EXPECT_TRUE(result1.has_value());
  EXPECT_DOUBLE_EQ(*result1, 1.0);
  EXPECT_DOUBLE_EQ(output[0], 10.0);

  input[0] = 20.0;
  auto result2 = p.on_data(2.0, input.data(), output.data(), 0);
  EXPECT_TRUE(result2.has_value());
  EXPECT_DOUBLE_EQ(*result2, 2.0);
  EXPECT_DOUBLE_EQ(output[0], 30.0); // sum of 10 and 20
}

TEST_F(PipelineBasicTest, TwoStageOpToOp) {
  // Stage 1: op_exec with rolling sum (1 input -> 1 output)
  graph_node<op_type, double> g1;
  auto root1 = g1.root<dag_root_type<op_type>>(1);
  auto sum_node1 = g1.add<op::sum>(2).depends(root1 | 0);
  g1.add_output(sum_node1);
  op_exec_type exec1(g1, num_groups);

  // Stage 2: another op_exec with rolling sum (1 input -> 1 output)
  graph_node<op_type, double> g2;
  auto root2 = g2.root<dag_root_type<op_type>>(1);
  auto sum_node2 = g2.add<op::sum>(2).depends(root2 | 0);
  g2.add_output(sum_node2);
  op_exec_type exec2(g2, num_groups);

  // Create pipeline
  pipeline_type p(num_groups);
  p.add_stage(&exec1);
  p.add_stage(&exec2);

  EXPECT_EQ(p.num_stages(), 2);
  EXPECT_EQ(p.num_inputs(), 1);
  EXPECT_EQ(p.num_outputs(), 1);

  // Test data flow
  std::vector<double> input = {10.0};
  std::vector<double> output(1);

  // First value: stage1 sum=10, stage2 sum=10
  auto result1 = p.on_data(1.0, input.data(), output.data(), 0);
  EXPECT_TRUE(result1.has_value());
  EXPECT_DOUBLE_EQ(output[0], 10.0);

  // Second value: stage1 sum=20 (10+10), stage2 sum=30 (10+20)
  input[0] = 10.0;
  auto result2 = p.on_data(2.0, input.data(), output.data(), 0);
  EXPECT_TRUE(result2.has_value());
  EXPECT_DOUBLE_EQ(output[0], 30.0);

  // Third value: stage1 sum=20 (10+10), stage2 sum=40 (20+20)
  input[0] = 10.0;
  auto result3 = p.on_data(3.0, input.data(), output.data(), 0);
  EXPECT_TRUE(result3.has_value());
  EXPECT_DOUBLE_EQ(output[0], 40.0);
}

TEST_F(PipelineBasicTest, InputOutputMismatchThrows) {
  // Create incompatible stages
  graph_node<op_type, double> g1;
  auto root1 = g1.root<dag_root_type<op_type>>(1);
  auto sum_node = g1.add<op::sum>(2).depends(root1 | 0);
  g1.add_output(sum_node);
  op_exec_type exec1(g1, num_groups);

  // This stage expects 2 inputs, but previous stage outputs 1
  graph_node<op_type, double> g2;
  auto root2 = g2.root<dag_root_type<op_type>>(2);
  auto add = g2.add<op::add2>().depends(root2 | 0, root2 | 1);
  g2.add_output(add);
  op_exec_type exec2(g2, num_groups);

  pipeline_type p(num_groups);
  p.add_stage(&exec1);

  // Should throw because of input/output mismatch
  EXPECT_THROW(p.add_stage(&exec2), std::runtime_error);
}

TEST_F(PipelineBasicTest, NumGroupsMismatchThrows) {
  graph_node<op_type, double> g1;
  auto root1 = g1.root<dag_root_type<op_type>>(1);
  auto sum_node = g1.add<op::sum>(2).depends(root1 | 0);
  g1.add_output(sum_node);

  // Create executor with different num_groups
  op_exec_type exec1(g1, num_groups);
  op_exec_type exec2(g1, num_groups + 1);

  pipeline_type p(num_groups);
  p.add_stage(&exec1);

  // Should throw because of num_groups mismatch
  EXPECT_THROW(p.add_stage(&exec2), std::runtime_error);
}

TEST_F(PipelineBasicTest, MultipleGroupsIndependent) {
  // Create a simple op_exec
  graph_node<op_type, double> g;
  auto root = g.root<dag_root_type<op_type>>(1);
  auto sum_node = g.add<op::sum>(2).depends(root | 0);
  g.add_output(sum_node);
  op_exec_type exec(g, num_groups);

  pipeline_type p(num_groups);
  p.add_stage(&exec);

  std::vector<double> input(1);
  std::vector<double> output(1);

  // Process different values for different groups
  input[0] = 10.0;
  p.on_data(1.0, input.data(), output.data(), 0);
  EXPECT_DOUBLE_EQ(output[0], 10.0);

  input[0] = 100.0;
  p.on_data(1.0, input.data(), output.data(), 1);
  EXPECT_DOUBLE_EQ(output[0], 100.0);

  // Second round - groups should have independent state
  input[0] = 5.0;
  p.on_data(2.0, input.data(), output.data(), 0);
  EXPECT_DOUBLE_EQ(output[0], 15.0); // 10 + 5

  input[0] = 50.0;
  p.on_data(2.0, input.data(), output.data(), 1);
  EXPECT_DOUBLE_EQ(output[0], 150.0); // 100 + 50
}

} // namespace
