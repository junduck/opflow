#pragma once

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/binary.hpp"
#include "opflow/op_base.hpp"

namespace opflow::op {
template <time_point_like T>
struct cov_ew : public detail::binary_op<T> {
  using base = detail::binary_op<T>;
  using base::pos0;
  using base::pos1;

  detail::smooth mx;   ///< mean of first input
  detail::smooth my;   ///< mean of second input
  detail::smooth s2xy; ///< cov
  double alpha;        ///< smoothing factor
  bool initialised;    ///< whether the first value has been processed

  explicit cov_ew(double alpha, size_t pos0 = 0, size_t pos1 = 1) noexcept
      : base{pos0, pos1}, mx{}, my{}, s2xy{}, alpha{alpha}, initialised{false} {
    assert(alpha > 0. && "alpha/period must be positive.");
    if (alpha >= 1.) {
      // alpha is actually a period
      this->alpha = 2.0 / (alpha + 1);
    }
  }

  void init(T, double const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    double const x = in[0][pos0];
    double const y = in[1][pos1];

    mx = x;             // Initialize with the first value
    my = y;             // Initialize with the first value
    s2xy = 0.;          // Cross product starts at zero
    initialised = true; // Mark as initialized
  }

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    double const x = in[0][pos0];
    double const y = in[1][pos1];

    if (!initialised) [[unlikely]] {
      mx = x; // Initialize with the first value
      my = y;
      initialised = true;
      return;
    }

    double const dx = x - mx;
    double const dy = y - my;
    mx.add(x, alpha);
    my.add(y, alpha);
    s2xy.add((1.0 - alpha) * dx * dy, alpha);
  }

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");
    assert(initialised && "value called with empty state.");

    out[0] = mx;   // mean of first input
    out[1] = my;   // mean of second input
    out[2] = s2xy; // covariance
  }

  size_t num_outputs() const noexcept override {
    return 3; // mx, my, cov
  }
};
} // namespace opflow::op
