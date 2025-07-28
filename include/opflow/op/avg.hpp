#pragma once

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/unary.hpp"

#ifndef NDEBUG
#include <numeric>
namespace opflow::op {
template <std::floating_point T>
T avg_naive(std::span<const T> data) {
  if (data.empty()) {
    throw std::runtime_error("Cannot compute average of empty data.");
  }
  T sum = std::accumulate(data.begin(), data.end(), T{});
  return sum / data.size();
}
} // namespace opflow::op
#endif

namespace opflow::op {
template <typename T, std::floating_point U>
struct avg : public detail::unary_op<T, U> {
  using base = detail::unary_op<T, U>;
  using base::pos;

  detail::smooth<U> val; ///< mean value
  size_t n;              ///< count of values processed

  explicit avg(size_t avg_at = 0) noexcept : base{avg_at}, val{}, n{0} {}

  void init(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    n = 1;
    val = in[0][pos];
  }

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    ++n;
    val.add(in[0][pos], 1.0 / n);
  }

  void inverse(T, U const *const *rm) noexcept override {
    assert(rm && rm[0] && "NULL removal data.");
    --n;
    val.sub(rm[0][pos], 1.0 / n);
  }

  // Currently we can not utilise rolling update due to engine design
  void roll(U x, U x0) noexcept { val.addsub(x, x0, 1. / n); }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    *out = val;
  }
};
} // namespace opflow::op
