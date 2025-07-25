#pragma once

#include <algorithm>
#include <cassert>
#include <limits>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <vector>

#include "dependency_map.hpp"
#include "history_ringbuf.hpp"
#include "op_base.hpp"

namespace opflow {

template <typename T>
struct engine_builder;
template <typename T>
struct engine;

template <typename T>
struct root_input : public op_base<T> {
  std::vector<double> mem;

  explicit root_input(size_t n) : mem(n, 0.0) {} // Initialize with zeros

  void step(T tick, double const *const *in) noexcept override {
    std::ignore = tick; // Unused in root_input
    if (in && in[0]) {  // Add null pointer check
      std::copy(in[0], in[0] + mem.size(), mem.data());
    }
  }
  void value(double *out) noexcept override {
    if (out) { // Add null pointer check
      std::copy(mem.begin(), mem.end(), out);
    }
  }

  size_t num_depends() const noexcept override { return 0; } // Root input has no dependencies
  size_t num_outputs() const noexcept override { return mem.size(); }
  size_t num_inputs(size_t) const noexcept override { return 0; } // No inputs
};

template <typename T>
struct rollsum : public op_base<T> {
  double sum{};
  T current{};
  duration_t<T> window_size;
  std::vector<size_t> sum_idx{};

  template <std::ranges::input_range R>
  rollsum(R &&idx, duration_t<T> window = {}) : window_size(window) {
    if (std::ranges::empty(idx))
      sum_idx.push_back(0);
    else
      sum_idx.assign(std::ranges::begin(idx), std::ranges::end(idx));
  }

  bool cumulative() const noexcept { return window_size == duration_t<T>{}; }

  void step(T tick, double const *const *in) noexcept override {
    assert(tick != T{} && "default-constructed tick.");
    assert(tick > current && "non-monotonic tick.");

    current = tick;

    auto const *data = in[0];
    for (auto i : sum_idx)
      sum += data[i];
  }

  void inverse(T tick, double const *const *rm) noexcept override {
    assert(!cumulative() && "Inverse called on cumulative rollsum");

    std::ignore = tick; // Unused in reverse
    auto const *data = rm[0];
    for (auto i : sum_idx) {
      sum -= data[i];
    }
  }

  void value(double *out) noexcept override { *out = sum; }

  size_t num_depends() const noexcept override { return 1; }
  size_t num_outputs() const noexcept override { return 1; }
  size_t num_inputs(size_t pid) const noexcept override {
    assert(pid == 0 && "rollsum expects input from parent with id 0");

    return sum_idx.size();
  }
};

template <typename T>
struct engine_builder {
  using node_type = std::shared_ptr<op_base<T>>;

  struct node_info {
    node_type op;
    std::vector<size_t> dependencies;
    size_t output_offset;
    size_t output_count;
  };

  std::vector<node_info> nodes{};
  dependency_map dependency_graph{};
  size_t total_output_size{0};

  explicit engine_builder(size_t input_size) {
    if (input_size == 0) {
      throw std::invalid_argument("Input size must be greater than 0");
    }

    // Add root input node
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
        return std::numeric_limits<size_t>::max();
      }
    }

    // Validate that the number of dependencies matches what the node expects
    if (std::ranges::distance(dependencies) != static_cast<std::ptrdiff_t>(op->num_depends())) {
      return std::numeric_limits<size_t>::max();
    }

    // Add to dependency graph
    auto dep_id = dependency_graph.add(dependencies);
    if (dep_id == invalid_id) {
      return std::numeric_limits<size_t>::max();
    }

    // Calculate output offset and size
    size_t output_count = op->num_outputs();
    size_t output_offset = total_output_size;
    total_output_size += output_count;

    // Store node info
    std::vector<size_t> deps_vec(std::ranges::begin(dependencies), std::ranges::end(dependencies));
    nodes.push_back({std::move(op), std::move(deps_vec), output_offset, output_count});

    return id;
  }

  size_t num_nodes() const { return nodes.size(); }
  size_t get_total_output_size() const { return total_output_size; }

  // Build method declaration - implementation after engine definition
  engine<T> build(size_t initial_history_capacity = 64);
};

template <typename T>
struct engine {
  using node_type = std::shared_ptr<op_base<T>>;

  std::vector<node_type> nodes{};
  dependency_map dependency_graph{};
  std::vector<size_t> output_offset{}; // Starting index in step data for each node output
  size_t output_size{};                // Total size of all outputs per step

