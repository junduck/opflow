#include <benchmark/benchmark.h>
#include <memory>
#include <random>
#include <vector>

// Simulate more realistic DAG scenario
template <typename T>
struct complex_op_base {
  using data_type = T;

  virtual void on_data(data_type const *in, size_t in_size) noexcept = 0;
  virtual void value(data_type *out, size_t out_size) const noexcept = 0;
  // virtual void on_evict(data_type const *rm, size_t rm_size) noexcept = 0;
  //  virtual size_t num_inputs() const noexcept = 0;
  //  virtual size_t num_outputs() const noexcept = 0;
  //  virtual bool is_cumulative() const noexcept = 0;
  virtual ~complex_op_base() = default;
};

template <typename T>
struct complex_op_trait {
  using data_type = T;

  struct vtable_type {
    void (*on_data)(void *node, data_type const *in, size_t in_size) noexcept;
    void (*value)(void const *node, data_type *out, size_t out_size) noexcept;
    // void (*on_evict)(void *node, data_type const *rm, size_t rm_size) noexcept;
    //  size_t (*num_inputs)(void const *node) noexcept;
    //  size_t (*num_outputs)(void const *node) noexcept;
    //  bool (*is_cumulative)(void const *node) noexcept;
    void (*destroy)(void *node) noexcept;
  };

  template <typename Deriv>
  struct vtable_for {
    static void on_data(void *node, data_type const *in, size_t in_size) noexcept {
      static_cast<Deriv *>(node)->on_data(in, in_size);
    }
    static void value(void const *node, data_type *out, size_t out_size) noexcept {
      static_cast<Deriv const *>(node)->value(out, out_size);
    }
    // static void on_evict(void *node, data_type const *rm, size_t rm_size) noexcept {
    //   static_cast<Deriv *>(node)->on_evict(rm, rm_size);
    // }
    //  static size_t num_inputs(void const *node) noexcept { return static_cast<Deriv const *>(node)->num_inputs(); }
    //  static size_t num_outputs(void const *node) noexcept { return static_cast<Deriv const *>(node)->num_outputs(); }
    //  static bool is_cumulative(void const *node) noexcept { return static_cast<Deriv const *>(node)->is_cumulative();
    //  }
    static void destroy(void *node) noexcept { delete static_cast<Deriv *>(node); }

    static constexpr vtable_type vtable{&on_data, &value,
                                        // &on_evict, &num_inputs, &num_outputs, &is_cumulative,
                                        &destroy};
  };

  void *node;
  vtable_type const *vtable;

  template <typename Deriv>
  complex_op_trait(Deriv *d) : node(d), vtable(&vtable_for<Deriv>::vtable) {}

  // Move semantics
  complex_op_trait(complex_op_trait &&other) noexcept : node(other.node), vtable(other.vtable) { other.node = nullptr; }

  ~complex_op_trait() {
    if (node && vtable) {
      vtable->destroy(node);
    }
  }

  void on_data(data_type const *in, size_t in_size) noexcept { vtable->on_data(node, in, in_size); }
  void value(data_type *out, size_t out_size) const noexcept { vtable->value(node, out, out_size); }
  // void on_evict(data_type const *rm, size_t rm_size) noexcept { vtable->on_evict(node, rm, rm_size); }
  // size_t num_inputs() const noexcept { return vtable->num_inputs(node); }
  // size_t num_outputs() const noexcept { return vtable->num_outputs(node); }
  // bool is_cumulative() const noexcept { return vtable->is_cumulative(node); }
};

// Create many different operator types to stress vtable cache
#define DECLARE_OP(Name, Inputs, Outputs, Cumulative)                                                                  \
  struct Name : public complex_op_base<double> {                                                                       \
    std::vector<double> state;                                                                                         \
    Name() : state(Inputs + Outputs) {}                                                                                \
    void on_data(data_type const *in, size_t) noexcept override {                                                      \
      for (size_t i = 0; i < Inputs; ++i)                                                                              \
        state[i] += in[i];                                                                                             \
    }                                                                                                                  \
    void value(data_type *out, size_t) const noexcept override {                                                       \
      for (size_t i = 0; i < Outputs; ++i)                                                                             \
        out[i] = state[i % state.size()];                                                                              \
    }                                                                                                                  \
  };

