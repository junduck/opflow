#pragma once

#include <cstddef>

namespace opflow {
/**
 * @brief Base class for aggregators
 *
 * @note This is the base class for special batched N:1 aggregation in data transform stage, and impl should be
 * optimised for batched processing. For streaming processing is implemented by op_base.
 *
 */
template <typename Data>
struct agg_base {
  using data_type = Data;

  /**
   * @brief Process current aggregation window and write to output
   *
   * @param n number of elements (rows) in the input
   * @param in pointer to the input data, in[col][row]
   * @param out pointer to the output data
   */
  virtual void process(size_t n, data_type const *const *in, data_type *out) noexcept = 0;

  /**
   * @brief Get the number of input columns expected by the aggregator
   *
   */
  virtual size_t num_inputs() const noexcept = 0;

  /**
   * @brief Get the number of output columns produced by the aggregator
   *
   */
  virtual size_t num_outputs() const noexcept = 0;

  /**
   * @brief Reset the internal state of the aggregator.
   *
   */
  virtual void reset() noexcept {}

  virtual ~agg_base() noexcept = default;
};
} // namespace opflow
