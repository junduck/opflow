#pragma once

#include <span>

#include "opflow/agg_base.hpp"
#include "opflow/def.hpp"

namespace opflow::agg {
struct order_flow : public agg_base<double> {
  using data_type = double;

  void on_data(size_t n, data_type const *const *in, data_type *out) noexcept override {
    std::span<data_type const> buy_vol(in[0], n);
    std::span<data_type const> sell_vol(in[1], n);

    data_type buy{};
    data_type sell{};

    for (size_t i = 0; i < n; ++i) {
      buy += buy_vol[i];
      sell += sell_vol[i];
    }

    out[0] = buy - sell; // Net order flow
  }

  OPFLOW_INOUT(2, 1)
  OPFLOW_CLONEABLE(order_flow)
};
} // namespace opflow::agg