  history_ringbuf<T, double> step_history; // High-performance circular buffer for step data
  std::vector<T> watermarks{};             // Last tick for each node that was cleaned up to

  // Private constructor for engine_builder
  explicit engine(engine_builder<T> &&builder, size_t initial_history_capacity)
      : dependency_graph(std::move(builder.dependency_graph)), output_size(builder.total_output_size),
        step_history(output_size, initial_history_capacity) {

    nodes.reserve(builder.nodes.size());
    output_offset.reserve(builder.nodes.size());
    watermarks.resize(builder.nodes.size(), T{});

    // Move nodes and build offset/rolling tracking
    for (auto &node_info : builder.nodes) {
      nodes.push_back(std::move(node_info.op));
      output_offset.push_back(node_info.output_offset);
    }
  }

  // Legacy constructor for backward compatibility
  explicit engine(size_t input_size) : output_size(input_size), step_history(input_size, 64) {

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
    if (!std::ranges::all_of(dependencies, [id](auto dep) { return dep < id; })) {
      return invalid_id; // Invalid dependency index
    }

    auto dep_ptrs = std::views::all(dependencies) | std::views::transform([this](auto dep) {
                      return static_cast<op_base<T> const *>(nodes[dep].get());
                    });
    if (!op->compatible_with(dep_ptrs)) {
      return invalid_id; // Incompatible dependencies
    }

    // All validations passed, now modify state
    nodes.push_back(std::move(op));
    auto dep_id = dependency_graph.add(dependencies);
    if (dep_id == invalid_id) {
      nodes.pop_back();
      return std::numeric_limits<size_t>::max();
    }
    watermarks.push_back(T{});

    auto n = nodes.back()->num_outputs();

    // For legacy constructor, we need to resize history if output size changes
    if (output_size == nodes[0]->num_outputs() && nodes.size() > 1) {
      // This is a legacy add_op call, we need to rebuild history with new size
      output_size += n;
      output_offset.push_back(output_size - n);

      // Create new history with updated size
      history_ringbuf<T, double> new_history(output_size, step_history.size() > 0 ? step_history.size() : 64);

      // Copy existing data if any
      for (const auto &[step_tick, step_data] : step_history) {
        std::vector<double> new_data(output_size, 0.0);
        std::copy(step_data.begin(), step_data.end(), new_data.begin());
        new_history.push(step_tick, new_data);
      }

      step_history = std::move(new_history);
    } else {
      output_size += n;
      output_offset.push_back(output_size - n);
    }

    return id;
  }

