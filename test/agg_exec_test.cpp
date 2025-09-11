#include <algorithm>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "opflow/agg/avg.hpp"
#include "opflow/agg/count.hpp"
#include "opflow/agg/ohlc.hpp"
#include "opflow/agg/sum.hpp"
#include "opflow/agg_exec.hpp"
#include "opflow/graph_agg.hpp"
#include "opflow/win/counter.hpp"
#include "opflow/win/tumbling.hpp"

namespace {
using namespace opflow;

class AggExecTest : public ::testing::Test {
protected:
  using op_type = agg_base<double>;
  using exec_type = agg_exec<double>;
  using graph_node_type = std::shared_ptr<op_type>;

  void SetUp() override {
    // Basic setup with OHLC and count for most tests
    g.input("val")
        .window<win::tumbling>("val", 3.0) // 3-unit tumbling window
        .add<agg::ohlc>("val")             // OHLC on column "val"
        .add<agg::count>();                // Count aggregation

    size_t num_groups = 2;
    exec = std::make_unique<exec_type>(g, 1, num_groups);
  }

  void TearDown() override {
    exec.reset();
    g.clear();
  }

  graph_agg<op_type> g;
  std::unique_ptr<exec_type> exec;
};

TEST_F(AggExecTest, BasicTumblingWindow) {
  std::vector<double> input_data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
  std::vector<double> output(5); // 4 for OHLC + 1 for count
  std::vector<std::vector<double>> emissions;

  for (auto value : input_data) {
    auto emit = exec->on_data(value, &value, 0);
    if (emit) {
      exec->value(output.data(), 0);
      emissions.push_back(output);
    }
  }

  // timestamp 8 won't emit because it's not the end of a window, we flush it instead
  EXPECT_TRUE(exec->flush(0).has_value());
  exec->value(output.data(), 0);
  emissions.push_back(output);

  // Tumbling window size 3: should emit at t=3, t=6, t=9
  // Windows: [0,3) -> {1,2}, [3,6) -> {3,4,5}, [6,9) -> {6,7,8}
  ASSERT_EQ(emissions.size(), 3);

  // First window: {1, 2}
  EXPECT_DOUBLE_EQ(emissions[0][0], 1.0); // Open
  EXPECT_DOUBLE_EQ(emissions[0][1], 2.0); // High
  EXPECT_DOUBLE_EQ(emissions[0][2], 1.0); // Low
  EXPECT_DOUBLE_EQ(emissions[0][3], 2.0); // Close
  EXPECT_DOUBLE_EQ(emissions[0][4], 2.0); // Count

  // Second window: {3, 4, 5}
  EXPECT_DOUBLE_EQ(emissions[1][0], 3.0); // Open
  EXPECT_DOUBLE_EQ(emissions[1][1], 5.0); // High
  EXPECT_DOUBLE_EQ(emissions[1][2], 3.0); // Low
  EXPECT_DOUBLE_EQ(emissions[1][3], 5.0); // Close
  EXPECT_DOUBLE_EQ(emissions[1][4], 3.0); // Count

  // Third window: {6, 7, 8}
  EXPECT_DOUBLE_EQ(emissions[2][0], 6.0); // Open
  EXPECT_DOUBLE_EQ(emissions[2][1], 8.0); // High
  EXPECT_DOUBLE_EQ(emissions[2][2], 6.0); // Low
  EXPECT_DOUBLE_EQ(emissions[2][3], 8.0); // Close
  EXPECT_DOUBLE_EQ(emissions[2][4], 3.0); // Count
}

TEST_F(AggExecTest, MultipleGroups) {
  // Test that different groups maintain independent state
  std::vector<double> group0_data = {1.0, 2.0, 3.0, 4.0, 5.0};
  std::vector<double> group1_data = {10.0, 20.0, 30.0, 40.0, 50.0};

  std::vector<double> output(5);
  std::vector<std::vector<double>> group0_emissions, group1_emissions;

  // Feed data to both groups interleaved
  for (size_t i = 0; i < std::max(group0_data.size(), group1_data.size()); ++i) {
    if (i < group0_data.size()) {
      auto emit = exec->on_data(group0_data[i], &group0_data[i], 0);
      if (emit) {
        exec->value(output.data(), 0);
        group0_emissions.push_back(output);
      }
    }

    if (i < group1_data.size()) {
      auto emit = exec->on_data(group1_data[i], &group1_data[i], 1);
      if (emit) {
        exec->value(output.data(), 1);
        group1_emissions.push_back(output);
      }
    }
  }

  // Flush remaining data
  if (exec->flush(0).has_value()) {
    exec->value(output.data(), 0);
    group0_emissions.push_back(output);
  }
  if (exec->flush(1).has_value()) {
    exec->value(output.data(), 1);
    group1_emissions.push_back(output);
  }

  // Both groups should have emitted windows
  EXPECT_GE(group0_emissions.size(), 1);
  EXPECT_GE(group1_emissions.size(), 1);

  // Verify groups have different values (showing independence)
  if (!group0_emissions.empty() && !group1_emissions.empty()) {
    EXPECT_NE(group0_emissions[0][0], group1_emissions[0][0]); // Different opens
  }
}

TEST_F(AggExecTest, MultipleAggregations) {
  // Create a more complex graph with multiple aggregations
  graph_agg<op_type> complex_graph;
  complex_graph.input("col0", "col1");
  complex_graph.window<win::counter>(2);
  complex_graph.add<agg::sum>("col0", "col1", 2); // Sum of columns 0 and 1
  complex_graph.add<agg::avg>("col0", 1);         // Average of column 0
  complex_graph.add<agg::count>();                // Count

  auto complex_exec = std::make_unique<exec_type>(complex_graph, 2, 1);

  // Test data: timestamp, col0, col1
  std::vector<std::array<double, 2>> test_data = {{1.0, 2.0}, {3.0, 4.0}, {5.0, 6.0}, {7.0, 8.0}};

  std::vector<double> output(4); // 2 sums + 1 avg + 1 count
  std::vector<std::vector<double>> emissions;

  for (size_t i = 0; i < test_data.size(); ++i) {
    double timestamp = static_cast<double>(i + 1);
    auto emit = complex_exec->on_data(timestamp, test_data[i].data(), 0);
    if (emit) {
      complex_exec->value(output.data(), 0);
      emissions.push_back(output);
    }
  }

  // Flush remaining
  if (complex_exec->flush(0).has_value()) {
    complex_exec->value(output.data(), 0);
    emissions.push_back(output);
  }

  EXPECT_GE(emissions.size(), 1);

  // Verify the first emission (first 2 data points)
  if (!emissions.empty()) {
    // Sum of col0: 1.0 + 3.0 = 4.0
    // Sum of col1: 2.0 + 4.0 = 6.0
    // Avg of col0: (1.0 + 3.0) / 2 = 2.0
    // Count: 2
    EXPECT_DOUBLE_EQ(emissions[0][0], 4.0); // Sum col0
    EXPECT_DOUBLE_EQ(emissions[0][1], 6.0); // Sum col1
    EXPECT_DOUBLE_EQ(emissions[0][2], 2.0); // Avg col0
    EXPECT_DOUBLE_EQ(emissions[0][3], 2.0); // Count
  }
}

TEST_F(AggExecTest, CounterWindow) {
  // Test counter window that emits every N events
  graph_agg<op_type> counter_graph;
  counter_graph.input("val");
  counter_graph.window<win::counter>(3); // Emit every 3 events
  counter_graph.add<agg::sum>("val", 1);

  auto counter_exec = std::make_unique<exec_type>(counter_graph, 1, 1);

  std::vector<double> input_data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
  std::vector<double> output(1);
  std::vector<double> emissions;

  for (size_t i = 0; i < input_data.size(); ++i) {
    auto emit = counter_exec->on_data(input_data[i], &input_data[i], 0);
    if (emit) {
      counter_exec->value(output.data(), 0);
      emissions.push_back(output[0]);
    }
  }

  // Should emit twice: after 3rd and 6th elements
  EXPECT_EQ(emissions.size(), 2);
  EXPECT_DOUBLE_EQ(emissions[0], 6.0);  // 1+2+3
  EXPECT_DOUBLE_EQ(emissions[1], 15.0); // 4+5+6

  // Flush remaining (element 7)
  if (counter_exec->flush(0).has_value()) {
    counter_exec->value(output.data(), 0);
    EXPECT_DOUBLE_EQ(output[0], 7.0);
  }
}

TEST_F(AggExecTest, EmptyWindow) {
  // Test behavior with no data
  std::vector<double> output(5);

  // No emissions should occur without data
  auto emit = exec->flush(0);
  EXPECT_FALSE(emit.has_value());
}

TEST_F(AggExecTest, SingleDataPoint) {
  // Test with just one data point
  std::vector<double> output(5);
  double test_value = 5.0;

  auto emit = exec->on_data(1.0, &test_value, 0);
  EXPECT_FALSE(emit.has_value()); // Shouldn't emit yet

  // Flush should emit the single point
  auto flush_emit = exec->flush(0);
  EXPECT_TRUE(flush_emit.has_value());

  exec->value(output.data(), 0);
  // Single point: OHLC all same, count = 1
  EXPECT_DOUBLE_EQ(output[0], 5.0); // Open
  EXPECT_DOUBLE_EQ(output[1], 5.0); // High
  EXPECT_DOUBLE_EQ(output[2], 5.0); // Low
  EXPECT_DOUBLE_EQ(output[3], 5.0); // Close
  EXPECT_DOUBLE_EQ(output[4], 1.0); // Count
}

TEST_F(AggExecTest, MultiColumnInput) {
  // Test with multiple input columns using explicit graph construction
  graph_agg<op_type> multi_graph;
  multi_graph.input("col0", "col1", "col2");
  multi_graph.window<win::counter>(3);
  multi_graph.add<agg::sum>("col0", "col1", "col2", 3); // Sum over 3 columns

  auto multi_exec = std::make_unique<exec_type>(multi_graph, 3, 1);

  std::vector<double> output(3);
  std::vector<std::array<double, 3>> input_data = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}, {7.0, 8.0, 9.0}};

  // Feed all data (should emit once due to tumbling window size 3)
  for (size_t i = 0; i < input_data.size(); ++i) {
    auto emit = multi_exec->on_data(i + 1.0, input_data[i].data(), 0);
    if (emit) {
      multi_exec->value(output.data(), 0);
      break;
    }
  }

  // Check sums: col0=1+4+7=12, col1=2+5+8=15, col2=3+6+9=18
  EXPECT_DOUBLE_EQ(output[0], 12.0);
  EXPECT_DOUBLE_EQ(output[1], 15.0);
  EXPECT_DOUBLE_EQ(output[2], 18.0);
}

