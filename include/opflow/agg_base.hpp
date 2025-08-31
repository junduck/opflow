#pragma once

#include <cstddef>

namespace opflow {
/**
 * @brief Base class for aggregators
 *
 * Instruction for coding agent:
 *
 * 1. Aggregates a window of data (n rows) into a single output row.
 * 2. Input: in[columns][rows], i.e., in[c][r] is the r-th row of column c.
 * 3. Output: out[output_columns]
 * 4. Aggregator does NOT select columns; required columns are specified at registration and passed correctly.
 * 5. n is guaranteed to be greater than 0.
 * 6. Aggregators are not required to be thread-safe.
 * 7. Aggregators are not required to perform pointer checks.
 * 8. Exec engine guarantees non-aliased in and out pointers.
 * @see opflow::agg::ohlc for a reference implementation.
 * @see opflow::agg::sum for a reference implementation.
 *
 */
template <typename Data>
struct agg_base {
  using data_type = Data;

  /**
   * @brief Process current aggregation window and write to output
   *
   * @param n number of elements (rows) in the input
   * @param in pointer to the input data, index dimension: (col, row)
   * @param out pointer to the output data, index dimension: (col)
   */
  virtual void on_data(size_t n, data_type const *const *in, data_type *out) noexcept = 0;
  virtual void reset() noexcept {}

  virtual size_t num_inputs() const noexcept = 0;
  virtual size_t num_outputs() const noexcept = 0;

  virtual agg_base *clone_at(void *mem) const noexcept = 0;
  virtual size_t clone_size() const noexcept = 0;
  virtual size_t clone_align() const noexcept = 0;

  virtual ~agg_base() noexcept = default;
};
} // namespace opflow
