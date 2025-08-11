#include <array>
#include <gtest/gtest.h>

#include "opflow/aggregator.hpp"
#include "opflow/aggregator_pipeline.hpp"
#include "opflow/graph.hpp"
#include "opflow/op/input.hpp"
#include "opflow/op/sum.hpp"

using namespace opflow;

TEST(AggregatorPipeline, CountLastAggregatorBasic) {
  using Time = int;
  using Data = double;
  using op_type = op_base<Time, Data>;
  using node_type = std::shared_ptr<op_type>;

  auto input = std::make_shared<op::root_input<Time, Data>>(1);
  auto sum = std::make_shared<op::sum<Time, Data>>();

  graph<node_type> g;
  g.add(input);
  g.add(sum, std::vector{input});

  std::unordered_map<node_type, window_descriptor<Time>> win;
  win[sum] = window_descriptor<Time>(true, size_t{1}); // cumulative

  auto pipe = std::make_shared<pipeline<Time, Data>>(g, sliding::step, win);
  auto agg = std::make_shared<count_last_aggregator<Time, Data>>(2, 1); // emit every 2 ticks

  aggregator_pipeline<Time, Data> driver(agg, pipe);

  std::array<Data, 1> v1{1.0}, v2{2.0}, v3{3.0}, v4{4.0};

  // Feed first tick (no emission)
  EXPECT_FALSE(driver.feed(1, v1));
  // Feed second tick -> emission at t=2 with value 2
  EXPECT_TRUE(driver.feed(2, v2));
  auto sum_out = driver.get_output(1);
  EXPECT_DOUBLE_EQ(sum_out[0], 2.0);
  // Feed third tick (no emission)
  EXPECT_FALSE(driver.feed(3, v3));
  // Feed fourth tick -> emission at t=4 with value 4, sum becomes 2 + 4 = 6
  EXPECT_TRUE(driver.feed(4, v4));
  sum_out = driver.get_output(1);
  EXPECT_DOUBLE_EQ(sum_out[0], 6.0);
}
