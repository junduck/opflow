#pragma once

#include "var.hpp"
#include "var_ew.hpp"

namespace opflow::op {
template <typename T>
class zscore : public stddev<T> {
public:
  using base = stddev<T>;
  using typename base::data_type;

  using base::base;

  void value(data_type *out) const noexcept override {
    base::value(out);
    if (out[1] != data_type{}) {
      out[1] = (out[0] - out[1]) / out[1]; // z-score calculation
    }
  }
};

template <typename T>
class zscore_ew : public std_ew<T> {
public:
  using base = std_ew<T>;
  using typename base::data_type;

  using base::base;

  void value(data_type *out) const noexcept override {
    base::value(out);
    if (out[1] != data_type{}) {
      out[1] = (out[0] - out[1]) / out[1]; // z-score calculation
    }
  }
};

// TODO: some patterns may have cyclic dependencies
/*
      zscore <-------------------------------|
        |                                    |
        |--------|                           |
                 |                           |
in -> if (in > zscore * thres) -> smooth(in)-|
                              |              |
                              |-> in---------|
 */

/**
 * @brief Adaptive z-score band
 *
 * @details This operator is based on Robust peak detection algorithm (using z-scores) some changes are made to the
 * original algorithm:
 * - Original algorithm uses a sliding window to calculate z-scores, and the z-score is dependent on current input and
 * threshold, which creates a cyclic dependency. We employs an exponential weighted zscore to overcome this issue.
 * - Due to the use of exponential smoothing, new input has larger weight than equal weighted sliding window, and a
 * "shorter" memory on history. Break even point is ~37% of the window.
 *
 * @todo Original algorithm can be implemented by maintaining a local ring buffer for all input
 *
 * @ref https://stackoverflow.com/a/22640362/17778516
 */
template <typename T>
class zband : public op_base<T> {
public:
  using base = op_base<T>;
  using typename base::data_type;

  zband(data_type alpha, data_type thres, data_type influence) noexcept
      : m(), s2(), initialised(), lagged(), stddev(), alpha(detail::smooth_factor(alpha)), thres(thres),
        influence(influence) {}

  void on_data(data_type const *in) noexcept override {
    data_type x = in[0];

    if (!initialised) [[unlikely]] {
      m = x;
      lagged = x;
      stddev = fmax<data_type>;
      initialised = true;
      return;
    }

    if (std::abs(x - m) > thres * stddev) {
      // adjust input based on z-score
      x = std::lerp(lagged, x, influence);
    }

    data_type const d = x - m;
    m.add(x, alpha);
    data_type const d2 = x - m;
    s2.add(d * d2, alpha);
    lagged = x;
    stddev = std::sqrt(s2);
  }

  void value(data_type *out) const noexcept override {
    out[0] = m;
    out[1] = stddev;
  }

  OPFLOW_INOUT(1, 2)
  OPFLOW_CLONEABLE(zband)

private:
  detail::smooth<data_type> m;  ///< mean
  detail::smooth<data_type> s2; ///< variance
  bool initialised;             ///< whether the first value has been processed

  data_type lagged;
  data_type stddev;
  data_type const alpha;
  data_type const thres;
  data_type const influence;
};
} // namespace opflow::op
