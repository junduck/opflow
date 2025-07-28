#pragma once

#include <cmath>

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/unary.hpp"

#ifndef NDEBUG
namespace opflow::op {
template <std::floating_point T>
T ewma_naive(std::span<T const> data, T alpha) {
  // weighted averages are calculated using weights
  // (1-alpha)**(n-1), (1-alpha)**(n-2), ..., 1-alpha, 1
  size_t n = data.size();
  if (n == 0)
    return T{0};
  T a1 = T{1} - alpha;
  T weighted_sum = T{0};
  T total_weight = T{0};
  for (size_t i = 0; i < n; ++i) {
    T w = std::pow(a1, n - i - 1);
    weighted_sum += data[i] * w;
    total_weight += w;
  }
  return weighted_sum / total_weight;
}
} // namespace opflow::op
#endif

namespace opflow::op {
template <typename T, std::floating_point U>
struct ewma : public detail::unary_op<T, U> {
  using base = detail::unary_op<T, U>;
  using base::pos;

  detail::accum<U> total_weight; ///< total weight (a1^n)
  U a1;                          ///< 1. - alpha
  U a1_n;                        ///< (1. - alpha)^n
  U s;                           ///< current weighted sum

  explicit ewma(U alpha, size_t pos = 0) noexcept : base{pos}, total_weight{}, a1{1. - alpha}, a1_n{1.}, s{} {
    assert(alpha > 0. && "alpha/period must be positive.");
    if (alpha >= 1.) {
      alpha = 2.0 / (alpha + 1);
      a1 = 1. - alpha;
    }
  }

  void init(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");

    total_weight = 0.;
    a1_n = 1.;
    s = 0.;

    step(T{}, in); // Initialize with the first value
  }

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    U x = in[0][pos];

    total_weight.add(a1_n);
    // s = (1. - alpha) * s + x
    s = std::fma(a1, s, x);
    // x contributes (1. - alpha) to the next value
    a1_n *= a1;
  }

  void inverse(T, U const *const *rm) noexcept override {
    assert(rm && rm[0] && "NULL removal data.");
    U x = rm[0][pos];

    // undo the contribution of x
    a1_n /= a1;
    // s = s - a1^n * x
    s = std::fma(a1_n, -x, s);
    total_weight.sub(a1_n);
  }

  // Currently we can not utilise rolling update due to engine design
  void roll(U x, U x0) noexcept {
    // s = (1 - alpha) * s + x - (1 - alpha)^n * x0
    s = std::fma(a1, s, std::fma(a1_n, -x0, x));
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    *out = s / total_weight;
  }
};
} // namespace opflow::op
