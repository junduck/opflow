#pragma once

#include <cassert>

#include "detail/accum.hpp"
#include "opflow/op_base.hpp"

namespace opflow::op {
template <std::floating_point U>
struct ema : public op_base<U> {
  detail::smooth<U> val; ///< Current EMA value
  U alpha;               ///< Smoothing factor
  bool initialised;      ///< Whether the first value has been processed

  explicit ema(U alpha) noexcept : val{}, alpha{detail::smooth_factor(alpha)}, initialised{false} {
    assert(alpha > 0. && "alpha/period must be positive.");
  }

  void on_data(U const *in) noexcept override {
    U const x = in[0];
    if (!initialised) {
      val = x;
      initialised = true;
      return;
    }
    val.add(x, alpha);
  }

  void value(U *out) noexcept override { out[0] = val; }

  void reset() noexcept override {
    val.reset();
    initialised = false;
  }

  size_t num_inputs() const noexcept override { return 1; }
  size_t num_outputs() const noexcept override { return 1; }
};

template <std::floating_point U>
struct ema_time : public op_base<U> {
  detail::smooth<U> val; ///< Current EMA value
  U inv_tau;             ///< 1. / Time constant
  bool initialised;      ///< Whether the first value has been processed

  explicit ema_time(U tau) noexcept : val{}, inv_tau{U(1) / tau}, initialised{false} {
    assert(tau > 0. && "Time constant must be positive.");
  }

  void on_data(U const *in) noexcept override {
    auto const x = in[0];
    auto const dt = in[1];

    if (!initialised) {
      val = x;
      initialised = true;
      return;
    }
    U const alpha = U(1) - std::exp(-dt * inv_tau);
    val.add(x, alpha);
  }

  void value(U *out) noexcept override { out[0] = val; }

  void reset() noexcept override {
    val.reset();
    initialised = false;
  }

  size_t num_inputs() const noexcept override { return 1; }
  size_t num_outputs() const noexcept override { return 1; }
};
} // namespace opflow::op
