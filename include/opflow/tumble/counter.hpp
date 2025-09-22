#pragma once

#include "../def.hpp"
#include "../tumble_base.hpp"

namespace opflow::tumble {
template <typename T>
class counter : public tumble_base<T> {
  using base = tumble_base<T>;

public:
  using typename base::data_type;
  using typename base::spec_type;
  explicit counter(size_t window_size) noexcept : window_size(window_size), count(), emitting() {}

  bool on_data(data_type tick, data_type const *) noexcept override {
    ++count;
    if (count < window_size) {
      return false;
    }
    // Emit current window
    // Timestamp is the timestamp of the last data point in the window
    emitting = tick;
    count = 0; // Reset count after emitting
    return true;
  }

  spec_type emit() noexcept override { return {.timestamp = emitting, .include = true}; }

  OPFLOW_CLONEABLE(counter)

private:
  size_t const window_size;
  size_t count;
  data_type emitting;
};
} // namespace opflow::tumble
