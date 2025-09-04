#include <benchmark/benchmark.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

template <typename T>
struct fn_base {
  using data_type = T;

  virtual void on_data(data_type in) noexcept = 0;
  virtual void value(data_type *out) const noexcept = 0;

  virtual ~fn_base() = default;
};

template <typename T>
struct fn_trait {
  using data_type = T;
  struct vtable_type {
    void (*on_data)(void *node, data_type in) noexcept;
    void (*value)(void const *node, data_type *out) noexcept;
    void (*destroy)(void *node) noexcept;
  };

  template <typename Deriv>
  struct vtable_for {
    static void on_data(void *node, data_type in) noexcept { static_cast<Deriv *>(node)->on_data(in); }
    static void value(void const *node, data_type *out) noexcept { static_cast<Deriv const *>(node)->value(out); }
    static void destroy(void *node) noexcept { static_cast<Deriv *>(node)->~Deriv(); }

    static constexpr vtable_type vtable{.on_data = &on_data, .value = &value, .destroy = &destroy};
  };

  void *node;
  vtable_type vtable;

  template <typename Deriv>
  fn_trait(Deriv *d) : node(d), vtable(vtable_for<Deriv>::vtable) {}

  // do not copy
  fn_trait(fn_trait const &) = delete;
  fn_trait &operator=(fn_trait const &) = delete;

  // allow move
  fn_trait(fn_trait &&other) noexcept : node(other.node), vtable(other.vtable) {
    other.node = nullptr; // prevent double destruction
  }

  fn_trait &operator=(fn_trait &&other) noexcept {
    if (this != &other) {
      if (node) {
        vtable.destroy(node);
      }
      node = other.node;
      vtable = other.vtable;
      other.node = nullptr;
    }
    return *this;
  }

  void on_data(data_type in) noexcept { vtable.on_data(node, in); }
  void value(data_type *out) const noexcept { vtable.value(node, out); }
  ~fn_trait() noexcept {
    if (node) {
      vtable.destroy(node);
    }
  }
};

struct count : public fn_base<size_t> {
  using base = fn_base<size_t>;
  using typename base::data_type;

  size_t total = 0;

  void on_data(data_type) noexcept override { ++total; }
  void value(data_type *out) const noexcept override { *out = total; }
};

template <size_t N>
struct sum_gen_alot_instance : public fn_base<size_t> {
  using base = fn_base<size_t>;
  using typename base::data_type;

  size_t total = 0;

  void on_data(data_type in) noexcept override { total += in / N; }
  void value(data_type *out) const noexcept override { *out = total; }
};

std::unique_ptr<fn_base<size_t>> make_fn(size_t nn) {
  switch (nn % 20) {
  case 0:
    return std::make_unique<count>();
  case 1:
    return std::make_unique<sum_gen_alot_instance<1>>();
  case 2:
    return std::make_unique<sum_gen_alot_instance<2>>();
  case 3:
    return std::make_unique<sum_gen_alot_instance<3>>();
  case 4:
    return std::make_unique<sum_gen_alot_instance<4>>();
  case 5:
    return std::make_unique<sum_gen_alot_instance<5>>();
  case 6:
    return std::make_unique<sum_gen_alot_instance<6>>();
  case 7:
    return std::make_unique<sum_gen_alot_instance<7>>();
  case 8:
    return std::make_unique<sum_gen_alot_instance<8>>();
  case 9:
    return std::make_unique<sum_gen_alot_instance<9>>();
  case 10:
    return std::make_unique<sum_gen_alot_instance<10>>();
  case 11:
    return std::make_unique<sum_gen_alot_instance<11>>();
  case 12:
    return std::make_unique<sum_gen_alot_instance<12>>();
  case 13:
    return std::make_unique<sum_gen_alot_instance<13>>();
  case 14:
    return std::make_unique<sum_gen_alot_instance<14>>();
  case 15:
    return std::make_unique<sum_gen_alot_instance<15>>();
  case 16:
    return std::make_unique<sum_gen_alot_instance<16>>();
  case 17:
    return std::make_unique<sum_gen_alot_instance<17>>();
  case 18:
    return std::make_unique<sum_gen_alot_instance<18>>();
  case 19:
    return std::make_unique<sum_gen_alot_instance<19>>();
  default:
    return nullptr;
  }
}

fn_trait<size_t> make_fn_trait(size_t nn) {
  switch (nn % 20) {
  case 0:
    return fn_trait<size_t>(new count());
  case 1:
    return fn_trait<size_t>(new sum_gen_alot_instance<1>());
  case 2:
    return fn_trait<size_t>(new sum_gen_alot_instance<2>());
  case 3:
    return fn_trait<size_t>(new sum_gen_alot_instance<3>());
  case 4:
    return fn_trait<size_t>(new sum_gen_alot_instance<4>());
  case 5:
    return fn_trait<size_t>(new sum_gen_alot_instance<5>());
  case 6:
    return fn_trait<size_t>(new sum_gen_alot_instance<6>());
  case 7:
    return fn_trait<size_t>(new sum_gen_alot_instance<7>());
  case 8:
    return fn_trait<size_t>(new sum_gen_alot_instance<8>());
  case 9:
    return fn_trait<size_t>(new sum_gen_alot_instance<9>());
  case 10:
    return fn_trait<size_t>(new sum_gen_alot_instance<10>());
  case 11:
    return fn_trait<size_t>(new sum_gen_alot_instance<11>());
  case 12:
    return fn_trait<size_t>(new sum_gen_alot_instance<12>());
  case 13:
    return fn_trait<size_t>(new sum_gen_alot_instance<13>());
  case 14:
    return fn_trait<size_t>(new sum_gen_alot_instance<14>());
  case 15:
    return fn_trait<size_t>(new sum_gen_alot_instance<15>());
  case 16:
    return fn_trait<size_t>(new sum_gen_alot_instance<16>());
  case 17:
    return fn_trait<size_t>(new sum_gen_alot_instance<17>());
  case 18:
    return fn_trait<size_t>(new sum_gen_alot_instance<18>());
  case 19:
    return fn_trait<size_t>(new sum_gen_alot_instance<19>());
  default:
    throw std::invalid_argument("unreachable");
  }
}

