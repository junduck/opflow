#pragma once

#include <algorithm>
#include <span>

#include "opflow/agg_base.hpp"
#include "opflow/def.hpp"

namespace opflow::agg {
template <typename Data>
struct ohlc : public agg_base<Data> {
  using data_type = Data;

  void on_data(size_t n, data_type const *const *in, data_type *out) noexcept override {
    std::span<data_type const> input(in[0], n);
    auto const o = input.front();
    auto const h = *std::ranges::max_element(input);
    auto const l = *std::ranges::min_element(input);
    auto const c = input.back();

    out[0] = o;
    out[1] = h;
    out[2] = l;
    out[3] = c;
  }

  OPFLOW_INOUT(1, 4)
  OPFLOW_CLONEABLE(ohlc)
};
} // namespace opflow::agg