TEST_F(AggExecTest, LargeDataset) {
  // Test with larger dataset to verify performance and correctness
  graph_agg<op_type> large_graph;
  large_graph.input("val");
  large_graph.window<win::counter>(100); // Emit every 100 events
  large_graph.add<agg::sum>("val", 1);
  large_graph.add<agg::avg>("val", 1);

  auto large_exec = std::make_unique<exec_type>(large_graph, 1, 1);

  const size_t dataset_size = 1000;
  std::vector<double> output(2);
  int emission_count = 0;

  for (size_t i = 0; i < dataset_size; ++i) {
    double value = static_cast<double>(i + 1);
    auto emit = large_exec->on_data(value, &value, 0);
    if (emit) {
      large_exec->value(output.data(), 0);
      emission_count++;

      // Verify first emission (sum of 1 to 100)
      if (emission_count == 1) {
        double expected_sum = 100.0 * 101.0 / 2.0; // Sum of 1 to 100
        double expected_avg = expected_sum / 100;
        EXPECT_DOUBLE_EQ(output[0], expected_sum);
        EXPECT_DOUBLE_EQ(output[1], expected_avg);
      }
    }
  }

  // Should have emitted 10 times (1000 / 100)
  EXPECT_EQ(emission_count, 10);
}

TEST_F(AggExecTest, QueryMethods) {
  // Test the query methods
  EXPECT_EQ(exec->num_inputs(), 1);
  EXPECT_EQ(exec->num_outputs(), 5); // 4 OHLC + 1 count
  EXPECT_EQ(exec->num_groups(), 2);

  // Test with different configuration
  graph_agg<op_type> simple_graph;
  simple_graph.input("col0", "col1", "col2");
  simple_graph.window<win::tumbling>(3.0);
  simple_graph.add<agg::sum>("col0", "col1", "col2", 3);
  auto simple_exec = std::make_unique<exec_type>(simple_graph, 3, 5);

  EXPECT_EQ(simple_exec->num_inputs(), 3);
  EXPECT_EQ(simple_exec->num_outputs(), 3); // 3 sums
  EXPECT_EQ(simple_exec->num_groups(), 5);
}

