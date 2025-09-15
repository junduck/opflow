#pragma once

#include <cmath>
#include <numeric>
#include <span>

#include "../agg_base.hpp"
#include "../def.hpp"

namespace opflow::agg {
template <typename Data>
struct stddev : public agg_base<Data> {
  using data_type = Data;

  // Number of input columns (set at construction)
  size_t const input_cols;
  size_t const ddof;

  // Constructor: specify number of input columns (computes stddev for each column independently)
  explicit stddev(size_t num_columns, size_t degrees_of_freedom = 1)
      : input_cols(num_columns), ddof(degrees_of_freedom) {}

  void on_data(size_t n, data_type const *const *in, data_type *out) noexcept override {
    for (size_t i = 0; i < input_cols; ++i) {
      if (n == 1) {
        out[i] = data_type{0};
        continue;
      }

      std::span<data_type const> col(in[i], n);
      // Compute mean
      auto mean = std::accumulate(col.begin(), col.end(), data_type{}) / static_cast<data_type>(n);

      // Compute variance
      data_type sum_sq_diff{};
      for (auto v : col) {
        data_type diff = v - mean;
        sum_sq_diff += diff * diff;
      }
      data_type variance = sum_sq_diff / static_cast<data_type>(n - ddof);

      // Standard deviation
      out[i] = std::sqrt(variance);
    }
  }

  OPFLOW_INOUT(input_cols, input_cols)
  OPFLOW_CLONEABLE(stddev)
};
} // namespace opflow::agg