/*
    void on_evict(data_type const *rm, size_t) noexcept override {                                                     \
      for (size_t i = 0; i < Inputs; ++i)                                                                              \
        state[i] -= rm[i];                                                                                             \
    }                                                                                                                  \
size_t num_inputs() const noexcept override { return Inputs; }
size_t num_outputs() const noexcept override { return Outputs; }
bool is_cumulative() const noexcept override { return Cumulative; }
*/

// Generate 50 different operator types
DECLARE_OP(Op01, 1, 1, true)
DECLARE_OP(Op02, 2, 1, false)
DECLARE_OP(Op03, 1, 2, true)
DECLARE_OP(Op04, 3, 1, false)
DECLARE_OP(Op05, 1, 3, true)
DECLARE_OP(Op06, 2, 2, false)
DECLARE_OP(Op07, 4, 1, true)
DECLARE_OP(Op08, 1, 4, false)
DECLARE_OP(Op09, 3, 2, true)
DECLARE_OP(Op10, 2, 3, false)
DECLARE_OP(Op11, 5, 1, true)
DECLARE_OP(Op12, 1, 5, false)
DECLARE_OP(Op13, 4, 2, true)
DECLARE_OP(Op14, 2, 4, false)
DECLARE_OP(Op15, 3, 3, true)
DECLARE_OP(Op16, 6, 1, false)
DECLARE_OP(Op17, 1, 6, true)
DECLARE_OP(Op18, 5, 2, false)
DECLARE_OP(Op19, 2, 5, true)
DECLARE_OP(Op20, 4, 3, false)
DECLARE_OP(Op21, 3, 4, true)
DECLARE_OP(Op22, 7, 1, false)
DECLARE_OP(Op23, 1, 7, true)
DECLARE_OP(Op24, 6, 2, false)
DECLARE_OP(Op25, 2, 6, true)
DECLARE_OP(Op26, 5, 3, false)
DECLARE_OP(Op27, 3, 5, true)
DECLARE_OP(Op28, 4, 4, false)
DECLARE_OP(Op29, 8, 1, true)
DECLARE_OP(Op30, 1, 8, false)
DECLARE_OP(Op31, 7, 2, true)
DECLARE_OP(Op32, 2, 7, false)
DECLARE_OP(Op33, 6, 3, true)
DECLARE_OP(Op34, 3, 6, false)
DECLARE_OP(Op35, 5, 4, true)
DECLARE_OP(Op36, 4, 5, false)
DECLARE_OP(Op37, 9, 1, true)
DECLARE_OP(Op38, 1, 9, false)
DECLARE_OP(Op39, 8, 2, true)
DECLARE_OP(Op40, 2, 8, false)
DECLARE_OP(Op41, 7, 3, true)
DECLARE_OP(Op42, 3, 7, false)
DECLARE_OP(Op43, 6, 4, true)
DECLARE_OP(Op44, 4, 6, false)
DECLARE_OP(Op45, 5, 5, true)
DECLARE_OP(Op46, 10, 1, false)
DECLARE_OP(Op47, 1, 10, true)
DECLARE_OP(Op48, 9, 2, false)
DECLARE_OP(Op49, 2, 9, true)
DECLARE_OP(Op50, 8, 3, false)

