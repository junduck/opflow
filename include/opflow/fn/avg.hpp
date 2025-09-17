#pragma once

#include "../def.hpp"
#include "../fn_base.hpp"

#include "../detail/accum.hpp"

namespace opflow::fn {
template <typename T>
class avg : public fn_base<T> {
public:
  using base = fn_base<T>;
  using typename base::data_type;

  void on_data(data_type const *in, data_type *out) noexcept {
    auto const x = in[0];
    ++n;
    val.add(x, data_type(1) / n);
    out[0] = val;
  }

  void reset() noexcept {
    n = 0;
    val.reset();
  }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(avg)

private:
  detail::smooth<data_type> val;
  size_t n;
};

template <typename T>
class avg_weighted : public fn_base<T> {
public:
  using base = fn_base<T>;
  using typename base::data_type;

  void on_data(data_type const *in, data_type *out) noexcept {
    auto const x = in[0];
    auto const w = in[1];
    w_sum.add(w);
    val.add(x, w / w_sum);
    out[0] = val;
  }

  void reset() noexcept {
    val.reset();
    w_sum.reset();
  }

  OPFLOW_INOUT(2, 1)
  OPFLOW_CLONEABLE(avg_weighted)

private:
  detail::smooth<data_type> val;
  detail::accum<data_type> w_sum;
};
} // namespace opflow::fn
