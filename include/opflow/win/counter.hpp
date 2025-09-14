#pragma once

#include "opflow/common.hpp"
#include "opflow/def.hpp"
#include "opflow/window_base.hpp"

namespace opflow::win {
// Simple event counter tumbling window
template <arithmetic T>
class counter : public window_base<T> {
  using base = window_base<T>;

public:
  using typename base::data_type;
  using typename base::spec_type;

  explicit counter(u32 window_size) noexcept : window_size(window_size), count(), time() {}

  bool on_data(data_type ts, data_type const *) noexcept override {
    time = ts;
    ++count;
    return count == window_size;
  }

  bool flush() noexcept override { return count; }

  spec_type emit() noexcept override {
    auto cnt = std::exchange(count, 0);
    return {.timestamp = time, .size = cnt, .offset = 0, .evict = cnt};
  }

  OPFLOW_CLONEABLE(counter);

private:
  u32 const window_size;
  u32 count;
  data_type time;
};
} // namespace opflow::win
