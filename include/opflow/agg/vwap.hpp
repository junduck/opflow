#pragma once

#include <span>

#include "opflow/agg_base.hpp"
#include "opflow/common.hpp"
#include "opflow/def.hpp"

namespace opflow::agg {
template <typename T>
struct vwap : public agg_base<T> {
  using data_type = T;

  void on_data(size_t n, data_type const *const *in, data_type *out) noexcept override {
    std::span<data_type const> price(in[0], n);
    std::span<data_type const> volume(in[1], n);

    data_type tnvr = 0;
    data_type vol = 0;

    for (size_t i = 0; i < n; ++i) {
      tnvr += price[i] * volume[i];
      vol += volume[i];
    }

    if (very_small(vol)) {
      out[0] = 0;
    } else {
      out[0] = tnvr / vol;
    }
  }

  OPFLOW_INOUT(2, 1)
  OPFLOW_CLONEABLE(vwap)
};
} // namespace opflow::agg
