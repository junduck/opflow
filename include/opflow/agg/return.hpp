#pragma once

#include <span>

#include "../agg_base.hpp"
#include "../common.hpp"
#include "../def.hpp"

namespace opflow::agg {
template <typename T>
struct log_return : public agg_base<T> {
  using data_type = T;

  void on_data(size_t n, data_type const *const *in, data_type *out) noexcept override {
    std::span<data_type const> input(in[0], n);
    if (very_small(input.front())) {
      out[0] = 0;
      return;
    }
    auto const ret = std::log(input.back() / input.front());
    out[0] = ret;
  }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(log_return)
};

template <typename T>
struct simple_return : public agg_base<T> {
  using data_type = T;

  void on_data(size_t n, data_type const *const *in, data_type *out) noexcept override {
    std::span<data_type const> input(in[0], n);
    if (very_small(input.front())) {
      out[0] = 0;
      return;
    }
    auto const ret = (input.back() - input.front()) / input.front();
    out[0] = ret;
  }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(simple_return)
};
} // namespace opflow::agg
