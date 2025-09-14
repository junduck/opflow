#include <cstddef>
#include <numeric>
#include <random>
#include <vector>

#include <benchmark/benchmark.h>

#define CALC_STD 1

struct result_t {
  double open, high, low, close;
  double mean;
#if CALC_STD
  double std;
#endif
};

struct ohlcavg_online {
  double o, h, l, c;
  double m, s2;
  size_t n;

  void on_data(double p) {
    if (n == 0) {
      o = h = l = c = p;
      m = p;
    } else {
      if (p > h)
        h = p;
      if (p < l)
        l = p;
      c = p;
#if CALC_STD
      double d = p - m;
#endif
      m = m + (p - m) / (n + 1);
#if CALC_STD
      // welford's method for stddev
      s2 += d * (p - m);
#endif
    }
    n++;
  }

  result_t emit() {
    n = 0;
#if CALC_STD
    double const std = n > 1 ? std::sqrt(s2 / n) : 0.0;
    return {o, h, l, c, m, std};
#else
    return {o, h, l, c, m};
#endif
  }
};

struct ohlcavg_bulk {
  double o, h, l, c, m, std;

  void on_data(const std::vector<double> &prices) {
    o = prices[0];
    h = *std::ranges::max_element(prices);
    l = *std::ranges::min_element(prices);
    c = prices.back();
    m = std::accumulate(prices.begin(), prices.end(), 0.0) / prices.size();
#if CALC_STD
    double sum_sq_diff = 0.0;
    for (double p : prices) {
      double diff = p - m;
      sum_sq_diff += diff * diff;
    }
    std = std::sqrt(sum_sq_diff / prices.size());
#endif
  }

  result_t emit() {
#if CALC_STD
    return {o, h, l, c, m, std};
#else
    return {o, h, l, c, m};
#endif
  }
};

// Generate test data and window sizes with fixed seed for reproducible benchmarks
std::vector<double> generate_data(size_t count, uint32_t seed = 42) {
  std::vector<double> data;
  data.reserve(count);

  std::mt19937 gen(seed);
  std::normal_distribution<double> price_dist(100.0, 5.0); // Mean price 100, stddev 5

  for (size_t i = 0; i < count; ++i) {
    data.push_back(price_dist(gen));
  }

  return data;
}

std::vector<size_t> generate_window_sizes(size_t total_windows, uint32_t seed = 123) {
  std::vector<size_t> window_sizes;
  window_sizes.reserve(total_windows);

  std::mt19937 gen(seed);
  std::uniform_int_distribution<size_t> window_dist(10, 200);

  for (size_t i = 0; i < total_windows; ++i) {
    window_sizes.push_back(window_dist(gen));
  }

  return window_sizes;
}

// Global shared data for fair comparison
const size_t TOTAL_DATA_POINTS = 1000000; // 1 million data points
const size_t NUM_WINDOWS = 5000;          // Number of windows to process
const auto SHARED_DATA = generate_data(TOTAL_DATA_POINTS);
const auto SHARED_WINDOW_SIZES = generate_window_sizes(NUM_WINDOWS);

// Online model benchmark
static void BM_OnlineModel(benchmark::State &state) {
  for (auto _ : state) {
    ohlcavg_online online_model;
    size_t data_idx = 0;
    size_t current_window_size = 0;
    size_t window_idx = 0;

    for (size_t i = 0; i < TOTAL_DATA_POINTS && window_idx < NUM_WINDOWS; ++i) {
      online_model.on_data(SHARED_DATA[data_idx]);
      current_window_size++;
      data_idx++;

      if (current_window_size == SHARED_WINDOW_SIZES[window_idx]) {
        // Window complete, emit result and move to next window
        auto result = online_model.emit();
        benchmark::DoNotOptimize(result);

        current_window_size = 0;
        window_idx++;
      }
    }
  }

  state.SetItemsProcessed(static_cast<int64_t>(TOTAL_DATA_POINTS * static_cast<size_t>(state.iterations())));
}

// Bulk model benchmark
static void BM_BulkModel(benchmark::State &state) {
  for (auto _ : state) {
    ohlcavg_bulk bulk_model;
    std::vector<double> window_data;
    size_t data_idx = 0;
    size_t window_idx = 0;

    for (size_t i = 0; i < TOTAL_DATA_POINTS && window_idx < NUM_WINDOWS; ++i) {
      window_data.push_back(SHARED_DATA[data_idx]);
      data_idx++;

      if (window_data.size() == SHARED_WINDOW_SIZES[window_idx]) {
        // Window complete, process bulk data and emit result
        bulk_model.on_data(window_data);
        auto result = bulk_model.emit();
        benchmark::DoNotOptimize(result);

        window_data.clear();
        window_idx++;
      }
    }
  }

  state.SetItemsProcessed(static_cast<int64_t>(TOTAL_DATA_POINTS * static_cast<size_t>(state.iterations())));
}

// Register benchmarks
BENCHMARK(BM_OnlineModel)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_BulkModel)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();

/*
CALC_STD = 0

Run on (12 X 24 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB
  L1 Instruction 128 KiB
  L2 Unified 4096 KiB (x12)
Load Average: 2.36, 2.13, 2.04
-------------------------------------------------------------------------
Benchmark               Time             CPU   Iterations UserCounters...
-------------------------------------------------------------------------
BM_OnlineModel       1.96 ms         1.96 ms          356 items_per_second=509.831M/s
BM_BulkModel         1.71 ms         1.71 ms          410 items_per_second=585.477M/s

CALC_STD = 1

Run on (12 X 24 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB
  L1 Instruction 128 KiB
  L2 Unified 4096 KiB (x12)
Load Average: 2.50, 2.14, 2.04
-------------------------------------------------------------------------
Benchmark               Time             CPU   Iterations UserCounters...
-------------------------------------------------------------------------
BM_OnlineModel       1.95 ms         1.95 ms          357 items_per_second=513.809M/s
BM_BulkModel         2.33 ms         2.33 ms          298 items_per_second=428.983M/s

*/
