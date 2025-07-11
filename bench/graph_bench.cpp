#include "benchmark/benchmark.h"
#include "opflow/graph.hpp"
#include <random>
#include <unordered_set>

using namespace opflow;

// Benchmark for linear chain dependency
static void BM_LinearChain(benchmark::State &state) {
  const auto n = static_cast<int>(state.range(0));

  for (auto _ : state) {
    TopologicalSorter<int> sorter;

    // Create linear chain: 0 -> 1 -> 2 -> ... -> n-1
    sorter.add(0);
    for (int i = 1; i < n; ++i) {
      sorter.add(i, {i - 1});
    }

    auto result = sorter.static_order();
    benchmark::DoNotOptimize(result);
  }

  state.SetComplexityN(n);
}

// Benchmark for star pattern (one node depends on many)
static void BM_StarPattern(benchmark::State &state) {
  const auto n = static_cast<int>(state.range(0));

  for (auto _ : state) {
    TopologicalSorter<int> sorter;

    // Create star pattern: {0, 1, 2, ..., n-2} -> n-1
    std::unordered_set<int> deps;
    for (int i = 0; i < n - 1; ++i) {
      sorter.add(i);
      deps.insert(i);
    }
    sorter.add(n - 1, deps);

    auto result = sorter.static_order();
    benchmark::DoNotOptimize(result);
  }

  state.SetComplexityN(n);
}

// Benchmark for random DAG
static void BM_RandomDAG(benchmark::State &state) {
  const auto n = static_cast<int>(state.range(0));
  std::mt19937 gen(42); // Fixed seed for reproducibility
  std::uniform_real_distribution<> dis(0.0, 1.0);

  for (auto _ : state) {
    TopologicalSorter<int> sorter;

    // Add nodes first
    for (int i = 0; i < n; ++i) {
      sorter.add(i);
    }

    // Add random dependencies (ensuring no cycles by only depending on lower-numbered nodes)
    for (int i = 1; i < n; ++i) {
      std::unordered_set<int> deps;
      for (int j = 0; j < i; ++j) {
        if (dis(gen) < 0.1) { // 10% probability of dependency
          deps.insert(j);
        }
      }
      if (!deps.empty()) {
        sorter.add(i, deps); // Re-add with dependencies
      }
    }

    auto result = sorter.static_order();
    benchmark::DoNotOptimize(result);
  }

  state.SetComplexityN(n);
}

// Benchmark for interactive processing
static void BM_InteractiveProcessing(benchmark::State &state) {
  const auto n = static_cast<int>(state.range(0));

  for (auto _ : state) {
    TopologicalSorter<int> sorter;

    // Create linear chain
    sorter.add(0);
    for (int i = 1; i < n; ++i) {
      sorter.add(i, {i - 1});
    }

    sorter.prepare();

    while (!sorter.done()) {
      auto ready = sorter.get_ready(1); // Process one at a time
      sorter.mark_done(ready);
    }
  }

  state.SetComplexityN(n);
}

// Register benchmarks
BENCHMARK(BM_LinearChain)->Range(8, 8 << 10)->Complexity();
BENCHMARK(BM_StarPattern)->Range(8, 8 << 10)->Complexity();
BENCHMARK(BM_RandomDAG)->Range(8, 8 << 8)->Complexity();
BENCHMARK(BM_InteractiveProcessing)->Range(8, 8 << 10)->Complexity();

BENCHMARK_MAIN();
