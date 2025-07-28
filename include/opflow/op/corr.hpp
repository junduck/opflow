#pragma once

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/binary.hpp"

#ifndef NDEBUG
namespace opflow::op {
template <std::floating_point T>
T corr_naive(std::span<T const> data) {
  // Naive Pearson correlation (auto-correlation, lag-1)
  size_t n = data.size();
  if (n < 2)
    return T{0};
  T mean = T{0};
  for (T v : data)
    mean += v;
  mean /= static_cast<T>(n);
  T var = T{0};
  for (T v : data)
    var += (v - mean) * (v - mean);
  var /= static_cast<T>(n - 1);
  T cov = T{0};
  for (size_t i = 1; i < n; ++i)
    cov += (data[i - 1] - mean) * (data[i] - mean);
  cov /= static_cast<T>(n - 1);
  if (var == T{0})
    return T{0};
  return cov / var;
}
} // namespace opflow::op
#endif

namespace opflow::op {
template <typename T, std::floating_point U>
struct corr : public detail::binary_op<T, U> {
  using base = detail::binary_op<T, U>;
  using base::pos0;
  using base::pos1;

  detail::smooth<U> mx; ///< mean of first input
  detail::smooth<U> my; ///< mean of second input
  detail::accum<U> mxy; ///< m2 of cross product
  detail::accum<U> m2x; ///< m2 of first input
  detail::accum<U> m2y; ///< m2 of second input
  size_t n;             ///< count of values processed

  explicit corr(size_t pos0 = 0, size_t pos1 = 1) noexcept : base{pos0, pos1}, mx{}, my{}, mxy{}, m2x{}, m2y{}, n{} {}

  void init(T, U const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    U const x = in[0][pos0];
    U const y = in[1][pos1];

    n = 1;
    mx = x;   // Initialize with the first value
    my = y;   // Initialize with the first value
    m2x = 0.; // Second moment of first input starts at zero
    m2y = 0.; // Second moment of second input starts at zero
    mxy = 0.; // Cross product starts at zero
  }

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    U const x = in[0][pos0];
    U const y = in[1][pos1];

    ++n;
    U const a = 1.0 / n;
    U const dx = x - mx;
    U const dy = y - my;
    mx.add(x, a);
    my.add(y, a);
    m2x.add((x - mx) * dx);
    m2y.add((y - my) * dy);
    mxy.add((x - mx) * dy);
  }

  void inverse(T, U const *const rm) noexcept override {
    assert(rm && rm[0] && rm[1] && "NULL removal data.");
    U const x = rm[0][pos0];
    U const y = rm[1][pos1];

    --n;
    U const a = 1.0 / n;
    U const dx = x - mx;
    U const dy = y - my;
    mx.sub(x, a);
    my.sub(y, a);
    m2x.sub((x - mx) * dx);
    m2y.sub((y - my) * dy);
    mxy.sub((x - mx) * dy);
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    assert(this->n > 0 && "value called with empty state.");

    out[0] = mx; // mean of first input
    out[1] = my; // mean of second input
    if (n == 1) [[unlikely]] {
      // cov/corr is zero if only one sample
      out[2] = 0.;
      out[3] = 0.;
    } else {
      out[2] = mxy / (n - 1); // unbiased covariance
      U const denom = std::sqrt<U>(m2x * m2y);
      if (denom == 0.) {
        out[3] = 0.; // avoid division by zero
      } else {
        out[3] = mxy / denom; // Pearson correlation coefficient
      }
    }
  }

  size_t num_outputs() const noexcept override {
    // mx, my, cov, corr
    return 4;
  }
};
} // namespace opflow::op
