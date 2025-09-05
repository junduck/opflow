#pragma once

#include <utility>

#include "opflow/detail/accum.hpp"

namespace opflow::op {
struct s2_exp_weighted_sta {
  detail::smooth<double> m{};          ///< mean
  detail::smooth<double> s2{};         ///< variance (only using u_prev)
  detail::smooth<double> s2_welford{}; ///< variance (welford's like, using u_prev and u_curr)
  double alpha;                        ///< smoothing factor
  bool init{};                         ///< whether the first value has been processed

  explicit s2_exp_weighted_sta(double alpha) noexcept : alpha{alpha} {}

  std::pair<double, double> step(double x) noexcept {
    if (!init) {
      m = x;
      init = true;
      return {0., 0.};
    }

    double const d = x - m;
    double const a1 = 1.0 - alpha;
    m.add(x, alpha);
    s2.add(a1 * d * d, alpha); // only using u_prev
    double const d2 = x - m;
    s2_welford.add(d * d2, alpha); // Welford's
    return {s2.value(), s2_welford.value()};
  }
};
} // namespace opflow::op
