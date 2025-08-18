#include <gtest/gtest.h>

#include "opflow/op/root.hpp"
#include "opflow/op/sum.hpp"
#include "opflow/op_dag_exec.hpp"

namespace {
using namespace opflow;
using namespace opflow::literals;

class GraphExecTest : public ::testing::Test {
protected:
  using exec_type = op_dag_exec<double>;
  using op_type = typename exec_type::op_type;
  using node_type = typename exec_type::node_type;
  using root_type = op::graph_root<double>;
  using sum_type = op::sum<double>;
  using add2_type = op::add2<double>;

  void SetUp() override {
    root = std::make_shared<root_type>(1);
    sum_left = std::make_shared<sum_type>(2);  // 2-period rolling sum
    sum_right = std::make_shared<sum_type>(5); // 5-period rolling sum
    add2 = std::make_shared<add2_type>();

    g.add(root);
    g.add(sum_left, root | 0_p);
    g.add(sum_right, root | 0_p);
    g.add(add2, {sum_left | 0_p, sum_right | 0_p});
    std::vector<node_type> outputs = {sum_left, sum_right, add2};

    exec = std::make_unique<exec_type>(g, outputs);
  }

  graph<node_type> g;
  node_type root, sum_left, sum_right, add2;
  std::unique_ptr<exec_type> exec;
};

TEST_F(GraphExecTest, BasicStepFunctionality) {
  // Test basic pipeline step with single input value
  std::vector<double> input = {5.0};
  double tmp;

  // First step
  exec->on_data(1, input.data());

  // Verify outputs - sum_left and sum_right should both have value 5.0
  // add_final should have value 10.0 (5.0 + 5.0)

  sum_left->value(&tmp);
  EXPECT_DOUBLE_EQ(tmp, 5.0);
  sum_right->value(&tmp);
  EXPECT_DOUBLE_EQ(tmp, 5.0);
  add2->value(&tmp);
  EXPECT_DOUBLE_EQ(tmp, 10.0);
}

TEST_F(GraphExecTest, SlidingWindowBehavior) {
  // Test sliding window behavior with different window sizes
  std::vector<double> input = {1.0};

  // Add multiple data points to trigger window sliding
  for (int i = 1; i <= 12; ++i) {
    exec->on_data(i, input.data());
  }

  // sum_left has window size 2, so should have sum of last 2 values = 2.0
  // sum_right has window size 5, so should have sum of last 5 values = 5.0
  // add2 is stateless == sum_left + sum_right
  double tmp;
  sum_left->value(&tmp);
  EXPECT_DOUBLE_EQ(tmp, 2.0);
  sum_right->value(&tmp);
  EXPECT_DOUBLE_EQ(tmp, 5.0);
  add2->value(&tmp);
  EXPECT_DOUBLE_EQ(tmp, 7.0);
}
} // namespace
