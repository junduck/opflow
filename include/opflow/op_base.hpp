#pragma once

#include <cassert>
#include <tuple>
#include <variant>

#include "common.hpp"
#include "def.hpp"
#include "detail/utils.hpp"

namespace opflow {
enum class win_mode {
  cumulative, ///< Cumulative, no eviction / sliding window
  dyn_event,  ///< Dynamic event-based window
  event,      ///< Static event-based window
  dyn_time,   ///< Dynamic time-based window
  time,       ///< Static time-based window
};

struct event_mode_tag {};
struct time_mode_tag {};
constexpr inline event_mode_tag event_mode{};
constexpr inline time_mode_tag time_mode{};

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
 * - Execution engine guarantees valid pointers and correct array sizes.
 * - Operator should avoid buffering window data.
 * - Operator should aim for O(1) space and time online algorithms.
 * - Execution engine calls window_mode() once on init to determine operator window mode.
 * - If operator is cumulative, on_evict() won't be called. e.g. EMA, CMA
 * - If operator is non-cumulative
 *    - static window: engine calls window_size() once on init to determine the window size.
 *    - dynamic window: engine calls on_data() -> window_size() -> on_evict() on every step.
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

  virtual win_mode window_mode() const noexcept { return win_mode::cumulative; }
  virtual size_t window_size(event_mode_tag) const noexcept { return 0; }
  virtual data_type window_size(time_mode_tag) const noexcept { return data_type{}; }

  virtual size_t num_inputs() const noexcept = 0;
  virtual size_t num_outputs() const noexcept = 0;

  virtual op_base *clone_at(void *mem) const noexcept = 0;
  virtual size_t clone_size() const noexcept = 0;
  virtual size_t clone_align() const noexcept = 0;

  virtual ~op_base() noexcept = default;
};

template <typename T>
class simple_rollop : public op_base<T> {
public:
  using data_type = T;
  using base = op_base<T>;

  template <std::integral I>
  explicit simple_rollop(I win_event) noexcept : win_size(static_cast<size_t>(win_event)) {}

  explicit simple_rollop(data_type win_time) noexcept : win_size(win_time) {}

  win_mode window_mode() const noexcept override {
    auto visitor =
        detail::overload{[](size_t size) { return very_small(size) ? win_mode::cumulative : win_mode::event; },
                         [](data_type size) { return very_small(size) ? win_mode::cumulative : win_mode::time; }};
    return std::visit(visitor, win_size);
  }

  size_t window_size(event_mode_tag) const noexcept override {
    assert(std::holds_alternative<size_t>(win_size) &&
           "[BUG] Graph executor calls window_size(event_mode_tag) on time-based window op.");
    return std::get<size_t>(win_size);
  }

  data_type window_size(time_mode_tag) const noexcept override {
    assert(std::holds_alternative<data_type>(win_size) &&
           "[BUG] Graph executor calls window_size(time_mode_tag) on event-based window op.");
    return std::get<data_type>(win_size);
  }

protected:
  std::variant<size_t, data_type> win_size;
};

template <typename T, win_mode MODE>
class mode_rollop : public op_base<T> {
  static_assert(MODE != win_mode::dyn_event && MODE != win_mode::dyn_time,
                "Dynamic window mode not supported in mode_rollop.");

public:
  using base = op_base<T>;
  using typename base::data_type;

  explicit mode_rollop(size_t win_event) noexcept
    requires(MODE == win_mode::event)
      : win_event(win_event) {}

  explicit mode_rollop(data_type win_time) noexcept
    requires(MODE == win_mode::time)
      : win_time(win_time) {}

  win_mode window_mode() const noexcept override { return MODE; }

  size_t window_size(event_mode_tag) const noexcept override {
    if constexpr (MODE == win_mode::event) {
      return win_event;
    } else {
      assert(false && "[BUG] Graph executor calls window_size(event_mode_tag) on time-based window op.");
      // std::unreachable();
      return {};
    }
  }

  data_type window_size(time_mode_tag) const noexcept override {
    if constexpr (MODE == win_mode::time) {
      return win_time;
    } else {
      assert(false && "[BUG] Graph executor calls window_size(time_mode_tag) on event-based window op.");
      // std::unreachable();
      return {};
    }
  }

protected:
  union {
    size_t win_event;
    data_type win_time;
  };
};

template <typename T>
struct op_root : op_base<T> {
  using base = op_base<T>;
  using typename base::data_type;

  size_t const input_size;
  data_type const *mem;

  explicit op_root(size_t n) : input_size(n), mem(nullptr) {}

  void on_data(data_type const *in) noexcept override { mem = in; }
  void value(data_type *out) const noexcept override {
    data_type *OPFLOW_RESTRICT cast = out;
    for (size_t i = 0; i < input_size; ++i) {
      cast[i] = mem[i];
    }
  }

  OPFLOW_INOUT(input_size, input_size)
  OPFLOW_CLONEABLE(op_root)
};

template <typename T>
struct dag_root<op_base<T>> {
  using type = op_root<T>;
};

} // namespace opflow
