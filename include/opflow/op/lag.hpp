#pragma once

#include "opflow/def.hpp"
#include "opflow/op_base.hpp"

namespace opflow::op {

enum class fill_policy {
  nan,   ///< fill NaN if no value
  zero,  ///< fill 0 if no value
  last,  ///< fill last value if no value
  oldest ///< fill oldest value if no value
};

template <typename T, fill_policy Policy>
class lag : public win_base<T> {
public:
  using base = win_base<T>;
  using typename base::data_type;
  using typename base::win_type;

  template <typename U>
  lag(U period) noexcept : base(period), lagged(fnan<data_type>) {
    if constexpr (Policy == fill_policy::zero) {
      lagged = 0;
    }
  }

  void on_data(data_type const *in) noexcept override {
    if constexpr (Policy == fill_policy::last) {
      lagged = in[0];
    } else if constexpr (Policy == fill_policy::oldest) {
      if (std::isnan(lagged)) {
        lagged = in[0];
      }
    }
  }

  void on_evict(data_type const *rm) noexcept override { lagged = rm[0]; }

  void value(data_type *out) const noexcept override { out[0] = lagged; }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(lag)

private:
  data_type lagged;
};
} // namespace opflow::op
