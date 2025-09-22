#pragma once

#include <cmath>
#include <concepts>

#include "../def.hpp"
#include "../tumble_base.hpp"

namespace opflow::tumble {
template <typename T>
class time : public tumble_base<T> {
  using base = tumble_base<T>;

public:
  using typename base::data_type;
  using typename base::spec_type;

  explicit time(data_type window_size) noexcept : window_size(window_size), next_tick(), emitting(), init() {}

  bool on_data(data_type tick, data_type const *) noexcept override {

    // Case 1: Initialization state
    if (!init) {
      next_tick = align_to_next_window_begin(tick);
      init = true;
    }

    // Case 2: Still in the same window (tick < next_tick)
    if (tick < next_tick) {
      return false; // No window emitted
    }

    // Case 3: tick >= next_tick, emit current window(s)
    emitting = next_tick;

    while (tick >= next_tick) {
      next_tick += window_size;
    }
    return true; // Emit the current window
  }

  spec_type emit() noexcept override {
    return {.timestamp = emitting,
            // Right-open interval: [prev_emitting, emitting)
            .include = false};
  }

  OPFLOW_CLONEABLE(time)

private:
  constexpr data_type align_to_next_window_begin(data_type tick) const noexcept {
    if constexpr (std::integral<data_type>) {
      return ((tick / window_size) + 1) * window_size;
    } else {
      return std::floor(tick / window_size + 1) * window_size;
    }
  }

  data_type const window_size;
  data_type next_tick;
  data_type emitting;
  bool init = false;
};
} // namespace opflow::tumble
