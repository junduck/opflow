#pragma once

#include "opflow/agg_base.hpp"
#include "opflow/def.hpp"

namespace opflow::agg {
template <typename Data>
struct count : public agg_base<Data> {
  using data_type = Data;

  void on_data(size_t n, data_type const *const *, data_type *out) noexcept override {
    *out = static_cast<data_type>(n);
  }

  OPFLOW_INOUT(0, 1)
  OPFLOW_CLONEABLE(count)
};
} // namespace opflow::agg
