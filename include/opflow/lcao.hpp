#pragma once

#include <span>
#include <vector>

namespace opflow {
template <typename T, typename U>
struct lincomb {
  using time_type = T;
  using data_type = U;

  std::vector<data_type> coeffs; ///< Coefficients for linear combination
  std::vector<data_type> values; ///< Values for linear combination
  std::vector<bool> barrier;     ///< Barrier flags for each coefficient
  time_type timestamp;           ///< Timestamp for the linear combination
  bool init;

  lincomb(std::span<data_type const> coefs)
      : coeffs(coefs.begin(), coefs.end()), values(coeffs.size(), data_type{}), barrier(coeffs.size(), false),
        timestamp(), init(false) {}

  bool on_data(time_type t, size_t i, data_type value) {
    if (!init) {
      barrier[i] = true; // Set barrier for the first data point
      if (std::all_of(barrier.begin(), barrier.end(), [](bool b) { return b; })) {
        init = true;
      }
    }
    timestamp = std::max(timestamp, t);
    values[i] = value;

    return init;
  }

  data_type value() const {
    data_type result = data_type{};
    for (size_t i = 0; i < coeffs.size(); ++i) {
      result += coeffs[i] * values[i];
    }
    return result;
  }
};
} // namespace opflow
