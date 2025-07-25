#pragma once

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/binary.hpp"

namespace opflow::op {
template <typename T>
struct var_vw : public detail::weighted_op<T> {
  using base = detail::weighted_op<T>;
  using base::pos;
  using base::pow_weight;

  detail::smooth m;     ///< mean
  detail::accum w_sum;  ///< sum of weights
  detail::accum w2_sum; ///< sum of squared weights
  detail::accum m2;     ///< second moment
  size_t n;             ///< count of values processed

  explicit var_vw(size_t pos = 0, size_t pow_weight = 1) noexcept
      : base{pos, pow_weight}, m{}, w_sum{}, w2_sum{}, m2{}, n{} {}

  void init(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    double const x = in[0][pos];
    double const w = in[0][pow_weight];

    n = 1;
    w_sum = w;
    w2_sum = w * w;

    m = x;
    m2 = 0;
  }

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    double const x = in[0][pos];
    double const w = in[0][pow_weight];

    ++n;
    w_sum.add(w);
    w2_sum.add(w * w);

    double const d = x - m;
    m.add(x, w / w_sum);
    m2.add((x - m) * d * w);
  }

  void inverse(T, double const *const rm) noexcept override {
    assert(rm && rm[0] && "NULL removal data.");
    double const x = rm[0][pos];
    double const w = rm[0][pow_weight];

    --n;
    w_sum.sub(w);
    w2_sum.sub(w * w);

    double const d = x - m;
    m.sub(x, w / w_sum);
    m2.sub((x - m) * d * w);
  }

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");
    assert(this->n > 0 && "value called with empty state.");

    out[0] = m;
    if (n == 1) [[unlikely]] {
      out[1] = 0.;
    } else {
      double const rel_weight = w_sum - w2_sum / w_sum;
      // TODO: remove hardcoded epsilon
      if (rel_weight > 1e-15) [[likely]] {
        out[1] = m2 / rel_weight;
      } else {
        out[1] = 0.;
      }
    }
  }
};

template <typename T>
struct std_vw : public var_vw<T> {
  using base = var_vw<T>;
  using base::base;

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");

    base::value(out);
    out[1] = std::sqrt(out[1]);
  }
};
} // namespace opflow::op
