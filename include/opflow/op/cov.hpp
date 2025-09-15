#pragma once

#include "../def.hpp"
#include "../op_base.hpp"

#include "../detail/accum.hpp"

namespace opflow::op {
template <typename T>
struct cov : public simple_rollop<T> {
public:
  using base = simple_rollop<T>;
  using typename base::data_type;

  using base::base;

  void on_data(data_type const *in) noexcept override {
    data_type const x = in[0];
    data_type const y = in[1];

    ++n;
    data_type const a = 1.0 / n;
    data_type const dy = y - my;
    mx.add(x, a);
    my.add(y, a);
    mxy.add((x - mx) * dy);
  }

  void on_evict(data_type const *rm) noexcept override {
    data_type const x = rm[0];
    data_type const y = rm[1];

    --n;
    data_type const a = 1.0 / n;
    data_type const dy = y - my;
    mx.sub(x, a);
    my.sub(y, a);
    mxy.sub((x - mx) * dy);
  }

  // Currently we can not utilise rolling update due to engine design
  void roll(data_type x, data_type x0, data_type y, data_type y0) noexcept {
    data_type const dy = y - my;
    data_type const dy0 = y0 - my;
    data_type const a = 1.0 / n;
    mx.addsub(x, x0, a);
    my.addsub(y, y0, a);
    mxy.addsub((x - mx) * dy, (x0 - mx) * dy0);
  }

  void value(data_type *out) const noexcept override {
    out[0] = mx; // mean of first input
    out[1] = my; // mean of second input
    if (n == 1) [[unlikely]] {
      out[2] = 0.; // covariance is zero if only one sample
    } else {
      out[2] = mxy / (n - 1); // unbiased covariance
    }
  }

  OPFLOW_INOUT(2, 3)
  OPFLOW_CLONEABLE(cov)

private:
  detail::smooth<data_type> mx; ///< mean of first input
  detail::smooth<data_type> my; ///< mean of second input
  detail::accum<data_type> mxy; ///< m2 of cross product
  size_t n;                     ///< count of values processed
};
} // namespace opflow::op
