#pragma once

#include <cmath>

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/unary.hpp"
#include "opflow/op_base.hpp"

namespace opflow::op {
template <time_point_like T>
struct ewma : public detail::unary_op<T> {
  using base = detail::unary_op<T>;
  using base::pos;

  detail::accum total_weight; ///< total weight (a1^n)
  double a1;                  ///< 1. - alpha
  double a1_n;                ///< (1. - alpha)^n
  double s;                   ///< current weighted sum

  explicit ewma(double alpha, size_t pos = 0) noexcept : base{pos}, total_weight{}, a1{1. - alpha}, a1_n{1.}, s{} {
    assert(alpha > 0. && "alpha/period must be positive.");
    if (alpha >= 1.) {
      alpha = 2.0 / (alpha + 1);
      a1 = 1. - alpha;
    }
  }

  void init(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");

    total_weight = 0.;
    a1_n = 1.;
    s = 0.;

    step(T{}, in); // Initialize with the first value
  }

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    double x = in[0][pos];

    total_weight.add(a1_n);
    // s = (1. - alpha) * s + x
    s = std::fma(a1, s, x);
    // x contributes (1. - alpha) to the next value
    a1_n *= a1;
  }

  void inverse(T, double const *const *rm) noexcept override {
    assert(rm && rm[0] && "NULL removal data.");
    double x = rm[0][pos];

    // undo the contribution of x
    a1_n /= a1;
    // s = s - a1^n * x
    s = std::fma(a1_n, -x, s);
    total_weight.sub(a1_n);
  }

  // Currently we can not utilise rolling update due to engine design
  void roll(double x, double x0) noexcept {
    // s = (1 - alpha) * s + x - (1 - alpha)^n * x0
    s = std::fma(a1, s, std::fma(a1_n, -x0, x));
  }

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");
    *out = s / total_weight;
  }
};
} // namespace opflow::op
