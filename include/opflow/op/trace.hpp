#pragma once

#include <chrono>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "opflow/op_base.hpp"

namespace opflow::op {

enum class trace_event_type { init, step, inverse, value, window_start_query };

template <time_point_like T>
struct trace_event {
  trace_event_type type;
  T tick;
  std::chrono::steady_clock::time_point wall_time;
  std::vector<std::vector<double>> input_data; // Copy of input data for debugging
  std::vector<double> output_data;             // Copy of output data after operation
  T window_start_result{};                     // Result of window_start() if called

  trace_event(trace_event_type t, T tick_val) : type(t), tick(tick_val), wall_time(std::chrono::steady_clock::now()) {}
};

template <time_point_like T>
struct trace : public op_base<T> {
private:
  std::shared_ptr<op_base<T>> wrapped_op;
  std::string op_name;
  mutable std::deque<trace_event<T>> event_history;
  size_t max_events;
  bool capture_data;

public:
  explicit trace(std::shared_ptr<op_base<T>> op, std::string name = "unknown_op", size_t max_history = 1000,
                 bool capture_input_output = true)
      : wrapped_op(std::move(op)), op_name(std::move(name)), max_events(max_history),
        capture_data(capture_input_output) {

    if (!wrapped_op) {
      throw std::invalid_argument("Cannot trace null operator");
    }
  }

  void init(T tick, double const *const *in) noexcept override {
    trace_event<T> event(trace_event_type::init, tick);

    // Capture input data if enabled
    if (capture_data && in) {
      event.input_data.reserve(num_depends());
      for (size_t dep_id = 0; dep_id < num_depends(); ++dep_id) {
        if (in[dep_id]) {
          size_t input_size = num_inputs(dep_id);
          event.input_data.emplace_back(in[dep_id], in[dep_id] + input_size);
        } else {
          event.input_data.emplace_back(); // Empty vector for null input
        }
      }
    }

    // Call the wrapped operator
    wrapped_op->init(tick, in);

    // Store event
    add_event(std::move(event));
  }

  void step(T tick, double const *const *in) noexcept override {
    trace_event<T> event(trace_event_type::step, tick);

    // Capture input data if enabled
    if (capture_data && in) {
      event.input_data.reserve(num_depends());
      for (size_t dep_id = 0; dep_id < num_depends(); ++dep_id) {
        if (in[dep_id]) {
          size_t input_size = num_inputs(dep_id);
          event.input_data.emplace_back(in[dep_id], in[dep_id] + input_size);
        } else {
          event.input_data.emplace_back(); // Empty vector for null input
        }
      }
    }

    // Call the wrapped operator
    wrapped_op->step(tick, in);

    // Store event
    add_event(std::move(event));
  }

  void inverse(T tick, double const *const *rm) noexcept override {
    trace_event<T> event(trace_event_type::inverse, tick);

    // Capture removal data if enabled
    if (capture_data && rm) {
      event.input_data.reserve(num_depends());
      for (size_t dep_id = 0; dep_id < num_depends(); ++dep_id) {
        if (rm[dep_id]) {
          size_t input_size = num_inputs(dep_id);
          event.input_data.emplace_back(rm[dep_id], rm[dep_id] + input_size);
        } else {
          event.input_data.emplace_back(); // Empty vector for null input
        }
      }
    }

    // Call the wrapped operator
    wrapped_op->inverse(tick, rm);

    // Capture output data after inverse if enabled
    if (capture_data) {
      event.output_data.resize(num_outputs());
      wrapped_op->value(event.output_data.data());
    }

    // Store event
    add_event(std::move(event));
  }

  void value(double *out) noexcept override {
    trace_event<T> event(trace_event_type::value, T{});

    // Call the wrapped operator
    wrapped_op->value(out);

    // Capture output data if enabled
    if (capture_data && out) {
      event.output_data.assign(out, out + num_outputs());
    }

    // Store event
    add_event(std::move(event));
  }

  T window_start() const noexcept override {
    trace_event<T> event(trace_event_type::window_start_query, T{});

    // Call the wrapped operator
    T result = wrapped_op->window_start();
    event.window_start_result = result;

    // Store event
    add_event(std::move(event));

    return result;
  }

  // Delegate interface methods to wrapped operator
  size_t num_depends() const noexcept override { return wrapped_op->num_depends(); }

  size_t num_inputs(size_t pid) const noexcept override { return wrapped_op->num_inputs(pid); }

