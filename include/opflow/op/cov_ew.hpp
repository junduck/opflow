#pragma once

#include "opflow/def.hpp"
#include "opflow/op_base.hpp"

#include "opflow/detail/accum.hpp"

namespace opflow::op {
template <typename T>
class cov_ew : public op_base<T> {
public:
  using base = op_base<T>;
  using typename base::data_type;

  explicit cov_ew(data_type alpha) noexcept
      : mx{}, my{}, s2xy{}, alpha{detail::smooth_factor(alpha)}, initialised{false} {}

  void on_data(data_type const *in) noexcept override {
    data_type const x = in[0];
    data_type const y = in[1];

    if (!initialised) [[unlikely]] {
      mx = x; // Initialize with the first value
      my = y;
      initialised = true;
      return;
    }

    data_type const dx = x - mx;
    data_type const dy = y - my;
    mx.add(x, alpha);
    my.add(y, alpha);
    s2xy.add((1.0 - alpha) * dx * dy, alpha);
  }

  void value(data_type *out) const noexcept override {
    out[0] = mx;   // mean of first input
    out[1] = my;   // mean of second input
    out[2] = s2xy; // covariance
  }

  OPFLOW_INOUT(2, 3)
  OPFLOW_CLONEABLE(cov_ew)

private:
  detail::smooth<data_type> mx;   ///< mean of first input
  detail::smooth<data_type> my;   ///< mean of second input
  detail::smooth<data_type> s2xy; ///< cov
  data_type alpha;                ///< smoothing factor
  bool initialised;               ///< whether the first value has been processed
};

static_assert(dag_node<cov_ew<double>>);
} // namespace opflow::op
