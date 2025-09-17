#pragma once

#include "../def.hpp"
#include "../fn_base.hpp"

namespace opflow::fn {
template <typename T>
class simple_return : public fn_base<T> {
public:
  using base = fn_base<T>;
  using typename base::data_type;

  void on_data(data_type const *in, data_type *out) noexcept {
    auto const curr = in[0];
    if (!init) {
      open = curr;
      init = true;
      *out = 0;
    } else {
      if (very_small(open)) {
        *out = 0;
      } else {
        *out = (curr - open) / open;
      }
    }
  }

  void reset() noexcept {
    open = 0;
    init = false;
  }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(simple_return)

private:
  data_type open;
  bool init;
};

template <typename T>
class log_return : public fn_base<T> {
public:
  using base = fn_base<T>;
  using typename base::data_type;

  void on_data(data_type const *in, data_type *out) noexcept {
    auto const curr = std::log(in[0]);
    if (!init) {
      open = curr;
      init = true;
      *out = 0;
    } else {
      *out = curr - open;
    }
  }

  void reset() noexcept {
    open = 0;
    init = false;
  }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(log_return)

private:
  data_type open;
  bool init;
};
} // namespace opflow::fn
