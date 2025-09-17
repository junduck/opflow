#pragma once

#include "../def.hpp"
#include "../fn_base.hpp"

#include "../detail/accum.hpp"

namespace opflow::fn {
template <typename T>
class order_flow : public fn_base<T> {
public:
  using base = fn_base<T>;
  using typename base::data_type;

  void on_data(data_type const *in, data_type *out) noexcept {
    auto const buy = in[0];
    auto const sell = in[1];

    buy_sum.add(buy);
    sell_sum.add(sell);

    out[0] = buy_sum - sell_sum;
  }

  void reset() noexcept {
    buy_sum.reset();
    sell_sum.reset();
  }

  OPFLOW_INOUT(2, 1)
  OPFLOW_CLONEABLE(order_flow)

private:
  detail::accum<T> buy_sum;
  detail::accum<T> sell_sum;
};
} // namespace opflow::fn
