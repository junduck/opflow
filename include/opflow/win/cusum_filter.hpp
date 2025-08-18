#pragma once

#include "opflow/common.hpp"
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
template <arithmetic T>
struct cusum_filter : public window_base<T> {
  using base = window_base<T>;
  using typename base::data_type;
  using typename base::spec_type;

  data_type const thres;
  size_t const idx;

  data_type lagged_log;
  data_type cusum_pos, cusum_neg;
  spec_type curr;
  bool init;

  /**
   * @brief Construct a new cusum filter object
   *
   * @param log_threshold log threshold for an event
   * @param inspect_index index of the data point to calculate log difference
   */
  cusum_filter(data_type log_threshold, size_t inspect_index)
      : thres(log_threshold), idx(inspect_index), lagged_log(), cusum_pos(), cusum_neg(), curr(), init() {}

  bool process(data_type time, data_type const *in) noexcept override {
    auto curr_log = std::log(in[idx]);
    curr.timestamp = time;
    ++curr.size;
    if (!init) {
      lagged_log = curr_log;
      init = true;
      return false;
    }

    auto gain = curr_log - lagged_log;
    lagged_log = curr_log;
    cusum_pos = std::max(data_type{}, cusum_pos + gain);
    cusum_neg = std::min(data_type{}, cusum_neg + gain);
    return (cusum_pos > thres || cusum_neg < -thres);
  }

  bool flush() noexcept override {
    if (curr.size == 0) {
      return false;
    }
    return true;
  }

  spec_type emit() noexcept override {
    cusum_pos = data_type{};
    cusum_neg = data_type{};
    curr.evict = curr.size;
    return std::exchange(curr, {});
  }

  void reset() noexcept override {
    *this = cusum_filter(thres, idx); // Reset to a new instance with the same parameters
  }
};
} // namespace opflow::win
