#pragma once

#include "opflow/window_base.hpp"

namespace opflow::win {
/**
 * @brief Revised CUSUM filter window
 *
 * This class implements a revised CUSUM filter for change point detection. It emits tumbling windows on cumulated *log*
 * change of input data that exceeds the log threshold. Both positive and negative cusums are reset after each window
 * emission to ensure non-overlapping event windows.
 *
 * Note: This is a practical adaptation of the CUSUM filter inspired by López de Prado's original CUSUM filter resets
 * only the breached sum; this version resets both for windowing purposes.
 *
 * @ref https://en.wikipedia.org/wiki/CUSUM
 * @ref López de Prado, M. (2018). Advances in Financial Machine Learning
 */
template <typename Time, std::floating_point Data>
struct cusum_filter : window_base<Time, Data> {
  using base = window_base<Time, Data>;
  using typename base::data_type;
  using typename base::time_type;

  data_type thres;
  data_type lagged_log;
  data_type cusum_pos, cusum_neg;
  size_t n;
  size_t idx;

  /**
   * @brief Construct a new cusum filter object
   *
   * @param log_threshold log threshold for an event
   * @param inspect_index index of the data point to calculate log difference
   */
  cusum_filter(data_type log_threshold, size_t inspect_index)
      : thres(log_threshold), lagged_log(0), cusum_pos(0), cusum_neg(0), n(0), idx(inspect_index) {}

  bool process(time_type, data_type const *in) noexcept override {
    auto curr_log = std::log(in[idx]);
    if (!n++) {
      lagged_log = curr_log;
      return false;
    }

    data_type gain = curr_log - lagged_log;
    lagged_log = curr_log;
    cusum_pos = std::max(data_type{}, cusum_pos + gain);
    cusum_neg = std::min(data_type{}, cusum_neg + gain);
    return (cusum_pos > thres || cusum_neg < -thres);
  }

  size_t evict() noexcept override {
    cusum_pos = data_type{};
    cusum_neg = data_type{};
    return std::exchange(n, 0);
  }

  ~cusum_filter() noexcept override = default;
};
} // namespace opflow::win
