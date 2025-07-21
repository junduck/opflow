#pragma once

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/unary.hpp"
#include "opflow/op_base.hpp"

namespace opflow::op {
template <time_point_like T>
struct avg : public detail::unary_op<T> {
  using base = detail::unary_op<T>;
  using base::pos;

  detail::smooth val; ///< mean value
  size_t n;           ///< count of values processed

  explicit avg(size_t avg_at = 0) noexcept : base{avg_at}, val{}, n{0} {}

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    ++n;
    val.add(in[0][pos], 1.0 / n);
  }

  void inverse(T, double const *const *rm) noexcept override {
    assert(rm && rm[0] && "NULL removal data.");
    --n;
    val.sub(rm[0][pos], 1.0 / n);
  }

  // Currently we can not utilise rolling update due to engine design
  void roll(double x, double x0) noexcept { val.addsub(x, x0, 1.0 / n); }

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");
    *out = val;
  }
};
} // namespace opflow::op
