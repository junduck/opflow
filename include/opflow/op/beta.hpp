#pragma once

#include "../def.hpp"
#include "../op_base.hpp"

#include "../detail/accum.hpp"

namespace opflow::op {
template <typename T>
class beta : public win_base<T> {
public:
  using base = win_base<T>;
  using typename base::data_type;

  using base::base;

  void on_data(data_type const *in) noexcept override {
    data_type const x = in[0];
    data_type const y = in[1];

    ++n;
    data_type const a = 1.0 / n;
    data_type const dx = x - mx;
    data_type const dy = y - my;
    mx.add(x, a);
    my.add(y, a);
    m2x.add((x - mx) * dx);
    mxy.add((x - mx) * dy);
  }

  void on_evict(data_type const *rm) noexcept override {
    data_type const x = rm[0];
    data_type const y = rm[1];

    --n;
    data_type const a = 1.0 / n;
    data_type const dx = x - mx;
    data_type const dy = y - my;
    mx.sub(x, a);
    my.sub(y, a);
    m2x.sub((x - mx) * dx);
    mxy.sub((x - mx) * dy);
  }

  void value(data_type *out) const noexcept override {
    out[0] = mx; // mean of first input
    out[1] = my; // mean of second input

    if (n == 1) [[unlikely]] {
      out[2] = 0.;
      out[3] = 0.;
      return;
    }

    out[2] = mxy / (n - 1); // unbiased covariance

    if (m2x > 0.) {
      out[3] = mxy / m2x; // beta = cov(x, y) / var(x)
    } else {
      out[3] = 0.; // avoid division by zero
    }
  }

  OPFLOW_INOUT(2, 4)
  OPFLOW_CLONEABLE(beta)

private:
  detail::smooth<data_type> mx; ///< mean of first input
  detail::smooth<data_type> my; ///< mean of second input
  detail::accum<data_type> mxy; ///< m2 of cross product
  detail::accum<data_type> m2x; ///< m2 of first input
  size_t n;                     ///< count of values processed
};
} // namespace opflow::op
