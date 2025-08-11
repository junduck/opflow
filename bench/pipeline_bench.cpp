#include <benchmark/benchmark.h>
#include <memory>
#include <random>
#include <vector>

#include "opflow/graph.hpp"
#include "opflow/op/input.hpp"
#include "opflow/op/sum.hpp"
#include "opflow/pipeline.hpp"

namespace {
using namespace opflow;

using Time = int;
using Data = double;

// Benchmark fixture for pipeline chain overhead
class PipelineBenchmark : public benchmark::Fixture {
public:
  using op_type = op_base<Time, Data>;
  using node_type = std::shared_ptr<op_type>;
  using pipeline_type = opflow::pipeline<Time, Data>;
  using sum_type = op::sum<Time, Data>;
  using noop_type = op::noop<Time, Data>;
  using vect = std::vector<node_type>;

  void SetUp(const benchmark::State &state) override {
    // Get number of operators from state parameter
    num_operators = static_cast<size_t>(state.range(0));

    // Pre-generate random numbers to eliminate noise from RNG
    std::mt19937 gen(42); // Fixed seed for reproducibility
    std::uniform_real_distribution<Data> dist(1.0, 10.0);

    // Pre-generate enough data for all iterations
    input_data.clear();
    input_data.reserve(1000);
    for (size_t i = 0; i < 1000; ++i) {
      input_data.push_back(dist(gen));
    }

    // Build linear chain: input -> sum1 -> sum2 -> ... -> sumN
    BuildLinearChain();
  }

  void TearDown(const benchmark::State & /*state*/) override {
    // Clean up
    p.reset();
    g.clear();
    win.clear();
    input_data.clear();
  }

private:
  void BuildLinearChain() {
    g.clear();
    win.clear();

    // Create input node
    auto input = std::make_shared<op::root_input<Time, Data>>(1);
    g.add(input);

    // Create chain of sum operators
    node_type prev_node = input;
    for (size_t i = 0; i < num_operators; ++i) {
      auto sum_op = std::make_shared<sum_type>();
      g.add(sum_op, vect{prev_node});

      // Use small window size to reduce memory effects
      win[sum_op] = window_descriptor<Time>(false, 3);

      prev_node = sum_op;
    }

    // Create pipeline
    p = std::make_unique<pipeline_type>(g, sliding::time, win);
  }

public:
  size_t num_operators;
  std::vector<Data> input_data;
  graph<node_type> g;
  std::unordered_map<node_type, window_descriptor<Time>> win;
  std::unique_ptr<pipeline_type> p;
};

// Benchmark the overhead of adding operators to a linear chain
BENCHMARK_DEFINE_F(PipelineBenchmark, LinearChainOverhead)(benchmark::State &state) {
  size_t step_count = 0;
  size_t data_index = 0;

  for (auto _ : state) {
    // Use pre-generated data to avoid RNG overhead
    std::vector<Data> step_data = {input_data[data_index % input_data.size()]};

    // Step the pipeline
    p->step(static_cast<Time>(step_count + 1), step_data);

    // Prevent optimization of the output (force computation)
    auto output = p->get_output(num_operators); // Get final output
    benchmark::DoNotOptimize(output);

    ++step_count;
    ++data_index;
  }

  // Report metrics
  state.SetItemsProcessed(state.iterations());
  state.counters["operators"] = static_cast<double>(num_operators);
  state.counters["ops_per_step"] = static_cast<double>(num_operators);
}

// Register benchmark with different chain lengths
BENCHMARK_REGISTER_F(PipelineBenchmark, LinearChainOverhead)
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->Arg(32)
    ->Arg(64)
    ->Arg(128)
    ->Arg(256)
    ->Arg(512)
    ->Arg(1024)
    ->Unit(benchmark::kNanosecond);

} // namespace

BENCHMARK_MAIN();

// clang-format off
/*
CPU Caches:
  L1 Data 64 KiB
  L1 Instruction 128 KiB
  L2 Unified 4096 KiB (x12)
Load Average: 5.34, 4.37, 4.01
-----------------------------------------------------------------------------------------------------
Benchmark                                           Time             CPU   Iterations UserCounters...
-----------------------------------------------------------------------------------------------------
PipelineBenchmark/LinearChainOverhead/4           201 ns          200 ns      3540145 items_per_second=4.99812M/s operators=4 ops_per_step=4
PipelineBenchmark/LinearChainOverhead/8           324 ns          315 ns      2334213 items_per_second=3.17223M/s operators=8 ops_per_step=8
PipelineBenchmark/LinearChainOverhead/16          526 ns          525 ns      1323326 items_per_second=1.90514M/s operators=16 ops_per_step=16
PipelineBenchmark/LinearChainOverhead/32          954 ns          950 ns       734723 items_per_second=1.05254M/s operators=32 ops_per_step=32
PipelineBenchmark/LinearChainOverhead/64         1823 ns         1813 ns       391615 items_per_second=551.643k/s operators=64 ops_per_step=64
PipelineBenchmark/LinearChainOverhead/128        3574 ns         3555 ns       196532 items_per_second=281.318k/s operators=128 ops_per_step=128
PipelineBenchmark/LinearChainOverhead/256        7044 ns         7007 ns       101685 items_per_second=142.722k/s operators=256 ops_per_step=256
PipelineBenchmark/LinearChainOverhead/512       14848 ns        14282 ns        50170 items_per_second=70.0186k/s operators=512 ops_per_step=512
PipelineBenchmark/LinearChainOverhead/1024      32663 ns        31775 ns        24748 items_per_second=31.4711k/s operators=1.024k ops_per_step=1.024k
*/

// Time per op: 30 - 50 ns
