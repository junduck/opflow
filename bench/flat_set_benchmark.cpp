#include <benchmark/benchmark.h>
#include <memory>
#include <random>
#include <unordered_set>
#include <vector>

#include "opflow/impl/flat_set.hpp"

using namespace opflow::impl;

// Helper function to generate shared_ptr<int> data
std::vector<std::shared_ptr<int>> generate_shared_ptr_data(size_t count, unsigned seed = 42) {
  std::vector<std::shared_ptr<int>> data;
  data.reserve(count);

  std::mt19937 gen(seed);
  std::uniform_int_distribution<int> dist(1, static_cast<int>(count * 2));

  for (size_t i = 0; i < count; ++i) {
    data.push_back(std::make_shared<int>(dist(gen)));
  }

  return data;
}

// Helper function to generate query keys (mix of existing and non-existing)
std::vector<std::shared_ptr<int>> generate_query_keys(const std::vector<std::shared_ptr<int>> &inserted_data,
                                                      size_t query_count, double hit_ratio = 0.7, unsigned seed = 123) {

  std::vector<std::shared_ptr<int>> queries;
  queries.reserve(query_count);

  std::mt19937 gen(seed);
  std::uniform_real_distribution<double> hit_dist(0.0, 1.0);
  std::uniform_int_distribution<size_t> existing_idx_dist(0, inserted_data.size() - 1);
  std::uniform_int_distribution<int> new_val_dist(static_cast<int>(inserted_data.size() * 2 + 1),
                                                  static_cast<int>(inserted_data.size() * 4));

  for (size_t i = 0; i < query_count; ++i) {
    if (hit_dist(gen) < hit_ratio && !inserted_data.empty()) {
      // Return existing element
      queries.push_back(inserted_data[existing_idx_dist(gen)]);
    } else {
      // Return new element (not in set)
      queries.push_back(std::make_shared<int>(new_val_dist(gen)));
    }
  }

  return queries;
}

// Benchmark for flat_set query performance
static void BM_FlatSet_Query(benchmark::State &state) {
  const size_t container_size = static_cast<size_t>(state.range(0));
  const size_t query_count = 1000;

  // Setup data
  auto data = generate_shared_ptr_data(container_size);
  auto queries = generate_query_keys(data, query_count);

  // Insert data into flat_set
  flat_set<std::shared_ptr<int>> fs;
  for (const auto &item : data) {
    fs.insert(item);
  }

  // Benchmark query operations
  for (auto _ : state) {
    size_t found_count = 0;
    for (const auto &query : queries) {
      if (fs.contains(query)) {
        ++found_count;
      }
    }
    benchmark::DoNotOptimize(found_count);
  }

  state.SetItemsProcessed(static_cast<int64_t>(static_cast<size_t>(state.iterations()) * query_count));
  state.SetLabel("flat_set");
}

// Benchmark for std::unordered_set query performance
static void BM_UnorderedSet_Query(benchmark::State &state) {
  const size_t container_size = static_cast<size_t>(state.range(0));
  const size_t query_count = 1000;

  // Setup data
  auto data = generate_shared_ptr_data(container_size);
  auto queries = generate_query_keys(data, query_count);

  // Insert data into unordered_set
  std::unordered_set<std::shared_ptr<int>> us;
  for (const auto &item : data) {
    us.insert(item);
  }

  // Benchmark query operations
  for (auto _ : state) {
    size_t found_count = 0;
    for (const auto &query : queries) {
      if (us.contains(query)) {
        ++found_count;
      }
    }
    benchmark::DoNotOptimize(found_count);
  }

  state.SetItemsProcessed(static_cast<int64_t>(static_cast<size_t>(state.iterations()) * query_count));
  state.SetLabel("unordered_set");
}

// Benchmark for flat_set find() operation
static void BM_FlatSet_Find(benchmark::State &state) {
  const size_t container_size = static_cast<size_t>(state.range(0));
  const size_t query_count = 1000;

  // Setup data
  auto data = generate_shared_ptr_data(container_size);
  auto queries = generate_query_keys(data, query_count);

  // Insert data into flat_set
  flat_set<std::shared_ptr<int>> fs;
  for (const auto &item : data) {
    fs.insert(item);
  }

  // Benchmark find operations
  for (auto _ : state) {
    size_t found_count = 0;
    for (const auto &query : queries) {
      if (fs.find(query) != fs.end()) {
        ++found_count;
      }
    }
    benchmark::DoNotOptimize(found_count);
  }

  state.SetItemsProcessed(static_cast<int64_t>(static_cast<size_t>(state.iterations()) * query_count));
  state.SetLabel("flat_set_find");
}

// Benchmark for std::unordered_set find() operation
static void BM_UnorderedSet_Find(benchmark::State &state) {
  const size_t container_size = static_cast<size_t>(state.range(0));
  const size_t query_count = 1000;

  // Setup data
  auto data = generate_shared_ptr_data(container_size);
  auto queries = generate_query_keys(data, query_count);

  // Insert data into unordered_set
  std::unordered_set<std::shared_ptr<int>> us;
  for (const auto &item : data) {
    us.insert(item);
  }

  // Benchmark find operations
  for (auto _ : state) {
    size_t found_count = 0;
    for (const auto &query : queries) {
      if (us.find(query) != us.end()) {
        ++found_count;
      }
    }
    benchmark::DoNotOptimize(found_count);
  }

  state.SetItemsProcessed(static_cast<int64_t>(static_cast<size_t>(state.iterations()) * query_count));
  state.SetLabel("unordered_set_find");
}

// Register benchmarks with different container sizes
BENCHMARK(BM_FlatSet_Query)
    ->Arg(10)
    ->Arg(50)
    ->Arg(100)
    ->Arg(250)
    ->Arg(500)
    ->Arg(1000)
    ->Arg(2500)
    ->Arg(5000)
    ->Arg(7500)
    ->Arg(10000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_UnorderedSet_Query)
    ->Arg(10)
    ->Arg(50)
    ->Arg(100)
    ->Arg(250)
    ->Arg(500)
    ->Arg(1000)
    ->Arg(2500)
    ->Arg(5000)
    ->Arg(7500)
    ->Arg(10000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_FlatSet_Find)
    ->Arg(10)
    ->Arg(50)
    ->Arg(100)
    ->Arg(250)
    ->Arg(500)
    ->Arg(1000)
    ->Arg(2500)
    ->Arg(5000)
    ->Arg(7500)
    ->Arg(10000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_UnorderedSet_Find)
    ->Arg(10)
    ->Arg(50)
    ->Arg(100)
    ->Arg(250)
    ->Arg(500)
    ->Arg(1000)
    ->Arg(2500)
    ->Arg(5000)
    ->Arg(7500)
    ->Arg(10000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
