#pragma once

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/unary.hpp"

namespace opflow::op {

/**
 * @brief Enumeration for EMA initialisation strategies
 *
 * @details The behaviour of the EMA can be controlled by the initialisation strategy:
 *
 * - ema_init::first: Initialise with the first value
 * - ema_init::zero: Initialise with zero
 * - ema_init::unbiased: Initialise with bias-corrected value x / (1 - a ^ n) and all produced values are bias-corrected
 *
 */
enum class ema_init {
  first,   ///< Initialise with the first value
  zero,    ///< Initialise with zero
  unbiased ///< Initialise with bias-corrected value (1 - a ^ n)
};

template <typename T, std::floating_point U, ema_init Init = ema_init::first>
struct ema : public detail::unary_op<T, U> {
  using base = detail::unary_op<T, U>;
  using base::pos;

  detail::smooth<U> val;
  U alpha;          ///< Smoothing factor
  bool initialised; ///< Whether the first value has been processed

  explicit ema(U alpha, size_t pos = 0) noexcept
      : base{pos}, val{}, alpha{detail::smooth_factor(alpha)}, initialised{false} {
    assert(alpha > 0. && "alpha/period must be positive.");
  }

  void init(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");

    val = U{};
    initialised = false;
    step(T{}, in); // Initialize with the first value
  }

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    U x = in[0][pos];

    if constexpr (Init == ema_init::first) {
      if (!initialised) {
        val = x;
        initialised = true;
        return;
      }
    }
    val.add(x, alpha);
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    *out = val;
  }
};

// Ref: https://arxiv.org/abs/1412.6980

template <typename T, std::floating_point U>
struct ema<T, U, ema_init::unbiased> : public detail::unary_op<T, U> {
  using base = detail::unary_op<T, U>;
  using base::pos;

  detail::smooth<U> val; ///< Uncorrected EMA value
  U alpha;               ///< Smoothing factor
  U bias;                ///< Bias correction factor

  explicit ema(U alpha, size_t pos = 0) noexcept : base(pos), val(), alpha(detail::smooth_factor(alpha)), bias(1.) {
    assert(alpha > 0. && "alpha/period must be positive.");
  }

  void init(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");

    val = 0.;
    bias = 1.;
    step(T{}, in); // Initialize with the first value
  }

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    U x = in[0][pos];

    val.add(x, alpha);
    bias *= alpha;
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    *out = val / (U(1) - bias); // Apply bias correction
  }
};
} // namespace opflow::op