// Factory functions
std::unique_ptr<complex_op_base<double>> make_virtual_op(size_t type) {
  switch (type % 50) {
  case 0:
    return std::make_unique<Op01>();
  case 1:
    return std::make_unique<Op02>();
  case 2:
    return std::make_unique<Op03>();
  case 3:
    return std::make_unique<Op04>();
  case 4:
    return std::make_unique<Op05>();
  case 5:
    return std::make_unique<Op06>();
  case 6:
    return std::make_unique<Op07>();
  case 7:
    return std::make_unique<Op08>();
  case 8:
    return std::make_unique<Op09>();
  case 9:
    return std::make_unique<Op10>();
  case 10:
    return std::make_unique<Op11>();
  case 11:
    return std::make_unique<Op12>();
  case 12:
    return std::make_unique<Op13>();
  case 13:
    return std::make_unique<Op14>();
  case 14:
    return std::make_unique<Op15>();
  case 15:
    return std::make_unique<Op16>();
  case 16:
    return std::make_unique<Op17>();
  case 17:
    return std::make_unique<Op18>();
  case 18:
    return std::make_unique<Op19>();
  case 19:
    return std::make_unique<Op20>();
  case 20:
    return std::make_unique<Op21>();
  case 21:
    return std::make_unique<Op22>();
  case 22:
    return std::make_unique<Op23>();
  case 23:
    return std::make_unique<Op24>();
  case 24:
    return std::make_unique<Op25>();
  case 25:
    return std::make_unique<Op26>();
  case 26:
    return std::make_unique<Op27>();
  case 27:
    return std::make_unique<Op28>();
  case 28:
    return std::make_unique<Op29>();
  case 29:
    return std::make_unique<Op30>();
  case 30:
    return std::make_unique<Op31>();
  case 31:
    return std::make_unique<Op32>();
  case 32:
    return std::make_unique<Op33>();
  case 33:
    return std::make_unique<Op34>();
  case 34:
    return std::make_unique<Op35>();
  case 35:
    return std::make_unique<Op36>();
  case 36:
    return std::make_unique<Op37>();
  case 37:
    return std::make_unique<Op38>();
  case 38:
    return std::make_unique<Op39>();
  case 39:
    return std::make_unique<Op40>();
  case 40:
    return std::make_unique<Op41>();
  case 41:
    return std::make_unique<Op42>();
  case 42:
    return std::make_unique<Op43>();
  case 43:
    return std::make_unique<Op44>();
  case 44:
    return std::make_unique<Op45>();
  case 45:
    return std::make_unique<Op46>();
  case 46:
    return std::make_unique<Op47>();
  case 47:
    return std::make_unique<Op48>();
  case 48:
    return std::make_unique<Op49>();
  case 49:
    return std::make_unique<Op50>();
  default:
    return std::make_unique<Op01>();
  }
}

complex_op_trait<double> make_trait_op(size_t type) {
  switch (type % 50) {
  case 0:
    return complex_op_trait<double>(new Op01());
  case 1:
    return complex_op_trait<double>(new Op02());
  case 2:
    return complex_op_trait<double>(new Op03());
  case 3:
    return complex_op_trait<double>(new Op04());
  case 4:
    return complex_op_trait<double>(new Op05());
  case 5:
    return complex_op_trait<double>(new Op06());
  case 6:
    return complex_op_trait<double>(new Op07());
  case 7:
    return complex_op_trait<double>(new Op08());
  case 8:
    return complex_op_trait<double>(new Op09());
  case 9:
    return complex_op_trait<double>(new Op10());
  case 10:
    return complex_op_trait<double>(new Op11());
  case 11:
    return complex_op_trait<double>(new Op12());
  case 12:
    return complex_op_trait<double>(new Op13());
  case 13:
    return complex_op_trait<double>(new Op14());
  case 14:
    return complex_op_trait<double>(new Op15());
  case 15:
    return complex_op_trait<double>(new Op16());
  case 16:
    return complex_op_trait<double>(new Op17());
  case 17:
    return complex_op_trait<double>(new Op18());
  case 18:
    return complex_op_trait<double>(new Op19());
  case 19:
    return complex_op_trait<double>(new Op20());
  case 20:
    return complex_op_trait<double>(new Op21());
  case 21:
    return complex_op_trait<double>(new Op22());
  case 22:
    return complex_op_trait<double>(new Op23());
  case 23:
    return complex_op_trait<double>(new Op24());
  case 24:
    return complex_op_trait<double>(new Op25());
  case 25:
    return complex_op_trait<double>(new Op26());
  case 26:
    return complex_op_trait<double>(new Op27());
  case 27:
    return complex_op_trait<double>(new Op28());
  case 28:
    return complex_op_trait<double>(new Op29());
  case 29:
    return complex_op_trait<double>(new Op30());
  case 30:
    return complex_op_trait<double>(new Op31());
  case 31:
    return complex_op_trait<double>(new Op32());
  case 32:
    return complex_op_trait<double>(new Op33());
  case 33:
    return complex_op_trait<double>(new Op34());
  case 34:
    return complex_op_trait<double>(new Op35());
  case 35:
    return complex_op_trait<double>(new Op36());
  case 36:
    return complex_op_trait<double>(new Op37());
  case 37:
    return complex_op_trait<double>(new Op38());
  case 38:
    return complex_op_trait<double>(new Op39());
  case 39:
    return complex_op_trait<double>(new Op40());
  case 40:
    return complex_op_trait<double>(new Op41());
  case 41:
    return complex_op_trait<double>(new Op42());
  case 42:
    return complex_op_trait<double>(new Op43());
  case 43:
    return complex_op_trait<double>(new Op44());
  case 44:
    return complex_op_trait<double>(new Op45());
  case 45:
    return complex_op_trait<double>(new Op46());
  case 46:
    return complex_op_trait<double>(new Op47());
  case 47:
    return complex_op_trait<double>(new Op48());
  case 48:
    return complex_op_trait<double>(new Op49());
  case 49:
    return complex_op_trait<double>(new Op50());
  default:
    return complex_op_trait<double>(new Op01());
  }
}

