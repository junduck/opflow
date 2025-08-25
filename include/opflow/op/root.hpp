#pragma once

#include "opflow/def.hpp"
#include "opflow/op_base.hpp"

namespace opflow::op {
/**
 * @brief Graph root node that copies input data to its output.
 *
 */
template <typename T>
struct graph_root : op_base<T> {
  using base = op_base<T>;
  using typename base::data_type;

  data_type const *mem;
  size_t const input_size;

  explicit graph_root(size_t n) : mem(nullptr), input_size(n) {}

  void on_data(data_type const *in) noexcept override { mem = in; }
  void value(data_type *out) const noexcept override {
    data_type *OPFLOW_RESTRICT cast = out;
    for (size_t i = 0; i < input_size; ++i) {
      cast[i] = mem[i];
    }
  }
  void reset() noexcept override {}

  size_t num_inputs() const noexcept override { return input_size; }
  size_t num_outputs() const noexcept override { return input_size; }

  OPFLOW_CLONEABLE(graph_root)
};
} // namespace opflow::op
