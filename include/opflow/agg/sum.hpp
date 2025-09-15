#pragma once

#include <numeric>
#include <span>

#include "../agg_base.hpp"
#include "../def.hpp"

namespace opflow::agg {
template <typename Data>
struct sum : public agg_base<Data> {
  using data_type = Data;

  // Number of input columns (set at construction)
  size_t const input_cols;

  // Constructor: specify number of input columns (averages each column independently)
  explicit sum(size_t num_columns) : input_cols(num_columns) {}

  void on_data(size_t n, data_type const *const *in, data_type *out) noexcept override {
    for (size_t i = 0; i < input_cols; ++i) {
      std::span<data_type const> col(in[i], n);
      out[i] = std::accumulate(col.begin(), col.end(), data_type{});
    }
  }

  OPFLOW_INOUT(input_cols, input_cols)
  OPFLOW_CLONEABLE(sum)
};
} // namespace opflow::agg
