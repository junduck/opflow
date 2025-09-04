#pragma once

#include <span>

#include "opflow/agg_base.hpp"
#include "opflow/def.hpp"

namespace opflow::agg {
struct book_imba : public agg_base<double> {
  using data_type = double;

  void on_data(size_t n, data_type const *const *in, data_type *out) noexcept override {
    std::span<data_type const> bid_size(in[0], n);
    std::span<data_type const> ask_size(in[1], n);

    data_type bid{};
    data_type ask{};

    for (size_t i = 0; i < n; ++i) {
      bid += bid_size[i];
      ask += ask_size[i];
    }

    out[0] = (bid - ask) / (bid + ask);
  }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(book_imba)
};
} // namespace opflow::agg
