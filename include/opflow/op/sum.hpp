#pragma once

#include "opflow/def.hpp"
#include "opflow/detail/accum.hpp"
#include "opflow/op_base.hpp"

namespace opflow::op {
template <typename T>
class sum : public win_base<T> {
public:
  using base = win_base<T>;
  using typename base::data_type;

  using base::base;

  void on_data(data_type const *in) noexcept override { val.add(in[0]); }
  void on_evict(data_type const *rm) noexcept override { val.sub(rm[0]); }
  void value(data_type *out) const noexcept override { out[0] = val; }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(sum)

private:
  detail::accum<data_type> val; ///< accumulated value
};

#ifndef NDEBUG
// for testing purpose
template <std::floating_point U>
struct add2 : op_base<U> {
  using base = op_base<U>;
  using typename base::data_type;

  double val; ///< in[0] + in[2]

  using base::base;

  void on_data(data_type const *in) noexcept override { val = in[0] + in[1]; }
  void value(data_type *out) const noexcept override { out[0] = val; }

  OPFLOW_INOUT(2, 1)
  OPFLOW_CLONEABLE(add2)
};
#endif
} // namespace opflow::op