  size_t num_outputs() const noexcept override { return wrapped_op->num_outputs(); }

  template <std::ranges::forward_range R>
  bool compatible_with(R &&deps) const noexcept
    requires detail::range_derived_from<R, op_base<T>>
  {
    return wrapped_op->compatible_with(std::forward<R>(deps));
  }

  // Debugging and inspection methods
  const std::string &get_name() const { return op_name; }

  const std::deque<trace_event<T>> &get_event_history() const { return event_history; }

  void clear_history() { event_history.clear(); }

  size_t get_step_count() const {
    return std::count_if(event_history.begin(), event_history.end(),
                         [](const auto &e) { return e.type == trace_event_type::step; });
  }

  size_t get_inverse_count() const {
    return std::count_if(event_history.begin(), event_history.end(),
                         [](const auto &e) { return e.type == trace_event_type::inverse; });
  }

  size_t get_value_count() const {
    return std::count_if(event_history.begin(), event_history.end(),
                         [](const auto &e) { return e.type == trace_event_type::value; });
  }

  std::vector<T> get_step_ticks() const {
    std::vector<T> ticks;
    for (const auto &event : event_history) {
      if (event.type == trace_event_type::step) {
        ticks.push_back(event.tick);
      }
    }
    return ticks;
  }

  std::vector<T> get_inverse_ticks() const {
    std::vector<T> ticks;
    for (const auto &event : event_history) {
      if (event.type == trace_event_type::inverse) {
        ticks.push_back(event.tick);
      }
    }
    return ticks;
  }

  // Check if operations are happening in expected order
  bool validate_monotonic_steps() const {
    T last_tick{};
    bool first = true;

    for (const auto &event : event_history) {
      if (event.type == trace_event_type::step) {
        if (!first && event.tick <= last_tick) {
          return false; // Non-monotonic step detected
        }
        last_tick = event.tick;
        first = false;
      }
    }
    return true;
  }

  // Get the wrapped operator (for advanced debugging)
  std::shared_ptr<op_base<T>> get_wrapped_operator() const { return wrapped_op; }

  // Enable/disable data capture at runtime
  void set_data_capture(bool enabled) { capture_data = enabled; }

  bool is_data_capture_enabled() const { return capture_data; }

  // Get timing statistics
  struct timing_stats {
    std::chrono::nanoseconds total_step_time{0};
    std::chrono::nanoseconds total_inverse_time{0};
    std::chrono::nanoseconds total_value_time{0};
    size_t step_count{0};
    size_t inverse_count{0};
    size_t value_count{0};

    std::chrono::nanoseconds avg_step_time() const {
      return step_count > 0 ? total_step_time / step_count : std::chrono::nanoseconds{0};
    }

    std::chrono::nanoseconds avg_inverse_time() const {
      return inverse_count > 0 ? total_inverse_time / inverse_count : std::chrono::nanoseconds{0};
    }

    std::chrono::nanoseconds avg_value_time() const {
      return value_count > 0 ? total_value_time / value_count : std::chrono::nanoseconds{0};
    }
  };

  timing_stats get_timing_stats() const {
    timing_stats stats;

    for (size_t i = 1; i < event_history.size(); ++i) {
      const auto &prev = event_history[i - 1];
      const auto &curr = event_history[i];

      auto duration = curr.wall_time - prev.wall_time;

      switch (prev.type) {
      case trace_event_type::step:
        stats.total_step_time += duration;
        stats.step_count++;
        break;
      case trace_event_type::inverse:
        stats.total_inverse_time += duration;
        stats.inverse_count++;
        break;
      case trace_event_type::value:
        stats.total_value_time += duration;
        stats.value_count++;
        break;
      case trace_event_type::window_start_query:
        // Don't count window_start queries in timing
        break;
      }
    }

    return stats;
  }

private:
  void add_event(trace_event<T> event) const {
    event_history.push_back(std::move(event));

    // Trim history if it exceeds max size
    if (event_history.size() > max_events) {
      event_history.pop_front();
    }
  }
};

// Helper function to create traced operators
template <time_point_like T>
std::shared_ptr<trace<T>> make_trace(std::shared_ptr<op_base<T>> op, std::string name = "traced_op",
                                     size_t max_history = 1000, bool capture_data = true) {
  return std::make_shared<trace<T>>(std::move(op), std::move(name), max_history, capture_data);
}

constexpr inline size_t s_trace = sizeof(trace<uint64_t>);

} // namespace opflow::op
