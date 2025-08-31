#pragma once

#include "opflow/common.hpp"
#include "opflow/def.hpp"
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
template <arithmetic T>
class tumbling : public window_base<T> {
  using base = window_base<T>;

public:
  using typename base::data_type;
  using typename base::spec_type;

  explicit tumbling(data_type window_size) noexcept
      : window_size(window_size), next_tick(max_time<data_type>()), count(), emitting() {}

  bool on_data(data_type tick, data_type const *) noexcept override {

    // Case 1: Initialization state - next_tick is max_time<data_type>()
    if (next_tick == max_time<data_type>()) {
      next_tick = aligned_next_window_begin(tick);
    }

    // append data to window
    ++count;

    // Case 2: Still in the same window (tick < next_tick)
    if (tick < next_tick) {
      return false; // No window emitted
    }

    // Case 3: tick >= next_tick, we've reached or moved past the current window
    emitting.timestamp = next_tick;
    emitting.offset = 0;
    emitting.size = count - 1; // Latest tick does not belong to emitted window (right open)
    emitting.evict = emitting.size;

    while (tick >= next_tick) {
      next_tick += window_size;
    }

    return true; // Emit the current window
  }

  bool flush() noexcept override {
    // If no data points have been accumulated, do not emit
    if (count == 0) {
      return false;
    }
    // Force emission of the current window and move to the next window
    emitting.timestamp = next_tick;
    emitting.offset = 0;
    emitting.size = count;
    emitting.evict = emitting.size;
    next_tick += window_size;
    return true;
  }

  spec_type emit() noexcept override {
    // size initialised to 1 due to last data point that triggered the emit belongs to new window
    count -= emitting.evict;
    return std::exchange(emitting, {});
  }

  void reset() noexcept override {
    next_tick = max_time<data_type>();
    count = 0;
    emitting = {};
  }

  OPFLOW_CLONEABLE(tumbling)

private:
  data_type const window_size; ///< Size of the tumbling window
  data_type next_tick;         ///< Next tick time point for the tumbling window
  size_t count;                ///< Current number of elements pushed to window
  spec_type emitting;          ///< Window specification to emit

  data_type aligned_next_window_begin(data_type tick) const noexcept {
    if constexpr (std::integral<data_type>) {
      auto remainder = tick % window_size;
      if (remainder == data_type{}) {
        return tick + window_size; // Already aligned
      } else {
        return tick - remainder + window_size; // Align to the next window start
      }
    } else if constexpr (std::floating_point<data_type>) {
      auto remainder = std::fmod(tick, window_size);
      if (remainder == data_type{}) {
        return tick + window_size; // Already aligned
      } else {
        return tick - remainder + window_size; // Align to the next window start
      }
    }
  }
};
} // namespace opflow::win