  // Step function: execute one computation step with given input data
  auto step(T tick, const std::vector<double> &input_data) {
    // Validate input
    if (nodes.empty() || input_data.size() != nodes[0]->num_outputs()) {
      return;
    }

    // Check for non-monotonic ticks
    if (!step_history.empty() && tick <= step_history.back().first) {
      return;
    }

    // TODO: instead of using a copy of output buffer, we push history and use it in-place

    // Push empty entry to history and get mutable span for direct writing
    auto [_unused, data] = step_history.push(tick);

    // Process each node in topological order
    for (size_t node_id = 0; node_id < nodes.size(); ++node_id) {
      auto &node = nodes[node_id];

      // 1. Prepare input pointers for this node
      std::vector<const double *> input_ptrs;
      auto node_deps = dependency_graph.get_predecessors(node_id);
      input_ptrs.reserve(node_deps.size());

      if (node_id == 0) {
        // Root input node gets external input
        input_ptrs.push_back(input_data.data());
      } else {
        // Construct input from dependencies in current step
        for (auto dep_id : node_deps) {
          input_ptrs.push_back(data.data() + output_offset[dep_id]);
        }
      }

      // 2. Call step on the node
      node->step(tick, input_ptrs.data());

      // 3. Check expired data and call inverse if needed
      // auto retention = node->retention();
      auto retention = retention_policy::cumulative; // Default to cumulative for now
      if (retention != retention_policy::cumulative && node_id > 0) {
        // auto wm = node->watermark();
        auto wm = T{};
        if (wm != T{}) {
          auto last_wm = watermarks[node_id];
          if (last_wm != T{}) {
            assert(last_wm <= wm);
          }

          // Process watermark cleanup using history buffer
          std::vector<const double *> rm_ptrs;
          auto rm_node_deps = dependency_graph.get_predecessors(node_id);
          rm_ptrs.reserve(rm_node_deps.size());

          // Iterate through historical steps (excluding the current one we just added)
          for (size_t hist_idx = 0; hist_idx < step_history.size() - 1; ++hist_idx) {
            const auto [step_tick, step_data] = step_history[hist_idx];
            bool should_remove = false;

            if (retention == retention_policy::remove_start) {
              should_remove = (step_tick > last_wm && step_tick <= wm);
            } else if (retention == retention_policy::keep_start) {
              should_remove = (step_tick >= last_wm && step_tick < wm);
            }

            if (!should_remove) {
              continue;
            }

            // Prepare removal data pointers
            rm_ptrs.clear();
            for (auto dep_id : rm_node_deps) {
              rm_ptrs.push_back(step_data.data() + output_offset[dep_id]);
            }
            node->inverse(step_tick, rm_ptrs.data());
          }

          watermarks[node_id] = wm;
        }
      }

      // 4. Get node's output and write directly to step data
      double *output_ptr = data.data() + output_offset[node_id];
      node->value(output_ptr);
    }

    // 5. History entry is already stored with direct writes above

    // 6. Clean up expired history based on minimum watermark
    T min_watermark = tick;
    bool has_watermark = false;
    for (size_t i = 1; i < watermarks.size(); ++i) {
      // if (nodes[i]->retention() != retention_policy::cumulative) {
      if (false) {
        auto const &wm = watermarks[i];
        if (wm != T{}) {
          has_watermark = true;
          if (wm < min_watermark) {
            min_watermark = wm;
          }
        }
      }
    }

    // Remove expired steps from history
    if (has_watermark) {
      while (step_history.size() && step_history.front().first <= min_watermark) {
        step_history.pop();
      }
    }

    // TODO: history here is actually the sink
    // step_history.notify(); // Notify any observers of side effects?
  }

  // Get the latest output data
  std::vector<double> get_latest_output() const {
    if (!step_history.size()) {
      return {};
    }

    auto [latest_tick, latest_data] = step_history.back();
    return std::vector<double>(latest_data.begin(), latest_data.end());
  }

  // Get output for a specific node from the latest step
  std::vector<double> get_node_output(size_t node_id) const {
    if (step_history.empty() || node_id >= nodes.size()) {
      return {};
    }

    auto [latest_tick, latest_data] = step_history.back();
    size_t start_idx = output_offset[node_id];
    size_t num_outputs = nodes[node_id]->num_outputs();

    return std::vector<double>(latest_data.begin() + static_cast<std::ptrdiff_t>(start_idx),
                               latest_data.begin() + static_cast<std::ptrdiff_t>(start_idx + num_outputs));
  }

  // Validate engine state for debugging
  bool validate_state() const {
    if (nodes.size() != dependency_graph.size() || nodes.size() != watermarks.size()) {
      return false;
    }

    if (nodes.size() != output_offset.size()) {
      return false;
    }

    // Check dependency count matches node expectation
    for (size_t i = 0; i < nodes.size(); ++i) {
      if (dependency_graph.num_predecessors(i) != nodes[i]->num_depends()) {
        return false;
      }
    }

    return true;
  }

  // Clear all historical data
  void clear_history() {
    step_history.clear();
    std::fill(watermarks.begin(), watermarks.end(), T{});
  }

  // Utility methods
  size_t num_nodes() const { return nodes.size(); }
  size_t total_output_size() const { return output_size; }
  bool has_steps() const { return !step_history.empty(); }
  size_t num_steps() const { return step_history.size(); }

  // Get all step ticks (for debugging/monitoring)
  std::vector<T> get_step_ticks() const {
    std::vector<T> ticks;
    ticks.reserve(step_history.size());
    for (const auto &[step_tick, _unused] : step_history) {
      ticks.push_back(step_tick);
    }
    return ticks;
  }

  // Friend declaration for engine_builder access
  friend struct engine_builder<T>;
};

// Build method implementation for engine_builder
template <typename T>
engine<T> engine_builder<T>::build(size_t initial_history_capacity) {
  if (nodes.empty()) {
    throw std::runtime_error("Cannot build engine with no nodes");
  }

  return engine<T>(std::move(*this), initial_history_capacity);
}

using engine_int = engine<int>;
} // namespace opflow