// Benchmark setup constants
constexpr size_t NUM_FUNCTIONS = 1000;
constexpr size_t NUM_DATA_POINTS = 100000;

// Global test data to avoid allocation overhead in benchmarks
std::vector<size_t> g_test_data;
std::vector<std::unique_ptr<fn_base<size_t>>> g_virtual_functions;
std::vector<fn_trait<size_t>> g_trait_functions;

// Initialize test data once
void setup_benchmark_data() {
  static bool initialized = false;
  if (initialized)
    return;

  // Generate random test data
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<size_t> dis(1, 1000);

  g_test_data.reserve(NUM_DATA_POINTS);
  for (size_t i = 0; i < NUM_DATA_POINTS; ++i) {
    g_test_data.push_back(dis(gen));
  }

  // Create virtual function instances
  g_virtual_functions.reserve(NUM_FUNCTIONS);
  for (size_t i = 0; i < NUM_FUNCTIONS; ++i) {
    g_virtual_functions.push_back(make_fn(i));
  }

  // Create trait function instances
  g_trait_functions.reserve(NUM_FUNCTIONS);
  for (size_t i = 0; i < NUM_FUNCTIONS; ++i) {
    g_trait_functions.push_back(make_fn_trait(i));
  }

  initialized = true;
}

// Benchmark virtual function calls
static void BM_VirtualCalls(benchmark::State &state) {
  setup_benchmark_data();

  for (auto _ : state) {
    for (size_t i = 0; i < NUM_FUNCTIONS; ++i) {
      auto &func = g_virtual_functions[i];
      size_t data_idx = i % NUM_DATA_POINTS;

      // Call on_data
      func->on_data(g_test_data[data_idx]);

      // Call value
      size_t result;
      func->value(&result);

      // Prevent optimization
      benchmark::DoNotOptimize(result);
    }
  }
}

// Benchmark trait function calls
static void BM_TraitCalls(benchmark::State &state) {
  setup_benchmark_data();

  for (auto _ : state) {
    for (size_t i = 0; i < NUM_FUNCTIONS; ++i) {
      auto &func = g_trait_functions[i];
      size_t data_idx = i % NUM_DATA_POINTS;

      // Call on_data
      func.on_data(g_test_data[data_idx]);

      // Call value
      size_t result;
      func.value(&result);

      // Prevent optimization
      benchmark::DoNotOptimize(result);
    }
  }
}

// Benchmark virtual function calls with shuffled access pattern
static void BM_VirtualCallsShuffled(benchmark::State &state) {
  setup_benchmark_data();

  // Create shuffled indices
  std::vector<size_t> indices(NUM_FUNCTIONS);
  std::iota(indices.begin(), indices.end(), 0);
  std::shuffle(indices.begin(), indices.end(), std::mt19937{42});

  for (auto _ : state) {
    for (size_t i = 0; i < NUM_FUNCTIONS; ++i) {
      size_t func_idx = indices[i];
      auto &func = g_virtual_functions[func_idx];
      size_t data_idx = i % NUM_DATA_POINTS;

      // Call on_data
      func->on_data(g_test_data[data_idx]);

      // Call value
      size_t result;
      func->value(&result);

      // Prevent optimization
      benchmark::DoNotOptimize(result);
    }
  }
}

// Benchmark trait function calls with shuffled access pattern
static void BM_TraitCallsShuffled(benchmark::State &state) {
  setup_benchmark_data();

  // Create shuffled indices
  std::vector<size_t> indices(NUM_FUNCTIONS);
  std::iota(indices.begin(), indices.end(), 0);
  std::shuffle(indices.begin(), indices.end(), std::mt19937{42});

  for (auto _ : state) {
    for (size_t i = 0; i < NUM_FUNCTIONS; ++i) {
      size_t func_idx = indices[i];
      auto &func = g_trait_functions[func_idx];
      size_t data_idx = i % NUM_DATA_POINTS;

      // Call on_data
      func.on_data(g_test_data[data_idx]);

      // Call value
      size_t result;
      func.value(&result);

      // Prevent optimization
      benchmark::DoNotOptimize(result);
    }
  }
}

// Register benchmarks
BENCHMARK(BM_VirtualCalls)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_TraitCalls)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_VirtualCallsShuffled)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_TraitCallsShuffled)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();

/*

Running /Users/junda/works/opflow/build-vscode/bench/bench_trait
Run on (12 X 24 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB
  L1 Instruction 128 KiB
  L2 Unified 4096 KiB (x12)
Load Average: 6.45, 3.46, 2.96
------------------------------------------------------------------
Benchmark                        Time             CPU   Iterations
------------------------------------------------------------------
BM_VirtualCalls               3.48 us         3.46 us       206435
BM_TraitCalls                 3.99 us         3.99 us       176871
BM_VirtualCallsShuffled       3.46 us         3.46 us       198533
BM_TraitCallsShuffled         3.94 us         3.94 us       178678

*/
