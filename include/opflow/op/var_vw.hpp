#pragma once

#include "../def.hpp"
#include "../op_base.hpp"

#include "../detail/accum.hpp"

namespace opflow::op {
template <typename T>
class var_vw : public simple_rollop<T> {
public:
  using base = simple_rollop<T>;
  using typename base::data_type;

  using base::base;

  void on_data(data_type const *in) noexcept override {
    data_type const x = in[0];
    data_type const w = in[1];

    ++n;
    w_sum.add(w);
    w2_sum.add(w * w);

    data_type const d = x - m;
    m.add(x, w / w_sum);
    m2.add((x - m) * d * w);
  }

  void on_evict(data_type const *rm) noexcept override {
    data_type const x = rm[0];
    data_type const w = rm[1];

    --n;
    w_sum.sub(w);
    w2_sum.sub(w * w);

    data_type const d = x - m;
    m.sub(x, w / w_sum);
    m2.sub((x - m) * d * w);
  }

  void value(data_type *out) const noexcept override {
    out[0] = m;
    if (n == 1) [[unlikely]] {
      out[1] = 0.;
    } else {
      data_type const rel_weight = w_sum - w2_sum / w_sum;
      if (rel_weight > feps100<data_type>) [[likely]] {
        out[1] = m2 / rel_weight;
      } else {
        out[1] = 0.;
      }
    }
  }

  OPFLOW_INOUT(2, 2)
  OPFLOW_CLONEABLE(var_vw)

protected:
  detail::smooth<data_type> m;     ///< mean
  detail::accum<data_type> w_sum;  ///< sum of weights
  detail::accum<data_type> w2_sum; ///< sum of squared weights
  detail::accum<data_type> m2;     ///< second moment
  size_t n;                        ///< count of values processed
};

template <typename T>
struct std_vw : public var_vw<T> {
  using base = var_vw<T>;
  using typename base::data_type;

  using base::base;

  void value(data_type *out) const noexcept override {
    base::value(out);
    out[1] = std::sqrt(out[1]);
  }
};
} // namespace opflow::op
