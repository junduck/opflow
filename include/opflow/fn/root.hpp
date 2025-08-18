#pragma once

#include "opflow/def.hpp"
#include "opflow/fn_base.hpp"

namespace opflow::fn {
template <typename T>
struct graph_root : fn_base<T> {
  using base = fn_base<T>;
  using typename base::data_type;

  size_t const input_size;

  explicit graph_root(size_t n) : input_size(n) {}

  void on_data(data_type const *in, data_type *out) noexcept override {
    data_type *OPFLOW_RESTRICT cast = out;
    for (size_t i = 0; i < input_size; ++i) {
      cast[i] = in[i];
    }
  }

  size_t num_inputs() const noexcept override { return input_size; }
  size_t num_outputs() const noexcept override { return input_size; }
};
} // namespace opflow::fn
