#pragma once

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/binary.hpp"

namespace opflow::op {
template <typename T>
struct cov : public detail::binary_op<T> {
  using base = detail::binary_op<T>;
  using base::pos0;
  using base::pos1;

  detail::smooth mx; ///< mean of first input
  detail::smooth my; ///< mean of second input
  detail::accum mxy; ///< m2 of cross product
  size_t n;          ///< count of values processed

  explicit cov(size_t pos0 = 0, size_t pos1 = 1) noexcept : base{pos0, pos1}, mx{}, my{}, mxy{}, n{} {}

  void init(T, double const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    double const x = in[0][pos0];
    double const y = in[1][pos1];

    n = 1;
    mx = x;   // Initialize with the first value
    my = y;   // Initialize with the first value
    mxy = 0.; // Cross product starts at zero
  }

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    double const x = in[0][pos0];
    double const y = in[1][pos1];

    ++n;
    double const a = 1.0 / n;
    double const dy = y - my;
    mx.add(x, a);
    my.add(y, a);
    mxy.add((x - mx) * dy);
  }

  void inverse(T, double const *const rm) noexcept override {
    assert(rm && rm[0] && rm[1] && "NULL removal data.");
    double const x = rm[0][pos0];
    double const y = rm[1][pos1];

    --n;
    double const a = 1.0 / n;
    double const dy = y - my;
    mx.sub(x, a);
    my.sub(y, a);
    mxy.sub((x - mx) * dy);
  }

  // Currently we can not utilise rolling update due to engine design
  void roll(double x, double x0, double y, double y0) noexcept {
    double const dy = y - my;
    double const dy0 = y0 - my;
    double const a = 1.0 / n;
    mx.addsub(x, x0, a);
    my.addsub(y, y0, a);
    mxy.addsub((x - mx) * dy, (x0 - mx) * dy0);
  }

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");
    assert(this->n > 0 && "value called with empty state.");

    out[0] = mx; // mean of first input
    out[1] = my; // mean of second input
    if (n == 1) [[unlikely]] {
      out[2] = 0.; // covariance is zero if only one sample
    } else {
      out[2] = mxy / (n - 1); // unbiased covariance
    }
  }

  size_t num_outputs() const noexcept override {
    return 3; // mx, my, cov
  }
};
} // namespace opflow::op
