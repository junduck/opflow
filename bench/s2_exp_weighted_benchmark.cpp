#include <algorithm>
#include <benchmark/benchmark.h>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#include "dev/utils.hpp"
#include "s2_exp_weighted_sta.hpp"

using namespace opflow::op;

// Test function to compare the two variance calculation methods
void compare_variance_methods(const std::vector<double> &data, double alpha) {
  s2_exp_weighted_sta calculator(alpha);

  std::vector<std::pair<double, double>> results;
  results.reserve(data.size());

  for (double x : data) {
    auto [s2_standard, s2_welford] = calculator.step(x);
    results.emplace_back(s2_standard, s2_welford);
  }

  // Print comparison for the last few values
  std::cout << std::fixed << std::setprecision(8);
  std::cout << "Alpha: " << alpha << "\n";
  std::cout << "Last 5 variance comparisons (Standard vs Welford):\n";

  size_t start = std::max(size_t{0}, results.size() > 5 ? results.size() - 5 : 0);
  for (size_t i = start; i < results.size(); ++i) {
    double diff = std::abs(results[i].first - results[i].second);
    double rel_diff = results[i].second != 0.0 ? diff / std::abs(results[i].second) : 0.0;

    std::cout << "Step " << i + 1 << ": " << results[i].first << " vs " << results[i].second << " (diff: " << diff
              << ", rel: " << rel_diff << ")\n";
  }
  std::cout << "\n";
}

// Benchmark for small alpha (slow adaptation)
static void BM_S2ExpWeighted_SmallAlpha(benchmark::State &state) {
  const double alpha = 0.01; // Slow adaptation
  const size_t n_samples = 1000;

  auto data_range = utils::make_unif_range<double>(n_samples, -10.0, 10.0, 42);
  std::vector<double> samples;
  samples.reserve(n_samples);
  for (auto x : data_range) {
    samples.push_back(x);
  }

  for (auto _ : state) {
    s2_exp_weighted_sta calculator(alpha);

    for (double x : samples) {
      auto result = calculator.step(x);
      benchmark::DoNotOptimize(result);
    }
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n_samples));
}

// Benchmark for medium alpha
static void BM_S2ExpWeighted_MediumAlpha(benchmark::State &state) {
  const double alpha = 0.1; // Medium adaptation
  const size_t n_samples = 1000;

  auto data_range = utils::make_unif_range<double>(n_samples, -5.0, 5.0, 123);
  std::vector<double> samples;
  samples.reserve(n_samples);
  for (auto x : data_range) {
    samples.push_back(x);
  }

  for (auto _ : state) {
    s2_exp_weighted_sta calculator(alpha);

    for (double x : samples) {
      auto result = calculator.step(x);
      benchmark::DoNotOptimize(result);
    }
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n_samples));
}

// Benchmark for large alpha (fast adaptation)
static void BM_S2ExpWeighted_LargeAlpha(benchmark::State &state) {
  const double alpha = 0.5; // Fast adaptation
  const size_t n_samples = 1000;

  auto data_range = utils::make_unif_range<double>(n_samples, 0.0, 100.0, 456);
  std::vector<double> samples;
  samples.reserve(n_samples);
  for (auto x : data_range) {
    samples.push_back(x);
  }

  for (auto _ : state) {
    s2_exp_weighted_sta calculator(alpha);

    for (double x : samples) {
      auto result = calculator.step(x);
      benchmark::DoNotOptimize(result);
    }
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n_samples));
}

// Benchmark with non-stationary data (changing mean and variance)
static void BM_S2ExpWeighted_NonStationary(benchmark::State &state) {
  const double alpha = 0.2;
  const size_t n_samples = 1000;

  for (auto _ : state) {
    s2_exp_weighted_sta calculator(alpha);

    // Generate non-stationary data: changing mean and variance over time
    for (size_t i = 0; i < n_samples; ++i) {
      // Mean shifts from 0 to 10, variance increases over time
      double mean_shift = 10.0 * i / n_samples;
      double variance_scale = 1.0 + 3.0 * i / n_samples;

      std::mt19937 gen(static_cast<unsigned>(789 + i));
      std::normal_distribution<double> dist(mean_shift, std::sqrt(variance_scale));

      double x = dist(gen);
      auto result = calculator.step(x);
      benchmark::DoNotOptimize(result);
    }
  }

  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(n_samples));
}

// Test function to validate numerical accuracy
static void TestNumericalAccuracy() {
  std::cout << "=== Numerical Accuracy Test ===\n\n";

  // Test 1: Uniform random data
  {
    auto data_range = utils::make_unif_range<double>(500, -10.0, 10.0, 42);
    std::vector<double> samples;
    samples.reserve(500);
    for (auto x : data_range) {
      samples.push_back(x);
    }

    std::cout << "Test 1: Uniform random data [-10, 10], 500 samples\n";
    compare_variance_methods(samples, 0.1);
  }

  // Test 2: Normal distribution data
  {
    std::vector<double> samples;
    samples.reserve(500);

    std::mt19937 gen(123);
    std::normal_distribution<double> dist(5.0, 2.0);

    for (int i = 0; i < 500; ++i) {
      samples.push_back(dist(gen));
    }

    std::cout << "Test 2: Normal distribution (μ=5, σ=2), 500 samples\n";
    compare_variance_methods(samples, 0.05);
  }

  // Test 3: Data with outliers
  {
    auto data_range = utils::make_unif_range<double>(200, 0.0, 1.0, 456);
    std::vector<double> samples;
    samples.reserve(200);
    for (auto x : data_range) {
      samples.push_back(x);
    }

    // Add some outliers
    samples[50] = 100.0;
    samples[100] = -50.0;
    samples[150] = 75.0;

    std::cout << "Test 3: Uniform data [0, 1] with outliers, 200 samples\n";
    compare_variance_methods(samples, 0.2);
  }

  // Test 4: High smoothing factor
  {
    auto data_range = utils::make_unif_range<double>(500, -1.0, 1.0, 789);
    std::vector<double> samples;
    samples.reserve(500);
    for (auto x : data_range) {
      samples.push_back(x);
    }

    std::cout << "Test 4: High smoothing factor (α=0.8), 500 samples\n";
    compare_variance_methods(samples, 0.8);
  }
}

BENCHMARK(BM_S2ExpWeighted_SmallAlpha);
BENCHMARK(BM_S2ExpWeighted_MediumAlpha);
BENCHMARK(BM_S2ExpWeighted_LargeAlpha);
BENCHMARK(BM_S2ExpWeighted_NonStationary);

int main(int argc, char **argv) {
  // Run numerical accuracy tests first
  TestNumericalAccuracy();

  std::cout << "=== Performance Benchmarks ===\n\n";

  // Run benchmarks
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;
  benchmark::RunSpecifiedBenchmarks();

  return 0;
}
