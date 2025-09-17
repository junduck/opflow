#pragma once

#include "../def.hpp"
#include "../fn_base.hpp"

#include "../detail/accum.hpp"

namespace opflow::fn {
template <typename T>
class book_imba : public fn_base<T> {
public:
  using base = fn_base<T>;
  using typename base::data_type;

  void on_data(data_type const *in, data_type *out) noexcept {
    auto const bid = in[0];
    auto const ask = in[1];

    bid_sum.add(bid);
    ask_sum.add(ask);

    auto const b = bid_sum.value();
    auto const a = ask_sum.value();

    out[0] = (b - a) / (b + a);
  }

  void reset() noexcept {
    bid_sum.reset();
    ask_sum.reset();
  }

  OPFLOW_INOUT(2, 1)
  OPFLOW_CLONEABLE(book_imba)

private:
  detail::accum<T> bid_sum;
  detail::accum<T> ask_sum;
};
} // namespace opflow::fn
