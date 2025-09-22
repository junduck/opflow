#pragma once

#include <algorithm>
#include <cmath>

#include "../def.hpp"
#include "../tumble_base.hpp"

namespace opflow::tumble {
template <typename T>
class cusum : public tumble_base<T> {
  using base = tumble_base<T>;

public:
  using typename base::data_type;
  using typename base::spec_type;

  explicit cusum(data_type log_threshold) noexcept
      : thres(std::abs(log_threshold)), lagged_log(), cusum_pos(), cusum_neg(), emitting(), init() {}

  bool on_data(data_type timestamp, data_type const *in) noexcept override {
    auto curr_log = std::log(in[0]);
    if (!init) {
      lagged_log = curr_log;
      init = true;
      return false;
    }

    auto gain = curr_log - lagged_log;
    lagged_log = curr_log;
    cusum_pos = std::max(data_type{}, cusum_pos + gain);
    cusum_neg = std::min(data_type{}, cusum_neg + gain);
    if (cusum_pos > thres || cusum_neg < -thres) {
      emitting = timestamp;
      cusum_pos = data_type{};
      cusum_neg = data_type{};
      return true;
    }
    return false;
  }

  spec_type emit() noexcept override { return {.timestamp = emitting, .include = true}; }

  OPFLOW_CLONEABLE(cusum)

private:
  data_type const thres;
  data_type lagged_log;
  data_type cusum_pos, cusum_neg;
  data_type emitting;
  bool init;
};
} // namespace opflow::tumble
