#pragma once

#include "../common.hpp"
#include "../def.hpp"
#include "../win_base.hpp"

namespace opflow::win {
template <typename T>
class session : public win_base<T> {
  using base = win_base<T>;

public:
  using typename base::data_type;
  using typename base::spec_type;

  explicit session(data_type session_gap) noexcept : session_gap(session_gap), init(), count() {}

  bool on_data(data_type tick, data_type const *) noexcept override {
    if (!init) {
      last_tick = tick;
      init = true;
    }

    // append data to window
    ++count;

    if (tick - last_tick < session_gap) {
      return false; // No window emitted
    }

    emitting.timestamp = last_tick;
    emitting.offset = 0;
    emitting.size = count - 1;      // Latest tick does not belong to emitted session (right open)
    emitting.evict = emitting.size; // Tumbling
    last_tick = tick;
    return true; // Emit the current window
  }

  bool flush() noexcept override {
    // If no data points have been accumulated, do not emit
    if (count == 0) {
      return false;
    }
    // Force emission of the current window
    emitting.timestamp = last_tick;
    emitting.offset = 0;
    emitting.size = count;
    emitting.evict = emitting.size;
    return true;
  }

  spec_type emit() noexcept override {
    count -= emitting.evict;
    return std::exchange(emitting, {});
  }

  OPFLOW_CLONEABLE(session)

private:
  data_type const session_gap; ///< Minimum gap to start a new session
  data_type last_tick;         ///< Last tick seen
  u32 count;                   ///< Current number of elements in the session
  spec_type emitting;          ///< Session specification to emit
  bool init;
};
} // namespace opflow::win
