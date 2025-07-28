#pragma once

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/unary.hpp"

namespace opflow::op {
template <typename T, std::floating_point U>
struct sum : public detail::unary_op<T, U> {
  using base = detail::unary_op<T, U>;
  using base::pos;

  detail::accum<U> val; ///< Accumulated value

  explicit sum(size_t sum_at = 0) : base{sum_at}, val{} {}

  void init(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    val = in[0][pos]; // Initialize with the first value
  }

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    val.add(in[0][pos]);
  }

  void inverse(T, U const *const *rm) noexcept override {
    assert(rm && rm[0] && "NULL removal data.");
    val.sub(rm[0][pos]);
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    *out = val;
  }
};

} // namespace opflow::op
