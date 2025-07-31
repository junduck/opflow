#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include <benchmark/benchmark.h>

// inlined vtable

struct vtable {
  void (*fn1)(void *self, int arg1, int arg2) noexcept;
  void (*fn2)(void *self, double arg) noexcept;
};

template <typename Impl>
struct vtable_impl {
  static void fn1(void *self, int arg1, int arg2) noexcept { static_cast<Impl *>(self)->fn1(arg1, arg2); }
  static void fn2(void *self, double arg) noexcept { static_cast<Impl *>(self)->fn2(arg); }
};

template <typename Impl>
constexpr inline vtable vtable_of{vtable_impl<Impl>::fn1, vtable_impl<Impl>::fn2};

struct type1 {
  double data2;
  int data1;

  void fn1(int arg1, int arg2) noexcept { data1 = arg1 + arg2; }
  void fn2(double arg) noexcept { data2 = arg + data1; }
};
static_assert(alignof(type1) == alignof(uint64_t), "type1 must be aligned to 8 bytes");

struct type2 {
  double data2;
  double data1;
  double data0;

  void fn1(int arg1, int arg2) noexcept { data1 = arg1 * arg2; }
  void fn2(double arg) noexcept {
    data2 = arg * data1;
    data0 += data2;
  }
};
static_assert(alignof(type2) == alignof(uint64_t), "type2 must be aligned to 8 bytes");

struct inlined_trait {
  // memory layout: | vptr1 | storage1 | vptr2 | storage2 | ...
  std::vector<uint64_t> data;
  std::vector<size_t> offset;

  template <typename T, typename... Args>
  void emplace_back(Args &&...args) {
    size_t storage_slot = sizeof(T) / sizeof(uint64_t) + 1 + 1; // +1 for vptr
    offset.push_back(data.size());
    data.resize(data.size() + storage_slot);
    // UB but fine

    // 1. put vptr
    data[offset.back()] = reinterpret_cast<uint64_t>(&vtable_of<T>);
    // 2. put storage
    std::construct_at(reinterpret_cast<T *>(&data[offset.back() + 1]), std::forward<Args>(args)...);
  }

  void do_call_fn1(int arg1, int arg2) {
    for (size_t i = 0; i < offset.size(); ++i) {
      auto vptr = reinterpret_cast<vtable *>(data[offset[i]]);
      vptr->fn1(reinterpret_cast<void *>(&data[offset[i] + 1]), arg1, arg2);
    }
  }

  void do_call_fn2(double arg) {
    for (size_t i = 0; i < offset.size(); ++i) {
      auto vptr = reinterpret_cast<vtable *>(data[offset[i]]);
      vptr->fn2(reinterpret_cast<void *>(&data[offset[i] + 1]), arg);
    }
  }
};

// virtual base
struct base {
  virtual void fn1(int arg1, int arg2) noexcept = 0;
  virtual void fn2(double arg) noexcept = 0;
  virtual ~base() = default;
};

// concrete types
struct derived1 : base {
  double data2;
  int data1;
  void fn1(int arg1, int arg2) noexcept override { data1 = arg1 + arg2; }
  void fn2(double arg) noexcept override { data2 = arg + data1; }
};

struct derived2 : base {
  double data2;
  double data1;
  double data0;
  void fn1(int arg1, int arg2) noexcept override { data1 = arg1 * arg2; }
  void fn2(double arg) noexcept override {
    data2 = arg * data1;
    data0 += data2;
  }
};

// Benchmark: Inlined vtable approach
static void BM_InlinedTrait(benchmark::State &state) {
  constexpr long long num_nodes = 100000;
  inlined_trait container;

  // Prepare data - mix of type1 and type2
  std::mt19937 gen(42);
  std::uniform_int_distribution<> dist(0, 1);

  for (long long i = 0; i < num_nodes; ++i) {
    if (dist(gen) == 0) {
      container.emplace_back<type1>();
    } else {
      container.emplace_back<type2>();
    }
  }

  // Benchmark the calls
  std::uniform_int_distribution<> arg_dist(1, 100);
  std::uniform_real_distribution<double> double_dist(1.0, 100.0);

  for (auto _ : state) {
    int arg1 = arg_dist(gen);
    int arg2 = arg_dist(gen);
    double arg = double_dist(gen);

    container.do_call_fn1(arg1, arg2);
    container.do_call_fn2(arg);

    benchmark::DoNotOptimize(container);
  }

  state.SetItemsProcessed(state.iterations() * num_nodes * 2); // 2 function calls per iteration
}

// Benchmark: Traditional virtual inheritance
static void BM_VirtualInheritance(benchmark::State &state) {
  constexpr long long num_nodes = 100000;
  std::vector<std::unique_ptr<base>> container;

  // Prepare data - mix of derived1 and derived2
  std::mt19937 gen(42);
  std::uniform_int_distribution<> dist(0, 1);

  for (long long i = 0; i < num_nodes; ++i) {
    if (dist(gen) == 0) {
      container.emplace_back(std::make_unique<derived1>());
    } else {
      container.emplace_back(std::make_unique<derived2>());
    }
  }

  // Benchmark the calls
  std::uniform_int_distribution<> arg_dist(1, 100);
  std::uniform_real_distribution<double> double_dist(1.0, 100.0);

  for (auto _ : state) {
    int arg1 = arg_dist(gen);
    int arg2 = arg_dist(gen);
    double arg = double_dist(gen);

    for (auto &obj : container) {
      obj->fn1(arg1, arg2);
      obj->fn2(arg);
    }

    benchmark::DoNotOptimize(container);
  }

  state.SetItemsProcessed(state.iterations() * num_nodes * 2); // 2 function calls per iteration
}

// Register benchmarks
BENCHMARK(BM_InlinedTrait);
BENCHMARK(BM_VirtualInheritance);

// Benchmark main
BENCHMARK_MAIN();

/*
CPU Caches:
  L1 Data 64 KiB
  L1 Instruction 128 KiB
  L2 Unified 4096 KiB (x12)
Load Average: 2.95, 3.33, 3.88
--------------------------------------------------------------------------------
Benchmark                      Time             CPU   Iterations UserCounters...
--------------------------------------------------------------------------------
BM_InlinedTrait          1140743 ns      1138629 ns          615 items_per_second=175.65M/s
BM_VirtualInheritance     575486 ns       574932 ns         1233 items_per_second=347.867M/s
*/

/**
 * @brief Benchmark results for inlined trait vs virtual inheritance
 * @details The inlined vtable approach is significantly slower than the virtual inheritance approach.
 * I believe this will be the kind of slow down we will see when using std::function type erased operators.
 *
 * Conclusion: we should implement more utility operators so users dont need to wrap with std::function
 */
