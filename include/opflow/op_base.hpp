#pragma once

#include <cassert>
#include <tuple>
#include <variant>

#include "common.hpp"
#include "def.hpp"
#include "detail/utils.hpp"

namespace opflow {
enum class win_type {
  event, ///< Event-based window
  time,  ///< Time-based window
};

struct event_window_tag {};
struct time_window_tag {};
constexpr inline event_window_tag event_window{};
constexpr inline time_window_tag time_window{};

/**
 * @brief Base class for online operators
 *
 * Instruction for coding agent:
 *
 * - This is the base class for all online (streaming) operators.
 * - When new data is available, on_data() is called with an array of T to update state: T const* in.
 * - When old data is expired (out of window), on_evict() is called with an array of T to remove: T const* rm.
 * - To retrieve the current result, value() is called with an array of T to write: T* out.
 * - Number of input elements is given by num_inputs().
 * - Number of output elements is given by num_outputs().
 * - reset() restores the functor to its initial state if the functor is stateful.
 * - Execution engine guarantees valid pointers and correct array sizes.
 * - Operator should avoid buffering window data.
 * - Operator should aim for O(1) space and time online algorithms.
 * - Execution engine calls is_cumulative() once on init to determine if the operator is cumulative.
 * - If operator is cumulative, on_evict() won't be called. e.g. EMA, CMA
 * - If operator is non-cumulative
 *    - Execution engine calls window_type() once on init to determine the window type.
 *    - Execution engine calls is_dynamic() once on init to determine if the operator has dynamic windowing.
 *        - static window: engine calls window_size() once on init to determine the window size.
 *        - dynamic window: engine calls window_size() after every on_data() to determine the window size.
 * - Refer to @see opflow::op::avg @see opflow::op::dynwin_avg for example implementations.
 * - Important checklist:
 *    - Implement on_evict() for non-cumulative operators.
 *    - Do not buffer window data unnecessarily.
 *
 * @tparam T data type
 */
template <typename T>
struct op_base {
  using data_type = T;

  virtual void on_data(data_type const *in) noexcept = 0;
  virtual void on_evict(data_type const *rm) noexcept { std::ignore = rm; }
  virtual void value(data_type *out) const noexcept = 0;
  virtual void reset() noexcept = 0;

  virtual size_t num_inputs() const noexcept = 0;
  virtual size_t num_outputs() const noexcept = 0;

  virtual bool is_cumulative() const noexcept { return true; }
  virtual bool is_dynamic() const noexcept { return false; }

  virtual win_type window_type() const noexcept { return win_type::event; }
  virtual size_t window_size(event_window_tag) const noexcept { return 0; }
  virtual data_type window_size(time_window_tag) const noexcept { return data_type{}; }

  virtual op_base *clone_at(void *mem) const noexcept = 0;
  virtual size_t clone_size() const noexcept = 0;
  virtual size_t clone_align() const noexcept = 0;

  virtual ~op_base() noexcept = default;
};

/**
 * @brief Simple windowed operator base class
 *
 * The default implementation:
 * - Takes a single constructor argument for the window size.
 * - is_dynamic() returns false, derived class should override it if needed.
 * - is_cumulative() returns true if the window size is zero.
 * - window_size() returns this->win_event or this->win_time depending on the window type.
 *
 * @tparam T data type
 * @tparam WIN window type
 */
template <typename T, win_type WIN>
struct win_base : public op_base<T> {
  using base = op_base<T>;
  using typename base::data_type;

  union {
    size_t win_event;
    data_type win_time;
  };

  explicit win_base(size_t win_event) noexcept
    requires(WIN == win_type::event)
      : win_event(win_event) {}

  explicit win_base(data_type win_time) noexcept
    requires(WIN == win_type::time)
      : win_time(win_time) {}

  bool is_cumulative() const noexcept override {
    if constexpr (WIN == win_type::event) {
      return win_event == 0;
    } else {
      return win_time == data_type{};
    }
  }

  win_type window_type() const noexcept override { return WIN; }

  size_t window_size(event_window_tag) const noexcept override {
    if constexpr (WIN == win_type::event) {
      return win_event;
    } else {
      assert(false && "[BUG] Graph executor calls window_size(event_window_tag) on time-based window op.");
      // std::unreachable();
      return {};
    }
  }

  data_type window_size(time_window_tag) const noexcept override {
    if constexpr (WIN == win_type::time) {
      return win_time;
    } else {
      assert(false && "[BUG] Graph executor calls window_size(time_window_tag) on event-based window op.");
      // std::unreachable();
      return {};
    }
  }
};

/**
 * @brief Simple windowed operator base class, with erased window type
 *
 * The default implementation:
 * - Has same behaviour as @see opflow::win_base
 * - Erases the window type so implementations can be agnostic of the window type.
 * - Constructor overload determines the window type: data_type for time-based window, size_t for event-based window.
 *
 * @tparam T data type
 * @tparam WIN window type
 */
template <typename T>
struct win_erased_base : public op_base<T> {
  using data_type = T;
  using base = op_base<T>;

  std::variant<size_t, data_type> win_size;

  explicit win_erased_base(size_t win_event) noexcept : win_size(win_event) {}

  explicit win_erased_base(data_type win_time) noexcept : win_size(win_time) {}

  bool is_cumulative() const noexcept override {
    auto visitor =
        detail::overload{[](size_t arg) { return arg == 0; }, [](data_type arg) { return arg == data_type{}; }};
    return std::visit(visitor, win_size);
  }

  win_type window_type() const noexcept override {
    auto visitor = detail::overload{[](size_t) { return win_type::event; }, [](data_type) { return win_type::time; }};
    return std::visit(visitor, win_size);
  }

  size_t window_size(event_window_tag) const noexcept override {
    assert(std::holds_alternative<size_t>(win_size) &&
           "[BUG] Graph executor calls window_size(event_window_tag) on time-based window op.");
    return std::get<size_t>(win_size);
  }

  data_type window_size(time_window_tag) const noexcept override {
    assert(std::holds_alternative<data_type>(win_size) &&
           "[BUG] Graph executor calls window_size(time_window_tag) on event-based window op.");
    return std::get<data_type>(win_size);
  }
};

template <typename T>
struct op_root : op_base<T> {
  using base = op_base<T>;
  using typename base::data_type;

  data_type const *mem;
  size_t const input_size;

  explicit op_root(size_t n) : mem(nullptr), input_size(n) {}

  void on_data(data_type const *in) noexcept override { mem = in; }
  void value(data_type *out) const noexcept override {
    data_type *OPFLOW_RESTRICT cast = out;
    for (size_t i = 0; i < input_size; ++i) {
      cast[i] = mem[i];
    }
  }
  void reset() noexcept override {}

  size_t num_inputs() const noexcept override { return input_size; }
  size_t num_outputs() const noexcept override { return input_size; }

  OPFLOW_CLONEABLE(op_root)
};

template <typename T>
struct dag_root<op_base<T>> {
  using type = op_root<T>;
};

} // namespace opflow
