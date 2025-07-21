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
  double alpha; ///< Smoothing factor
  bool init;

  explicit ema(double alpha, size_t pos = 0) noexcept : base{pos}, val{}, alpha{alpha}, init{ZeroInit} {}
  explicit ema(size_t period, size_t pos = 0) noexcept : ema{2.0 / (period + 1), pos} {}

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    double x = in[0][pos];

    if constexpr (!ZeroInit) {
      if (!init) [[unlikely]] {
        val = x; // Initialize with the first value
        init = true;
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
