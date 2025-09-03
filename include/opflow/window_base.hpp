#pragma once

#include <cstddef>

namespace opflow {

/**
 * @brief Window spec
 *
 * @tparam Time
 */
template <typename Time>
struct window_spec {
  Time timestamp; ///< Timestamp associated with this window
  size_t offset;  ///< Offset of the window in the input data
  size_t size;    ///< Size of the window in data points
  size_t evict;   ///< Number of data points to evict from queue after window aggregation
};

/**
 * @brief Base class for window emitters
 *
 * Instruction for coding agent:
 *
 * 1. Window emitter signals when a window is emitted for an aggregator.
 * 2. Window emitter itself does not store and process data points.
 * 3. Aggregator will call on_data() to determine if a window should be emitted.
 * 4. Aggregator will call emit() if a window is emitted.
 * 5. Aggregator will collect data points and perform aggregation, this is NOT the concern of a window emitter.
 * 6. Data points in the window are emitted and evicted FIFO. Following table illustrates an example.
 *
 * | queue      | process | emit          | window   | note                                      |
 * |------------|---------|---------------|----------|-------------------------------------------|
 * | 0          | false   | N/A           | N/A      | no window                                 |
 * | 0,1        | false   | N/A           | N/A      | no window                                 |
 * | 0,1,2      | false   | N/A           | N/A      | no window                                 |
 * | 0,1,2,3    | true    | [T0, 0, 3, 2] | 0,1,2    | offset 0, size 3, evict/pop 2 from queue  |
 * | 2,3,4      | false   | N/A           | N/A      | no window, 0,1 evicted from queue         |
 * | 2,3,4,5    | true    | [T1, 1, 3, 4] | 3,4,5    | offset 1, size 3, evict/pop 4 from queue  |
 * | 6          | false   | N/A           | N/A      | no window, 2,3,4,5 evicted from queue     |
 *
 * Note that the queue and window are maintained by aggregator and is for exposition only.
 *
 * @see opflow::win::tumbling for a reference implementation.
 * @see opflow::win::cusum_filter for a reference implementation that inspects input data.
 *
 */
template <typename Data>
struct window_base {
  using data_type = Data;
  using spec_type = window_spec<data_type>;

  /**
   * @brief Process a new data point
   *
   * @param t   The timestamp of the incoming data point.
   * @param in  Pointer to the incoming data point. May not be used if impl does not inspect data.
   * @return true if a window is emitted, false otherwise.
   * @note  This method is called for each incoming data point.
   */
  virtual bool on_data(data_type t, data_type const *in) noexcept = 0;

  /**
   * @brief Force emission of the current window, if any.
   *
   * This method is called to force a window emission (e.g., on timeout).
   * If a window is available to emit, returns true; otherwise, returns false.
   * After returning true, emit() should be called to get the window specification.
   *
   * @return true if a window is emitted, false otherwise.
   */
  virtual bool flush() noexcept = 0;

  /**
   * @brief Get current window specification
   *
   * This method is called only after on_data() returns true.
   *
   * @return spec_type
   */
  virtual spec_type emit() noexcept = 0;

  /**
   * @brief Reset the internal state of the window.
   *
   * Called when the transformer is re-initialised or cleared.
   *
   */
  virtual void reset() noexcept = 0;

  virtual window_base *clone_at(void *mem) const noexcept = 0;
  virtual size_t clone_size() const noexcept = 0;
  virtual size_t clone_align() const noexcept = 0;

  virtual ~window_base() noexcept = default;
};

} // namespace opflow
