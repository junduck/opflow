#pragma once

#include "../def.hpp"
#include "../fn_base.hpp"

#include "../detail/accum.hpp"

namespace opflow::fn {
template <typename T>
class sum : public fn_base<T> {
public:
  using base = fn_base<T>;
  using typename base::data_type;

  void on_data(data_type const *in, data_type *out) noexcept {
    auto const x = in[0];
    val.add(x);
    out[0] = val;
  }

  void reset() noexcept { val.reset(); }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(sum)

private:
  detail::accum<data_type> val;
};
} // namespace opflow::fn
