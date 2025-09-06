#include <benchmark/benchmark.h>

#include <deque>
#include <iostream>
#include <random>
#include <vector>

struct deque_impl {
  std::deque<double> deq;

  deque_impl(size_t window) {
    // Constructor takes window size for consistency, though deque doesn't need pre-allocation
    (void)window; // Suppress unused parameter warning
  }

  void on_data(double val) {
    while (!deq.empty() && deq.back() > val) {
      deq.pop_back();
    }
    deq.push_back(val);
  }

  void on_evict(double val) {
    // window is never empty
    if (deq.front() == val)
      deq.pop_front();
  }

  double value() const { return deq.front(); }
};

struct vector_impl {
  std::vector<double> vec;
  size_t start_idx; // Index of the first valid element

  vector_impl(size_t window) : start_idx(0) { vec.reserve(window); }

  void on_data(double val) {
    // Remove elements from back that are greater than the new value
    while (vec.size() > start_idx && vec.back() > val) {
      vec.pop_back();
    }

    vec.push_back(val);

    if (vec.capacity() == vec.size()) {
      vec.erase(vec.begin(), vec.begin() + static_cast<ptrdiff_t>(start_idx));
      start_idx = 0;
    }
  }

  void on_evict(double val) {
    // If the minimum element is being evicted, advance start_idx
    if (vec[start_idx] == val)
      ++start_idx;
  }

  double value() const { return vec[start_idx]; }
};

// Generate random data for benchmarking
std::vector<double> generate_random_data(size_t count) {
  std::vector<double> data;
  data.reserve(count);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<double> dist(0.0, 1000.0);

  for (size_t i = 0; i < count; ++i) {
    data.push_back(dist(gen));
  }

  return data;
}

// Correctness verification function
bool verify_correctness() {
  const size_t data_size = 1000; // Smaller test first
  const size_t window_size = 10;

  auto data = generate_random_data(data_size);

  deque_impl deque_rolling_min(window_size);
  vector_impl vector_rolling_min(window_size);
  std::deque<double> window;

  for (size_t i = 0; i < data_size; ++i) {
    double val = data[i];

    // Add new value to both implementations
    deque_rolling_min.on_data(val);
    vector_rolling_min.on_data(val);
    window.push_back(val);

    // Maintain window size
    if (window.size() > window_size) {
      double evicted = window.front();
      window.pop_front();
      deque_rolling_min.on_evict(evicted);
      vector_rolling_min.on_evict(evicted);
    }

    // Compare outputs after window is established
    if (window.size() >= window_size) {
      double deque_result = deque_rolling_min.value();
      double vector_result = vector_rolling_min.value();

      if (deque_result != vector_result) {
        std::cerr << "Correctness check failed at iteration " << i << ": deque=" << deque_result
                  << ", vector=" << vector_result << std::endl;
        std::cerr << "Current window: ";
        for (size_t j = 0; j < window.size(); ++j) {
          std::cerr << window[j] << " ";
        }
        std::cerr << std::endl;
        std::cerr << "Last value added: " << val << std::endl;
        return false;
      }
    }
  }

  std::cout << "Correctness check passed: both implementations produce identical results" << std::endl;
  return true;
}

// Benchmark deque implementation
static void BM_RollingMin_Deque(benchmark::State &state) {
  const size_t data_size = 1000000; // 1 million
  const size_t window_size = 100;   // 100 scale window

  // Preallocate random data
  auto data = generate_random_data(data_size);

  for (auto _ : state) {
    deque_impl rolling_min(window_size);
    std::deque<double> window;

    for (size_t i = 0; i < data_size; ++i) {
      double val = data[i];

      // Add new value
      rolling_min.on_data(val);
      window.push_back(val);

      // Maintain window size
      if (window.size() > window_size) {
        double evicted = window.front();
        window.pop_front();
        rolling_min.on_evict(evicted);
      }

      // Get current minimum (prevents optimization)
      benchmark::DoNotOptimize(rolling_min.value());
    }
  }
}

