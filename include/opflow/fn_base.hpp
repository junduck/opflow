#pragma once

#include <cstddef>

#include "common.hpp"
#include "def.hpp"

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
 * - If the functor is stateful and implements reset() method, it can be used as an tumbling aggregator.
 *    - on_data() is called on arrival.
 *    - reset() is called when the window is closed.
 * - Number of input elements is given by num_inputs().
 * - Number of output elements is given by num_outputs().
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

  virtual fn_base *clone_at(void *mem) const noexcept = 0;
  virtual size_t clone_size() const noexcept = 0;
  virtual size_t clone_align() const noexcept = 0;

  virtual ~fn_base() noexcept = default;
};

template <typename T>
struct fn_root : fn_base<T> {
  using base = fn_base<T>;
  using typename base::data_type;

  size_t const input_size;

  explicit fn_root(size_t n) : input_size(n) {}

  void on_data(data_type const *in, data_type *out) noexcept override {
    data_type *OPFLOW_RESTRICT cast = out;
    for (size_t i = 0; i < input_size; ++i) {
      cast[i] = in[i];
    }
  }

  OPFLOW_INOUT(input_size, input_size)
  OPFLOW_CLONEABLE(fn_root)
};

template <typename T>
struct dag_root<fn_base<T>> {
  using type = fn_root<T>;
};
} // namespace opflow
