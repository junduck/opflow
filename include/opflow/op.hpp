#pragma once

#include <algorithm>
#include <cassert>
#include <deque>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <stdexcept>
#include <vector>

namespace opflow {

template <typename T>
using time_delta_t = decltype(std::declval<T>() - std::declval<T>());

template <typename T>
concept scala_time = std::regular<T> && std::totally_ordered<T> && std::numeric_limits<T>::is_specialized &&
                     requires(T a, time_delta_t<T> b) {
                       { a - b } -> std::convertible_to<T>;
                       { a + b } -> std::convertible_to<T>;
                       { T{} } -> std::same_as<T>; // Ensure default construction
                     };

template <scala_time T>
struct op_base {

  // Accessing dependent data double *in[parent_id], double *rm[parent_id]
  virtual void step(T tick, double const *const *in) noexcept = 0;
  virtual void inverse(T tick, double const *const *rm) noexcept = 0;

  // Output directly written to buf, value should not change state of the op so engine can query it at any time
  virtual void value(double *out) const noexcept = 0;

  // Returns oldest tick this op needs to keep track of (i.e. engine will call inverse on expired data
  // boundary condition tick <= watermark
  virtual T watermark() const noexcept {
    return T{}; // Returning T{} means no watermark i.e. this is a cumulative op, no inverse is called upon it
  }

  virtual constexpr size_t num_depends() const noexcept = 0;
  virtual constexpr size_t num_outputs() const noexcept = 0;

  virtual ~op_base() = default;
};

template <scala_time T>
struct root_input : public op_base<T> {
  std::vector<double> mem;

  explicit root_input(size_t n) : mem(n, 0.0) {} // Initialize with zeros

  void step(T tick, double const *const *in) noexcept override {
    std::ignore = tick; // Unused in root_input
    if (in && in[0]) {  // Add null pointer check
      std::copy(in[0], in[0] + mem.size(), mem.data());
    }
  }
  void inverse(T, double const *const *) noexcept override {} // noop
  void value(double *out) const noexcept override {
    if (out) { // Add null pointer check
      std::copy(mem.begin(), mem.end(), out);
    }
  }

  constexpr size_t num_depends() const noexcept override { return 0; } // Root input has no dependencies
  size_t num_outputs() const noexcept override { return mem.size(); }
};

template <scala_time T>
struct rollsum : public op_base<T> {
  double sum;
  T last_tick;
  time_delta_t<T> window;
  std::vector<size_t> sum_idx; // Indices of inputs contributing to the sum

  template <std::ranges::input_range R>
  rollsum(time_delta_t<T> window, R &&idx) : sum(0.0), last_tick(T{}), window(window) {
    if (std::ranges::empty(idx))
      sum_idx = {0};
    else
      sum_idx = std::vector<size_t>(std::ranges::begin(idx), std::ranges::end(idx));
  }

  explicit rollsum(time_delta_t<T> window) : rollsum(window, std::span<size_t>{}) {}

  void step(T tick, double const *const *in) noexcept override {
    // Handle initialization case
    if (last_tick == T{}) {
      last_tick = tick;
    } else if (tick <= last_tick) {
      // TODO: this is a bug of engine, we should not handle any late/out-of-order ticks
      return;
    } else {
      last_tick = tick;
    }

    auto const *data = in[0];
    for (auto idx : sum_idx) {
      // Add bounds checking to prevent buffer overflow
      if (idx < static_cast<size_t>(std::numeric_limits<std::ptrdiff_t>::max())) {
        sum += data[idx];
      }
    }
  }

  void inverse(T tick, double const *const *rm) noexcept override {
    std::ignore = tick; // Unused in reverse
    auto const *data = rm[0];
    for (auto idx : sum_idx) {
      // Add bounds checking to prevent buffer underflow
      if (idx < static_cast<size_t>(std::numeric_limits<std::ptrdiff_t>::max())) {
        sum -= data[idx];
      }
    }
  }

  void value(double *out) const noexcept override { *out = sum; }

