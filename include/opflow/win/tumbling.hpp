#pragma once

#include "opflow/window_base.hpp"

namespace opflow::win {
/**
 * @brief Time-based tumbling window emitter.
 *
 * Emits a window every `window_size` time units. Emits all data points
 * that have arrived since the last emission. After emission, all points
 * in the window are evicted.
 *
 * Following financial data convention, windows are left-closed, right-open intervals.
 *
 * | Bar Timestamp | Interval Covered       |
 * |---------------|------------------------|
 * | 10:01:00      | [10:00:00, 10:01:00)   |
 * | 10:02:00      | [10:01:00, 10:02:00)   |
 *
 * Example:
 *   window_size = 10, aka windows should be emitted on 10, 20, 30...
 *   Data arrives at t=1,2,3,11,12,13,20,23,60,62,70
 *   - At t=1,2,3: no window emitted
 *   - At t=11: window emitted for points at t=1,2,3, associated timestamp 10 i.e. [0, 10)
 *   - At t=12,13: no window emitted
 *   - At t=20: window emitted for points at t=11,12,13, associated timestamp 20 i.e. [10, 20)
 *   - At t=23: no window emitted
 *   - At t=60: window emitted for points at t=20,23, associated timestamp 30. i.e. [20, 30)
 *   - At t=62: no window emitted
 *   - At t=70: window emitted for points at t=60,62, associated timestamp 70. i.e. [60, 70)
 *
 */
template <typename Time, typename Data>
struct tumbling : window_base<Time, Data> {
  using base = window_base<Time, Data>;
  using typename base::data_type;
  using typename base::dura_type;
  using typename base::spec_type;
  using typename base::time_type;

  dura_type const window_size; ///< Size of the tumbling window
  time_type next_tick;         ///< Next tick time point for the tumbling window
  spec_type curr;              ///< Current window specification

  time_type aligned_next_window_begin(time_type tick) const noexcept {
    if constexpr (std::integral<time_type>) {
      auto remainder = tick % window_size;
      if (remainder == dura_type{}) {
        return tick + window_size; // Already aligned
      } else {
        return tick - remainder + window_size; // Align to the next window start
      }
    } else if constexpr (std::floating_point<time_type>) {
      auto remainder = std::fmod(tick, window_size);
      if (remainder == dura_type{}) {
        return tick + window_size; // Already aligned
      } else {
        return tick - remainder + window_size; // Align to the next window start
      }
    } else {
      // TODO: this may be inefficient
      // our typename T only requires comparison and basic arithmetic
      time_type t{};
      while (t <= tick) {
        t += window_size;
      }
      return t; // Align to the next window start
    }
  }

  explicit tumbling(dura_type window) noexcept : window_size(window), next_tick(), curr() {}

  bool process(time_type tick, data_type const *) noexcept override {

    // Case 1: Initialization state - next_tick is T{} (default constructed)
    if (next_tick == time_type{}) {
      next_tick = aligned_next_window_begin(tick);
    }

    // Case 2: Still in the same window (tick < next_tick)
    if (tick < next_tick) {
      ++curr.size;
      return false; // No window emitted
    }

    // Case 3: tick >= next_tick, we've reached or moved past the current window
    curr.timestamp = next_tick;
    curr.evict = curr.size; // tumbling window

    while (tick >= next_tick) {
      next_tick += window_size;
    }

    return true; // Emit the current window
  }

  bool flush() noexcept override {
    // If no data points have been accumulated, do not emit
    if (curr.size == 0) {
      return false;
    }
    // Force emission of the current window and move to the next window
    curr.timestamp = next_tick;
    curr.evict = curr.size;
    next_tick += window_size;
    return true;
  }

  spec_type emit() noexcept override {
    // size initialised to 1 due to last data point that triggered the emit belongs to new window
    return std::exchange(curr, {.timestamp = {}, .size = 1, .evict = 0});
  }

  void reset() noexcept override {
    next_tick = time_type{};
    curr = {};
  }
};
} // namespace opflow::win
