#pragma once

#include "common.hpp"

namespace opflow {
/**
 * @brief Base class for window emitters
 *
 */
template <typename Time, typename Data>
struct window_base {
  using time_type = Time;
  using dura_type = duration_t<time_type>;
  using data_type = Data;

  /**
   * @brief Process a new data point
   *
   * @param t   The timestamp of the incoming data point.
   * @param in  Pointer to the incoming data point.
   * @return true if a window should be emitted after this data point.
   * @note  This method is called for each incoming data point.
   */
  virtual bool process(time_type t, data_type const *in) noexcept = 0;

  /**
   * @brief Evict data points from window after a window is emitted.
   *
   * This method is called only after process() returns true. Transformer will evict
   * oldest elements from the window according to return value.
   *
   * @return size_t number of data point to evict from window.
   *                - Return 0 for cumulative window (no eviction)
   *                - Return N for tumbling window (evict all N elements in window)
   *                - Return 0 < K < N for sliding window (evict K elements in window)
   */
  virtual size_t evict() noexcept = 0;

  /**
   * @brief Reset the internal state of the window.
   *
   * Called when the transformer is re-initialised or cleared.
   *
   */
  virtual void reset() noexcept = 0;

  virtual ~window_base() noexcept = default;
};

} // namespace opflow