  T watermark() const noexcept override {
    if (last_tick == T{})
      return T{}; // No data processed yet
    if (window <= time_delta_t<T>{0})
      return T{}; // Invalid or infinite window
    // Proper watermark calculation: last_tick - window, but handle underflow
    auto delta = static_cast<time_delta_t<T>>(window);
    return last_tick >= delta ? last_tick - delta : T{};
  }

  constexpr size_t num_depends() const noexcept override { return 1; }
  constexpr size_t num_outputs() const noexcept override { return 1; }
};

template <typename T>
struct engine {
  using node_type = std::shared_ptr<op_base<T>>;
  std::vector<node_type> nodes{};
  std::vector<std::vector<size_t>> depends{};

  struct slice {
    T tick;
    std::vector<double> data;
  };

  std::deque<slice> steps{};
  std::vector<size_t> v_idx{}; // Indexes into steps.data for each node output
  size_t v_size{};             // Total size of all outputs

  std::vector<T> watermarks{}; // Last tick for each node that was cleaned up to

  explicit engine(size_t input_size) {
    if (input_size == 0) {
      throw std::invalid_argument("Input size must be greater than 0");
    }
    auto root = std::make_shared<root_input<T>>(input_size);
    auto result = add_op(std::move(root), std::vector<size_t>{});
    if (result == std::numeric_limits<size_t>::max()) {
      throw std::runtime_error("Failed to add root input node");
    }
  }

  template <std::ranges::input_range R>
  size_t add_op(node_type op, R &&dependencies) {
    size_t id = nodes.size();

    // Validate dependencies first
    for (auto dep : dependencies) {
      if (dep >= id) {
        // Invalid dependency - don't modify state
        return std::numeric_limits<size_t>::max();
      }
    }

    // Validate that the number of dependencies matches what the node expects
    if (std::ranges::distance(dependencies) != static_cast<std::ptrdiff_t>(op->num_depends())) {
      return std::numeric_limits<size_t>::max();
    }

    // All validations passed, now modify state
    nodes.push_back(std::move(op));
    depends.emplace_back(std::ranges::begin(dependencies), std::ranges::end(dependencies));
    watermarks.push_back(T{}); // Initialize watermark for new node

    auto n = nodes.back()->num_outputs();
    v_size += n;
    v_idx.push_back(v_size - n);
    return id;
  }

  // Step function: execute one computation step with given input data
  void step(T tick, const std::vector<double> &input_data) {
    // Validate input
    if (nodes.empty() || input_data.size() != nodes[0]->num_outputs()) {
      return; // Invalid input, skip
    }

    // Check for non-monotonic ticks (basic validation)
    if (!steps.empty() && tick <= steps.back().tick) {
      return; // Skip non-monotonic or duplicate ticks
    }

    // Ensure we have space for this step's data
    steps.emplace_back();
    auto &current_step = steps.back();
    current_step.tick = tick;
    current_step.data.resize(v_size, 0.0); // Initialize with zeros

    // Process each node in topological order
    for (size_t node_id = 0; node_id < nodes.size(); ++node_id) {
      auto &node = nodes[node_id];

      // 1. Prepare input pointers for this node
      std::vector<const double *> input_ptrs;
      input_ptrs.reserve(depends[node_id].size() + 1); // +1 for potential root input

      if (node_id == 0) {
        // Root input node gets external input
        input_ptrs.push_back(input_data.data());
      } else {
        // Construct input from dependencies
        for (auto dep_id : depends[node_id]) {
          input_ptrs.push_back(current_step.data.data() + v_idx[dep_id]);
        }
      }

      // 2. Call step on the node
      node->step(tick, input_ptrs.data());

      // Logic of calling inverse:
      // Operator reports its watermark X, which means its state should only contain data from (X, current_step.tick]
      // e.g. A rolling sum of 4 bars: on bar 6, its state should only contain data from bar 3 to bar 6, and op reports
      // watermark 2 because 6 (tick) - 4 (window) = 2 (watermark), engine calls inverse on all steps with last < ticks
      // <= 2

      // 3. Check expired data and call inverse if needed
      auto wm = node->watermark();
      if (wm != T{} && node_id > 0) { // Skip watermark processing for root node
        auto last = watermarks[node_id];
        // Only assert if last is not default-initialized (T{})
        if (last != T{}) {
          assert(last <= wm); // Watermark should not decrease
        }

        // Use pre-allocated vector for removal pointers
        std::vector<const double *> rm_ptrs;
        rm_ptrs.reserve(depends[node_id].size());
        // Iterate through historical steps and clean up expired data
        for (auto const &history : steps) {
          if (history.tick > wm)
            break; // cleanup finished
          if (history.tick <= last)
            continue; // already cleaned up this tick

          // Prepare removal data pointers
          rm_ptrs.clear();
          for (auto dep_id : depends[node_id]) {
            rm_ptrs.push_back(history.data.data() + v_idx[dep_id]);
          }
          node->inverse(history.tick, rm_ptrs.data());
        }

        // Update cleanup watermark for this node - use wm instead of last
        watermarks[node_id] = wm;
      }

      // 4. Get node's output and write to current step data AFTER watermark cleanup
      double *output_ptr = current_step.data.data() + v_idx[node_id];
      node->value(output_ptr);
    }

    // 5. Clean up expired data based on minimum watermark across all nodes
    T min_watermark = tick; // Start with current tick as maximum possible
    bool has_watermark = false;
    for (size_t i = 1; i < watermarks.size(); ++i) { // Skip root node (index 0)
      auto const &wm = watermarks[i];
      if (wm != T{}) {
        has_watermark = true;
        if (wm < min_watermark) {
          min_watermark = wm;
        }
      }
    }

    // Only remove steps if we have watermarks and multiple steps
    if (has_watermark) {
      while (steps.size() > 1 && steps.front().tick <= min_watermark) {
        steps.pop_front();
      }
    }
  }

