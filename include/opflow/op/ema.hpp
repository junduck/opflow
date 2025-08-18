#pragma once

#include <cassert>

#include "detail/accum.hpp"
#include "opflow/op_base.hpp"

namespace opflow::op {
template <typename T>
struct ema : public op_base<T> {
  using base = op_base<T>;
  using typename base::data_type;

  detail::smooth<data_type> val; ///< Current EMA value
  data_type alpha;               ///< Smoothing factor
  bool initialised;              ///< Whether the first value has been processed

  explicit ema(data_type alpha) noexcept : val{}, alpha{detail::smooth_factor(alpha)}, initialised{false} {
    assert(alpha > 0. && "alpha/period must be positive.");
  }

  void on_data(data_type const *in) noexcept override {
    data_type const x = in[0];
    if (!initialised) {
      val = x;
      initialised = true;
      return;
    }
    val.add(x, alpha);
  }

  void value(data_type *out) noexcept override { out[0] = val; }

  void reset() noexcept override {
    val.reset();
    initialised = false;
  }

  size_t num_inputs() const noexcept override { return 1; }
  size_t num_outputs() const noexcept override { return 1; }
};

template <typename T>
struct ema_time : public op_base<T> {
  using base = op_base<T>;
  using typename base::data_type;

  detail::smooth<data_type> val; ///< Current EMA value
  data_type inv_tau;             ///< 1. / Time constant
  bool initialised;              ///< Whether the first value has been processed

  explicit ema_time(data_type tau) noexcept : val{}, inv_tau{data_type(1) / tau}, initialised{false} {
    assert(tau > 0. && "Time constant must be positive.");
  }

  void on_data(data_type const *in) noexcept override {
    auto const x = in[0];
    auto const dt = in[1];

    if (!initialised) {
      val = x;
      initialised = true;
      return;
    }
    data_type const alpha = data_type(1) - std::exp(-dt * inv_tau);
    val.add(x, alpha);
  }

  void value(data_type *out) noexcept override { out[0] = val; }

  void reset() noexcept override {
    val.reset();
    initialised = false;
  }

  size_t num_inputs() const noexcept override { return 1; }
  size_t num_outputs() const noexcept override { return 1; }
};
} // namespace opflow::op
