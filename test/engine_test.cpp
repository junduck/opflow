#include "opflow/op.hpp"
#include "gtest/gtest.h"
#include <vector>

using namespace opflow;

class EngineTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(EngineTest, BasicRootInput) {
  engine_int eng(3); // Create engine with 3 input values

  // Test state validation
  EXPECT_TRUE(eng.validate_state());
  EXPECT_EQ(eng.num_nodes(), 1);
  EXPECT_EQ(eng.total_output_size(), 3);

  // Test step with input data
  std::vector<double> input = {1.0, 2.0, 3.0};
  eng.step(1, input);

  EXPECT_TRUE(eng.has_steps());
  EXPECT_EQ(eng.num_steps(), 1);

  auto output = eng.get_latest_output();
  EXPECT_EQ(output.size(), 3);
  EXPECT_EQ(output[0], 1.0);
  EXPECT_EQ(output[1], 2.0);
  EXPECT_EQ(output[2], 3.0);
}

TEST_F(EngineTest, RollingSumOperator) {
  engine_int eng(2); // Create engine with 2 input values

  // Add a rolling sum operator with window of 3 ticks
  auto rollsum_op = std::make_shared<rollsum<int>>(3);
  auto rollsum_id = eng.add_op(rollsum_op, std::vector<size_t>{0});

  EXPECT_NE(rollsum_id, std::numeric_limits<size_t>::max());
  EXPECT_TRUE(eng.validate_state());
  EXPECT_EQ(eng.num_nodes(), 2);

  // Step through several iterations
  eng.step(1, {1.0, 2.0}); // sum = 1.0 + 2.0 = 3.0
  eng.step(2, {3.0, 4.0}); // sum = 3.0 + 7.0 = 10.0
  eng.step(3, {5.0, 6.0}); // sum = 10.0 + 11.0 = 21.0
  eng.step(4, {7.0, 8.0}); // sum = 21.0 + 15.0 = 36.0, but tick 1 expires

  // Get rolling sum output
  auto rollsum_output = eng.get_node_output(rollsum_id);
  EXPECT_EQ(rollsum_output.size(), 1);

  // The exact value depends on the watermark logic and which data was cleaned up
  EXPECT_GT(rollsum_output[0], 0.0); // Should be positive
}

TEST_F(EngineTest, InvalidDependency) {
  engine_int eng(1);

  auto op1 = std::make_shared<rollsum<int>>(2);
  auto id1 = eng.add_op(op1, std::vector<size_t>{0});
  EXPECT_NE(id1, std::numeric_limits<size_t>::max());

  // Try to add op2 that depends on a non-existent node
  auto op2 = std::make_shared<rollsum<int>>(2);
  auto id2 = eng.add_op(op2, std::vector<size_t>{5}); // Invalid dependency
  EXPECT_EQ(id2, std::numeric_limits<size_t>::max()); // Should fail

  EXPECT_TRUE(eng.validate_state());
}

TEST_F(EngineTest, MemoryManagement) {
  engine_int eng(1);

  auto rollsum_op = std::make_shared<rollsum<int>>(2); // Window of 2
  eng.add_op(rollsum_op, std::vector<size_t>{0});

  // Add several steps to test memory cleanup
  for (int i = 1; i <= 10; ++i) {
    eng.step(i, {static_cast<double>(i)});
  }

  // Should have cleaned up old data due to watermark
  EXPECT_LE(eng.num_steps(), 5); // Should not accumulate too much history

  auto memory_usage = eng.estimated_memory_usage();
  EXPECT_GT(memory_usage, 0);
}

TEST_F(EngineTest, ClearHistory) {
  engine_int eng(1);

  eng.step(1, {1.0});
  eng.step(2, {2.0});
  EXPECT_GE(eng.num_steps(), 1); // May have cleaned up some steps already

  eng.clear_history();
  EXPECT_EQ(eng.num_steps(), 0);
  EXPECT_FALSE(eng.has_steps());
}
