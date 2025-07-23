#pragma once

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/unary.hpp"
#include "opflow/op_base.hpp"

namespace opflow::op {
template <time_point_like T, bool ZeroInit = false>
struct ema : public detail::unary_op<T> {
  using base = detail::unary_op<T>;
  using base::pos;

  detail::smooth val;
  double alpha;     ///< Smoothing factor
  bool initialised; ///< Whether the first value has been processed

  explicit ema(double alpha, size_t pos = 0) noexcept : base{pos}, val{}, alpha{alpha}, initialised{ZeroInit} {
    assert(alpha > 0. && "alpha/period must be positive.");
    if (alpha >= 1.) {
      // alpha is actually a period
      this->alpha = 2.0 / (alpha + 1);
    }
  }

  void init(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");

    val = 0.;
    initialised = ZeroInit;
    step(T{}, in); // Initialize with the first value
  }

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    double x = in[0][pos];

    if constexpr (!ZeroInit) {
      if (!initialised) [[unlikely]] {
        val = x; // Initialize with the first value
        initialised = true;
        return;
      }
    }
    val.add(x, alpha);
  }

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");
    *out = val;
  }
};
} // namespace opflow::op
