#pragma once

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/unary.hpp"
#include "opflow/op_base.hpp"

namespace opflow::op {
template <time_point_like T>
struct sum : public detail::unary_op<T> {
  using base = detail::unary_op<T>;
  using base::pos;

  detail::accum val; ///< Accumulated value

  explicit sum(size_t sum_at = 0) : base{sum_at}, val{} {}

  void init(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    val = in[0][pos]; // Initialize with the first value
  }

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    val.add(in[0][pos]);
  }

  void inverse(T, double const *const *rm) noexcept override {
    assert(rm && rm[0] && "NULL removal data.");
    val.sub(rm[0][pos]);
  }

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");
    *out = val;
  }
};
constexpr inline size_t op_size = sizeof(sum<uint64_t>);

} // namespace opflow::op
