#pragma once

#include <algorithm>
#include <cmath>

#include "opflow/op/detail/unary.hpp"
#include "opflow/op_base.hpp"

namespace opflow::op {

// OHLC tumbling window operator

/**
 * @brief OHLC (Open, High, Low, Close) operator for a tumbling window.
 *
 * This operator computes OHLC in the [start, end) range of the tumbling window. i.e. it reports
 * first, max, min and last values in the half-open interval.
 *
 * @warning Unlike in most financial charting, tick on end boundary is not included in the window.
 * Therefore last close != this open even if the tick is exactly at the boundary. This design is to
 * maintain consistency with accumulative measurements within the window.
 *
 * @tparam T
 */
template <typename T, std::floating_point U>
struct ohlc : public detail::unary_op<T, U> {
  using base = detail::unary_op<T, U>;
  using base::pos;

  duration_t<T> window_size; ///< Size of the tumbling window
  T next_tick;               ///< Next tick time point for the tumbling window
  U open;                    ///< Open price
  U high;                    ///< High price
  U low;                     ///< Low price
  U close;                   ///< Close price

  std::array<U, 4> output_data; ///< Output data buffer for OHLC values

  // here we assume that a tumbling window is always aligned to epoch
  explicit ohlc(duration_t<T> window, size_t pos = 0) noexcept
      : base{pos}, window_size{window}, next_tick{}, open{}, high{}, low{}, close{} {
    output_data.fill(fnan<U>);
  }

  T align_to_window(T tick) const noexcept {
    if constexpr (std::is_integral_v<T>) {
      auto remainder = tick % window_size;
      if (remainder == 0)
        return tick; // Already aligned
      else
        return tick - remainder + window_size; // Align to the next window start
    } else if constexpr (std::is_floating_point_v<T>) {
      auto remainder = std::fmod(tick, window_size);
      if (remainder == 0.0)
        return tick; // Already aligned
      else
        return tick - remainder + window_size; // Align to the next window start
    } else {
      // TODO: this may be very inefficient
      // our typename T only requires comparison and basic arithmetic
      T t{};
      while (t < tick) {
        t += window_size;
      }
      return t; // Align to the next window start
    }
  }

  void step(T tick, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    U x = in[0][pos];

    // Case 1: Initialization state - next_tick is T{} (default constructed)
    if (next_tick == T{}) {
      next_tick = align_to_window(tick);
      open = high = low = close = x;
    }

    // Case 2: Still in the same window (tick < next_tick)
    if (tick < next_tick) {
      high = std::max(high, x);
      low = std::min(low, x);
      close = x;
      return;
    }

    // Cases 3: tick >= next_tick - we've reached or moved past the current window
    // First, emit the completed OHLC for the previous window
    output_data[0] = open;
    output_data[1] = high;
    output_data[2] = low;
    output_data[3] = close;

    // Handle potential gap in data (sparse data case)
    // Find the correct window that contains the current tick
    while (tick >= next_tick) {
      next_tick += window_size;
    }
    // After loop: tick is in range [next_tick - window_size, next_tick)

    // Initialize OHLC values for the new window
    open = high = low = close = x;
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    std::copy(output_data.begin(), output_data.end(), out);
    // Reset output data after reading
    // where there is not enough data for a tumbling window, we return nans
    output_data.fill(fnan<U>);
  }
};
} // namespace opflow::op
