#pragma once

#include "../def.hpp"
#include "../fn_base.hpp"

namespace opflow::fn {
template <typename T>
class ohlc : public fn_base<T> {
public:
  using base = fn_base<T>;
  using typename base::data_type;

  void on_data(data_type const *in, data_type *out) noexcept {
    auto const p = in[0];
    if (!init) {
      out[0] = out[1] = out[2] = out[3] = p;
      init = true;
    } else {
      if (p > out[1])
        out[1] = p;
      if (p < out[2])
        out[2] = p;
      out[3] = p;
    }
  }

  void reset() noexcept { init = false; }

  OPFLOW_INOUT(1, 4)
  OPFLOW_CLONEABLE(ohlc)

private:
  bool init;
};
} // namespace opflow::fn