  // Get the latest output data
  const std::vector<double> &get_latest_output() const {
    if (steps.empty()) {
      static const std::vector<double> empty;
      return empty;
    }
    return steps.back().data;
  }

  // Get output for a specific node from the latest step
  std::vector<double> get_node_output(size_t node_id) const {
    if (steps.empty() || node_id >= nodes.size()) {
      return {};
    }

    const auto &latest_data = steps.back().data;
    size_t start_idx = v_idx[node_id];
    size_t num_outputs = nodes[node_id]->num_outputs();

    return std::vector<double>(latest_data.begin() + static_cast<std::ptrdiff_t>(start_idx),
                               latest_data.begin() + static_cast<std::ptrdiff_t>(start_idx + num_outputs));
  }

  // Validate engine state for debugging
  bool validate_state() const {
    if (nodes.size() != depends.size() || nodes.size() != watermarks.size()) {
      return false;
    }

    if (nodes.size() != v_idx.size()) {
      return false;
    }

    // Check that dependencies are valid
    for (size_t i = 0; i < depends.size(); ++i) {
      for (auto dep : depends[i]) {
        if (dep >= i) { // Dependency should come before current node
          return false;
        }
      }

      // Check dependency count matches node expectation
      if (depends[i].size() != nodes[i]->num_depends()) {
        return false;
      }
    }

    return true;
  }

  // Clear all historical data (useful for reset)
  void clear_history() {
    steps.clear();
    std::fill(watermarks.begin(), watermarks.end(), T{});
  }

  // Get memory usage estimation in bytes
  size_t estimated_memory_usage() const {
    size_t total = 0;
    total += steps.size() * (sizeof(slice) + v_size * sizeof(double));
    total += nodes.size() * sizeof(node_type);
    total += depends.size() * sizeof(std::vector<size_t>);
    for (const auto &dep_list : depends) {
      total += dep_list.size() * sizeof(size_t);
    }
    return total;
  }

  // Utility methods
  size_t num_nodes() const { return nodes.size(); }

  size_t total_output_size() const { return v_size; }

  bool has_steps() const { return !steps.empty(); }

  size_t num_steps() const { return steps.size(); }

  // Get all step ticks (for debugging/monitoring)
  std::vector<T> get_step_ticks() const {
    std::vector<T> ticks;
    ticks.reserve(steps.size());
    for (const auto &step : steps) {
      ticks.push_back(step.tick);
    }
    return ticks;
  }
};

using engine_int = engine<int>;
} // namespace opflow
