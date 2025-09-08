#pragma once

#include "opflow/def.hpp"
#include "opflow/op_base.hpp"

#include "opflow/detail/accum.hpp"

namespace opflow::op {
template <typename T>
struct ewma : public op_base<T> {
public:
  using base = op_base<T>;
  using typename base::data_type;

  explicit ewma(data_type alpha) noexcept : total_weight(), a1(1. - detail::smooth_factor(alpha)), a1_n(1), s() {}

  void on_data(data_type const *in) noexcept override {
    data_type x = in[0];

    total_weight.add(a1_n);
    // s = (1. - alpha) * s + x
    s = std::fma(a1, s, x);
    // x contributes (1. - alpha) to the next value
    a1_n *= a1;
  }

  void on_evict(data_type const *rm) noexcept override {
    data_type x = rm[0];

    // undo the contribution of x
    a1_n /= a1;
    // s = s - a1^n * x
    s = std::fma(a1_n, -x, s);
    total_weight.sub(a1_n);
  }

  // Currently we can not utilise rolling update due to engine design
  void roll(data_type x, data_type x0) noexcept {
    // s = (1 - alpha) * s + x - (1 - alpha)^n * x0
    s = std::fma(a1, s, std::fma(a1_n, -x0, x));
  }

  void value(data_type *out) const noexcept override { *out = s / total_weight; }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(ewma)

private:
  detail::accum<data_type> total_weight; ///< total weight (a1^n)
  data_type const a1;                    ///< 1. - alpha
  data_type a1_n;                        ///< (1. - alpha)^n
  data_type s;                           ///< current weighted sum
};

static_assert(dag_node<ewma<double>>);
} // namespace opflow::op
