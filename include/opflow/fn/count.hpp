#pragma once

#include "../def.hpp"
#include "../fn_base.hpp"

namespace opflow::fn {
template <typename T>
class count : public fn_base<T> {
public:
  using base = fn_base<T>;
  using typename base::data_type;

  void on_data(data_type const * /*in*/, data_type *out) noexcept {
    ++n;
    *out = static_cast<data_type>(n);
  }

  void reset() noexcept { n = 0; }

  OPFLOW_INOUT(0, 1)
  OPFLOW_CLONEABLE(count)

private:
  size_t n;
};
} // namespace opflow::fn