// Benchmark vector implementation
static void BM_RollingMin_Vector(benchmark::State &state) {
  const size_t data_size = 1000000; // 1 million
  const size_t window_size = 100;   // 100 scale window

  // Preallocate random data
  auto data = generate_random_data(data_size);

  for (auto _ : state) {
    vector_impl rolling_min(window_size);
    std::deque<double> window;

    for (size_t i = 0; i < data_size; ++i) {
      double val = data[i];

      // Add new value
      rolling_min.on_data(val);
      window.push_back(val);

      // Maintain window size
      if (window.size() > window_size) {
        double evicted = window.front();
        window.pop_front();
        rolling_min.on_evict(evicted);
      }

      // Get current minimum (prevents optimization)
      benchmark::DoNotOptimize(rolling_min.value());
    }
  }
}

// Register benchmarks
BENCHMARK(BM_RollingMin_Deque);
BENCHMARK(BM_RollingMin_Vector);

// Benchmark with different window sizes
static void BM_RollingMin_Deque_WindowSize(benchmark::State &state) {
  const size_t data_size = 1000000;
  const size_t window_size = static_cast<size_t>(state.range(0));

  auto data = generate_random_data(data_size);

  for (auto _ : state) {
    deque_impl rolling_min(window_size);
    std::deque<double> window;

    for (size_t i = 0; i < data_size; ++i) {
      double val = data[i];

      rolling_min.on_data(val);
      window.push_back(val);

      if (window.size() > window_size) {
        double evicted = window.front();
        window.pop_front();
        rolling_min.on_evict(evicted);
      }

      benchmark::DoNotOptimize(rolling_min.value());
    }
  }
}

static void BM_RollingMin_Vector_WindowSize(benchmark::State &state) {
  const size_t data_size = 1000000;
  const size_t window_size = static_cast<size_t>(state.range(0));

  auto data = generate_random_data(data_size);

  for (auto _ : state) {
    vector_impl rolling_min(window_size);
    std::deque<double> window;

    for (size_t i = 0; i < data_size; ++i) {
      double val = data[i];

      rolling_min.on_data(val);
      window.push_back(val);

      if (window.size() > window_size) {
        double evicted = window.front();
        window.pop_front();
        rolling_min.on_evict(evicted);
      }

      benchmark::DoNotOptimize(rolling_min.value());
    }
  }
}

// Register parametrized benchmarks with different window sizes
BENCHMARK(BM_RollingMin_Deque_WindowSize)->Arg(50)->Arg(100)->Arg(200)->Arg(500);
BENCHMARK(BM_RollingMin_Vector_WindowSize)->Arg(50)->Arg(100)->Arg(200)->Arg(500);

int main(int argc, char **argv) {
  // Run correctness verification first
  if (!verify_correctness()) {
    std::cerr << "Correctness verification failed. Aborting benchmarks." << std::endl;
    return 1;
  }

  // Run benchmarks
  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
}

/*
Run on (12 X 24 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB
  L1 Instruction 128 KiB
  L2 Unified 4096 KiB (x12)
Load Average: 1.29, 1.49, 1.54
------------------------------------------------------------------------------
Benchmark                                    Time             CPU   Iterations
------------------------------------------------------------------------------
BM_RollingMin_Deque                   10500076 ns     10499348 ns           66
BM_RollingMin_Vector                   7920188 ns      7919322 ns           87
BM_RollingMin_Deque_WindowSize/50     10592672 ns     10591939 ns           66
BM_RollingMin_Deque_WindowSize/100    10622972 ns     10620076 ns           66
BM_RollingMin_Deque_WindowSize/200    10523810 ns     10522821 ns           67
BM_RollingMin_Deque_WindowSize/500    10579913 ns     10578864 ns           66
BM_RollingMin_Vector_WindowSize/50     8629129 ns      8628738 ns           80
BM_RollingMin_Vector_WindowSize/100    8625932 ns      8625062 ns           81
BM_RollingMin_Vector_WindowSize/200    8656169 ns      8655914 ns           81
BM_RollingMin_Vector_WindowSize/500    8654010 ns      8652790 ns           81
*/
