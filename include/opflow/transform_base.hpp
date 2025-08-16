#pragma once

#include <cstddef>

namespace opflow {
/**
 * @brief Base class for all transform operations.
 *
 * Abstracts 1:1 transformation and N:1 transformation (aggregation)
 *
 * @tparam Data The data type used in the transform operation.
 */
template <typename Data>
struct transform_base {
  using data_type = Data;

  /**
   * @brief Process incoming data.
   *
   * @param t The time associated with the incoming data.
   * @param in The input data to process.
   * @return true if an output is ready to be produced, false otherwise.
   */
  virtual bool on_data(data_type t, data_type const *in) noexcept = 0;

  /**
   * @brief Flush the transform state.
   *
   * This function is called to flush any remaining output from the transform. Default implementation always returns
   * false this is standard behaviour for a streaming (1:1) transform.
   *
   * @return true if output was produced, false otherwise.
   */
  virtual bool flush() noexcept { return false; }

  /**
   * @brief Get the output value.
   *
   * This function is called to retrieve the output value after on_data() returns true.
   *
   * @param out The output buffer to fill.
   * @return The timestamp associated with the output.
   */
  virtual data_type value(data_type *out) const noexcept = 0;

  /**
   * @brief Reset the transform state.
   *
   * This function is called to reset the internal state of the transform.
   */
  virtual void reset() noexcept = 0;

  /**
   * @brief Get the size of input this transform expects.
   *
   * @return size_t The size of input.
   */
  virtual size_t num_inputs() const noexcept = 0;

  /**
   * @brief Get the size of output this transform produces.
   *
   * @return size_t The size of output.
   */
  virtual size_t num_outputs() const noexcept = 0;

  virtual ~transform_base() noexcept = default;

  /**
   * @brief Check if this transform can be chained after prev.
   *
   * @param prev The previous transform to check compatibility with.
   * @return true If the transforms are compatible.
   * @return false If the transforms are not compatible.
   */
  bool compatible(transform_base const *prev) const noexcept { return prev->num_outputs() == num_inputs(); }
};
} // namespace opflow