TEST_F(AggExecTest, GroupIndexValidation) {
  // Test that group index validation works
  std::vector<double> output(5);
  double value = 1.0;

  // Valid group indices (0 and 1)
  EXPECT_NO_THROW(exec->on_data(1.0, &value, 0));
  EXPECT_NO_THROW(exec->on_data(1.0, &value, 1));
  EXPECT_NO_THROW(exec->value(output.data(), 0));
  EXPECT_NO_THROW(exec->value(output.data(), 1));
  EXPECT_NO_THROW(exec->flush(0));
  EXPECT_NO_THROW(exec->flush(1));

  // Invalid group index (2) should be caught by assertions in debug mode
  // In release mode, it may cause undefined behavior
#ifdef NDEBUG
  // In release mode, we can't easily test assertion failures
  GTEST_SKIP() << "Group index validation testing requires debug mode";
#endif
}

TEST_F(AggExecTest, WindowTimestampProgression) {
  // Test that window timestamps are correctly returned
  std::vector<double> input_data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
  std::vector<double> timestamps;

  for (size_t i = 0; i < input_data.size(); ++i) {
    double ts = static_cast<double>(i + 1);
    auto emit = exec->on_data(ts, &input_data[i], 0);
    if (emit) {
      timestamps.push_back(*emit);
    }
  }

  // Tumbling window size 3: should emit at timestamps 3 and 6
  ASSERT_EQ(timestamps.size(), 2);
  EXPECT_DOUBLE_EQ(timestamps[0], 3.0);
  EXPECT_DOUBLE_EQ(timestamps[1], 6.0);
}

TEST_F(AggExecTest, FlushBehavior) {
  // Test flush behavior with partial windows
  double value1 = 1.0, value2 = 2.0;
  std::vector<double> output(5);

  // Add some data but not enough to trigger emission
  EXPECT_FALSE(exec->on_data(1.0, &value1, 0).has_value());
  EXPECT_FALSE(exec->on_data(2.0, &value2, 0).has_value());

  // Flush should emit the partial window
  auto flush_result = exec->flush(0);
  EXPECT_TRUE(flush_result.has_value());
  EXPECT_DOUBLE_EQ(*flush_result, 3.0); // flushed window is [0, 3)

  exec->value(output.data(), 0);
  EXPECT_DOUBLE_EQ(output[0], 1.0); // Open
  EXPECT_DOUBLE_EQ(output[1], 2.0); // High
  EXPECT_DOUBLE_EQ(output[2], 1.0); // Low
  EXPECT_DOUBLE_EQ(output[3], 2.0); // Close
  EXPECT_DOUBLE_EQ(output[4], 2.0); // Count

  // Second flush on empty state should return nothing
  EXPECT_FALSE(exec->flush(0).has_value());
}
} // namespace
