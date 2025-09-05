#pragma once

#include "opflow/def.hpp"
#include "opflow/op_base.hpp"

#include "opflow/detail/accum.hpp"

namespace opflow::op {
template <typename T>
class var_ew : public op_base<T> {
public:
  using base = op_base<T>;
  using typename base::data_type;

  explicit var_ew(data_type alpha) noexcept : m{}, s2{}, alpha{detail::smooth_factor(alpha)}, initialised{false} {}

  void on_data(data_type const *in) noexcept override {
    data_type const x = in[0];

    if (!initialised) [[unlikely]] {
      m = x; // Initialize with the first value
      initialised = true;
      return;
    }

    data_type const d = x - m;
    // data_type const a1 = 1.0 - alpha;
    m.add(x, alpha);
    // s2.add(a1 * d * d, alpha);
    data_type const d2 = x - m;
    s2.add(d * d2, alpha); // use Welford-like method for one less multiplication
  }

  void value(data_type *out) const noexcept override {
    out[0] = m;
    out[1] = s2;
  }

  void reset() noexcept override {
    m.reset();
    s2.reset();
    initialised = false;
  }

  OPFLOW_INOUT(1, 2)
  OPFLOW_CLONEABLE(var_ew)

protected:
  detail::smooth<data_type> m;  ///< mean
  detail::smooth<data_type> s2; ///< variance
  data_type alpha;              ///< smoothing factor
  bool initialised;             ///< whether the first value has been processed
};

static_assert(dag_node<var_ew<double>>);

template <typename T>
struct std_ew : public var_ew<T> {
  using base = var_ew<T>;
  using typename base::data_type;

  using base::base;

  void value(data_type *out) const noexcept override {
    base::value(out);
    out[1] = std::sqrt(out[1]);
  }
};

static_assert(dag_node<std_ew<double>>);
} // namespace opflow::op
