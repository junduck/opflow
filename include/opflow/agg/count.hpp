#pragma once

#include "opflow/agg_base.hpp"

namespace opflow::agg {
template <typename Data>
struct count : public agg_base<Data> {
  using data_type = Data;

  void process(size_t n, data_type const *const *, data_type *out) noexcept override {
    *out = static_cast<data_type>(n);
  }

  size_t num_inputs() const noexcept override { return 1; }
  size_t num_outputs() const noexcept override { return 1; }
};
} // namespace opflow::agg
