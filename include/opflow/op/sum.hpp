#pragma once

#include "detail/accum.hpp"
#include "opflow/op_base.hpp"

namespace opflow::op {
template <typename T, win_type WIN = win_type::event>
struct sum : public win_base<T, WIN> {
  using base = win_base<T, WIN>;
  using typename base::data_type;

  detail::accum<data_type> val; ///< accumulated value

  using base::base;

  void on_data(data_type const *in) noexcept override { val.add(in[0]); }
  void on_evict(data_type const *rm) noexcept override { val.sub(rm[0]); }
  void value(data_type *out) const noexcept override { out[0] = val; }
  void reset() noexcept override { val.reset(); }

  size_t num_inputs() const noexcept override { return 1; }
  size_t num_outputs() const noexcept override { return 1; }
};

// for testing
template <std::floating_point U>
struct add2 : op_base<U> {
  using base = op_base<U>;
  using typename base::data_type;

  double val; ///< in[0] + in[2]

  using base::base;

  void on_data(data_type const *in) noexcept override { val = in[0] + in[1]; }
  void value(data_type *out) const noexcept override { out[0] = val; }
  void reset() noexcept override { val = 0; }

  size_t num_inputs() const noexcept override { return 2; }
  size_t num_outputs() const noexcept override { return 1; }
};
} // namespace opflow::op
