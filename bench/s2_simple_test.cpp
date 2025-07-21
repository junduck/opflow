#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#include "dev/utils.hpp"
#include "s2_exp_weighted_sta.hpp"

using namespace opflow::op;

// Simple test function to compare the two variance calculation methods
void test_variance_methods(const std::string &test_name, const std::vector<double> &data, double alpha) {
  s2_exp_weighted_sta calculator(alpha);

  std::cout << "\n=== " << test_name << " ===\n";
  std::cout << "Alpha: " << alpha << ", Samples: " << data.size() << "\n";
  std::cout << std::fixed << std::setprecision(8);

  double max_abs_diff = 0.0;
  double max_rel_diff = 0.0;
  size_t diff_count = 0;

  for (size_t i = 0; i < data.size(); ++i) {
    auto [s2_standard, s2_welford] = calculator.step(data[i]);

    if (i > 0) { // Skip first iteration since both methods return 0.0
      double abs_diff = std::abs(s2_standard - s2_welford);
      double rel_diff = s2_welford != 0.0 ? abs_diff / std::abs(s2_welford) : 0.0;

      max_abs_diff = std::max(max_abs_diff, abs_diff);
      max_rel_diff = std::max(max_rel_diff, rel_diff);

      if (abs_diff > 1e-12) { // Count significant differences
        diff_count++;
      }
    }

    // Print some intermediate results
    if (i == 9 || i == 49 || i == 99 || i == data.size() - 1) {
      std::cout << "Step " << i + 1 << ": Standard=" << s2_standard << ", Welford=" << s2_welford;
      if (i > 0) {
        double abs_diff = std::abs(s2_standard - s2_welford);
        double rel_diff = s2_welford != 0.0 ? abs_diff / std::abs(s2_welford) : 0.0;
        std::cout << " (abs_diff=" << abs_diff << ", rel_diff=" << rel_diff << ")";
      }
      std::cout << "\n";
    }
  }

  std::cout << "\nSummary:\n";
  std::cout << "  Max absolute difference: " << max_abs_diff << "\n";
  std::cout << "  Max relative difference: " << max_rel_diff << "\n";
  std::cout << "  Significant differences: " << diff_count << "/" << (data.size() - 1) << "\n";

  // Validate numerical equivalence
  assert(max_abs_diff < 1e-10 && "Variance methods should produce nearly identical results");
  std::cout << "  ✓ Test passed: Methods are numerically equivalent\n";
}

int main() {
  std::cout << "=== s2_exp_weighted_sta Variance Method Comparison ===\n";

  // Test 1: Uniform random data with various alpha values
  {
    auto data_range = utils::make_unif_range<double>(1000, -10.0, 10.0, 42);
    std::vector<double> samples;
    samples.reserve(1000);
    for (auto x : data_range) {
      samples.push_back(x);
    }

    test_variance_methods("Uniform Random Data (α=0.01)", samples, 0.01);
    test_variance_methods("Uniform Random Data (α=0.1)", samples, 0.1);
    test_variance_methods("Uniform Random Data (α=0.5)", samples, 0.5);
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

    test_variance_methods("Normal Distribution (μ=5, σ=2)", samples, 0.1);
  }

  // Test 3: Data with extreme outliers
  {
    auto data_range = utils::make_unif_range<double>(300, 0.0, 1.0, 456);
    std::vector<double> samples;
    samples.reserve(300);
    for (auto x : data_range) {
      samples.push_back(x);
    }

    // Add extreme outliers
    samples[100] = 1000.0;
    samples[200] = -500.0;

    test_variance_methods("Data with Extreme Outliers", samples, 0.2);
  }

  // Test 4: Non-stationary data
  {
    std::vector<double> samples;
    samples.reserve(400);

    for (size_t i = 0; i < 400; ++i) {
      // Mean and variance both change over time
      double mean = 5.0 * std::sin(2.0 * M_PI * i / 100.0);
      double variance = 1.0 + 2.0 * i / 400.0;

      std::mt19937 gen(789 + static_cast<unsigned>(i));
      std::normal_distribution<double> dist(mean, std::sqrt(variance));

      samples.push_back(dist(gen));
    }

    test_variance_methods("Non-stationary Data (changing mean & variance)", samples, 0.15);
  }

  // Test 5: High-frequency updates
  {
    test_variance_methods("High-frequency Updates (α=0.9)", {1.0, 2.0, 1.5, 3.0, 2.5, 1.0, 4.0, 2.0, 3.5, 1.5}, 0.9);
  }

  std::cout << "\n🎉 All tests passed! Both variance calculation methods are numerically equivalent.\n";

  return 0;
}
