#pragma once

#include "../def.hpp"
#include "../tumble_base.hpp"

namespace opflow::tumble {
template <typename T>
class session : public tumble_base<T> {
  using base = tumble_base<T>;

public:
  using typename base::data_type;
  using typename base::spec_type;

  explicit session(data_type session_gap) noexcept : session_gap(session_gap), last_tick(), emitting(), init() {}

  bool on_data(data_type timestamp, data_type const *) noexcept override {
    if (!init) {
      last_tick = timestamp;
      init = true;
    }

    if (timestamp - last_tick < session_gap) {
      last_tick = timestamp;
      return false; // No window emitted
    }

    emitting = last_tick;
    last_tick = timestamp;
    return true; // Emit the current window
  }

  spec_type emit() noexcept override { return {.timestamp = emitting, .include = false}; }

  OPFLOW_CLONEABLE(session)

private:
  data_type const session_gap;
  data_type last_tick;
  data_type emitting;
  bool init;
};
} // namespace opflow::tumble
