#pragma once

#include "opflow/def.hpp"
#include "opflow/op_base.hpp"

#include "opflow/detail/accum.hpp"

namespace opflow::op {
template <typename T>
class corr : public win_base<T> {
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
    m2y.add((y - my) * dy);
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
    m2y.sub((y - my) * dy);
    mxy.sub((x - mx) * dy);
  }

  void value(data_type *out) const noexcept override {
    out[0] = mx; // mean of first input
    out[1] = my; // mean of second input
    if (n == 1) [[unlikely]] {
      // cov/corr is zero if only one sample
      out[2] = 0.;
      out[3] = 0.;
    } else {
      out[2] = mxy / (n - 1); // unbiased covariance
      data_type const denom = std::sqrt<data_type>(m2x * m2y);
      if (denom == 0.) {
        out[3] = 0.; // avoid division by zero
      } else {
        out[3] = mxy / denom; // Pearson correlation coefficient
      }
    }
  }

  void reset() noexcept override {
    mx.reset();
    my.reset();
    mxy.reset();
    m2x.reset();
    m2y.reset();
    n = 0;
  }

  OPFLOW_INOUT(2, 4);
  OPFLOW_CLONEABLE(corr);

private:
  detail::smooth<data_type> mx; ///< mean of first input
  detail::smooth<data_type> my; ///< mean of second input
  detail::accum<data_type> mxy; ///< m2 of cross product
  detail::accum<data_type> m2x; ///< m2 of first input
  detail::accum<data_type> m2y; ///< m2 of second input
  size_t n;                     ///< count of values processed
};

static_assert(dag_node<corr<double>>);
} // namespace opflow::op