// Simulate DAG execution pattern
constexpr size_t DAG_SIZE = 100;
constexpr size_t NUM_ITERATIONS = 10000;

static void BM_DAG_Virtual_Complex(benchmark::State &state) {
  std::vector<std::unique_ptr<complex_op_base<double>>> ops;
  std::vector<std::vector<double>> inputs(DAG_SIZE);
  std::vector<std::vector<double>> outputs(DAG_SIZE);

  // Create random DAG topology
  std::mt19937 rng{42};
  for (size_t i = 0; i < DAG_SIZE; ++i) {
    ops.push_back(make_virtual_op(rng()));
    inputs[i].resize(10, 1.0 + i);
    outputs[i].resize(10);
    // inputs[i].resize(ops[i]->num_inputs(), 1.0 + i);
    // outputs[i].resize(ops[i]->num_outputs());
  }

  for (auto _ : state) {
    for (size_t iter = 0; iter < NUM_ITERATIONS; ++iter) {
      for (size_t i = 0; i < DAG_SIZE; ++i) {
        // Simulate DAG execution complexity
        // if (!ops[i]->is_cumulative() && iter > 10) {
        // ops[i]->on_evict(inputs[i].data(), inputs[i].size());
        //}
        ops[i]->on_data(inputs[i].data(), inputs[i].size());
        ops[i]->value(outputs[i].data(), outputs[i].size());

        // Update inputs for next iteration (simulate data flow)
        for (auto &val : inputs[i]) {
          val += 0.1;
        }
      }
    }
  }
}

static void BM_DAG_Trait_Complex(benchmark::State &state) {
  std::vector<complex_op_trait<double>> ops;
  std::vector<std::vector<double>> inputs(DAG_SIZE);
  std::vector<std::vector<double>> outputs(DAG_SIZE);

  // Create random DAG topology
  std::mt19937 rng{42};
  for (size_t i = 0; i < DAG_SIZE; ++i) {
    ops.push_back(make_trait_op(rng()));
    inputs[i].resize(10, 1.0 + i);
    outputs[i].resize(10);
    // inputs[i].resize(ops[i].num_inputs(), 1.0 + i);
    // outputs[i].resize(ops[i].num_outputs());
  }

  for (auto _ : state) {
    for (size_t iter = 0; iter < NUM_ITERATIONS; ++iter) {
      for (size_t i = 0; i < DAG_SIZE; ++i) {
        // Simulate DAG execution complexity
        // if (!ops[i].is_cumulative() && iter > 10) {
        // ops[i].on_evict(inputs[i].data(), inputs[i].size());
        //}
        ops[i].on_data(inputs[i].data(), inputs[i].size());
        ops[i].value(outputs[i].data(), outputs[i].size());

        // Update inputs for next iteration (simulate data flow)
        for (auto &val : inputs[i]) {
          val += 0.1;
        }
      }
    }
  }
}

BENCHMARK(BM_DAG_Virtual_Complex)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_DAG_Trait_Complex)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();

/*
Run on (12 X 24 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB
  L1 Instruction 128 KiB
  L2 Unified 4096 KiB (x12)
Load Average: 4.71, 4.39, 3.69
-----------------------------------------------------------------
Benchmark                       Time             CPU   Iterations
-----------------------------------------------------------------
BM_DAG_Virtual_Complex    7874355 ns      7869789 ns           90
BM_DAG_Trait_Complex      9472180 ns      9113688 ns           80
*/

/*
Reduced method

Run on (12 X 24 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB
  L1 Instruction 128 KiB
  L2 Unified 4096 KiB (x12)
Load Average: 3.64, 3.94, 3.91
-----------------------------------------------------------------
Benchmark                       Time             CPU   Iterations
-----------------------------------------------------------------
BM_DAG_Virtual_Complex       5.51 ms         5.48 ms         2558
BM_DAG_Trait_Complex         5.85 ms         5.85 ms         2398
*/
