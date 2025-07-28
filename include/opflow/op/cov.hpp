#pragma once

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/binary.hpp"

#ifndef NDEBUG
namespace opflow::op {
template <std::floating_point T>
T cov_naive(std::span<T const> data) {
  // Naive covariance (auto-covariance, lag-1)
  size_t n = data.size();
  if (n < 2)
    return T{0};
  T mean = T{0};
  for (T v : data)
    mean += v;
  mean /= static_cast<T>(n);
  T cov = T{0};
  for (size_t i = 1; i < n; ++i)
    cov += (data[i - 1] - mean) * (data[i] - mean);
  cov /= static_cast<T>(n - 1);
  return cov;
}
} // namespace opflow::op
#endif

namespace opflow::op {
template <typename T, std::floating_point U>
struct cov : public detail::binary_op<T, U> {
  using base = detail::binary_op<T, U>;
  using base::pos0;
  using base::pos1;

  detail::smooth<U> mx; ///< mean of first input
  detail::smooth<U> my; ///< mean of second input
  detail::accum<U> mxy; ///< m2 of cross product
  size_t n;             ///< count of values processed

  explicit cov(size_t pos0 = 0, size_t pos1 = 1) noexcept : base{pos0, pos1}, mx{}, my{}, mxy{}, n{} {}

  void init(T, U const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    U const x = in[0][pos0];
    U const y = in[1][pos1];

    n = 1;
    mx = x;   // Initialize with the first value
    my = y;   // Initialize with the first value
    mxy = 0.; // Cross product starts at zero
  }

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    U const x = in[0][pos0];
    U const y = in[1][pos1];

    ++n;
    U const a = 1.0 / n;
    U const dy = y - my;
    mx.add(x, a);
    my.add(y, a);
    mxy.add((x - mx) * dy);
  }

  void inverse(T, U const *const rm) noexcept override {
    assert(rm && rm[0] && rm[1] && "NULL removal data.");
    U const x = rm[0][pos0];
    U const y = rm[1][pos1];

    --n;
    U const a = 1.0 / n;
    U const dy = y - my;
    mx.sub(x, a);
    my.sub(y, a);
    mxy.sub((x - mx) * dy);
  }

  // Currently we can not utilise rolling update due to engine design
  void roll(U x, U x0, U y, U y0) noexcept {
    U const dy = y - my;
    U const dy0 = y0 - my;
    U const a = 1.0 / n;
    mx.addsub(x, x0, a);
    my.addsub(y, y0, a);
    mxy.addsub((x - mx) * dy, (x0 - mx) * dy0);
  }

  void value(U *out) noexcept override {
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
