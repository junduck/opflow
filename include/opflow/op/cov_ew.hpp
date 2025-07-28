#pragma once

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/binary.hpp"

namespace opflow::op {
template <typename T, std::floating_point U>
struct cov_ew : public detail::binary_op<T, U> {
  using base = detail::binary_op<T, U>;
  using base::pos0;
  using base::pos1;

  detail::smooth<U> mx;   ///< mean of first input
  detail::smooth<U> my;   ///< mean of second input
  detail::smooth<U> s2xy; ///< cov
  U alpha;                ///< smoothing factor
  bool initialised;       ///< whether the first value has been processed

  explicit cov_ew(U alpha, size_t pos0 = 0, size_t pos1 = 1) noexcept
      : base{pos0, pos1}, mx{}, my{}, s2xy{}, alpha{alpha}, initialised{false} {
    assert(alpha > 0. && "alpha/period must be positive.");
    if (alpha >= 1.) {
      // alpha is actually a period
      this->alpha = 2.0 / (alpha + 1);
    }
  }

  void init(T, U const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    U const x = in[0][pos0];
    U const y = in[1][pos1];

    mx = x;             // Initialize with the first value
    my = y;             // Initialize with the first value
    s2xy = 0.;          // Cross product starts at zero
    initialised = true; // Mark as initialized
  }

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    U const x = in[0][pos0];
    U const y = in[1][pos1];

    if (!initialised) [[unlikely]] {
      mx = x; // Initialize with the first value
      my = y;
      initialised = true;
      return;
    }

    U const dx = x - mx;
    U const dy = y - my;
    mx.add(x, alpha);
    my.add(y, alpha);
    s2xy.add((1.0 - alpha) * dx * dy, alpha);
  }

  void value(U *out) noexcept override {
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
