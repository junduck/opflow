#pragma once

#include <cstddef>

namespace opflow {
/**
 * @brief Base class for online functors
 *
 * Instruction for coding agent:
 *
 * - This is the base class for all online (streaming) algorithm functors.
 * - When new data is available, on_data() is called with:
 *    - an array of T to read: T const* in.
 *    - an array of T to write: T* out.
 *    - engine guarantees valid pointers and correct array sizes.
 *    - engine guarantees non-aliased in and out pointers.
 * - Number of input elements is given by num_inputs().
 * - Number of output elements is given by num_outputs().
 * - reset() restores the functor to its initial state.
 * - The execution engine guarantees valid pointers and correct array sizes.
 *
 * @tparam T data type
 */
template <typename T>
struct fn_base {
  using data_type = T;

  virtual void on_data(data_type const *in, data_type *out) noexcept = 0;
  virtual void reset() noexcept {}

  virtual size_t num_inputs() const noexcept = 0;
  virtual size_t num_outputs() const noexcept = 0;

  fn_base const *observer() const noexcept { return this; }

  virtual ~fn_base() noexcept = default;
};
} // namespace opflow
