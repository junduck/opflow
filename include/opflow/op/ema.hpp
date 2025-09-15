#pragma once

#include "../def.hpp"
#include "../op_base.hpp"

#include "../detail/accum.hpp"

namespace opflow::op {

/**
 * @brief Exponential Moving Average (EMA)
 *
 * Info for coding agent:
 * - Input:
 *   0. x
 * - Output:
 *   0. ema
 * - Param:
 *   0. alpha: Smoothing factor, if alpha > 1, it is used as "period" in common tech indicator library
 *      where alpha = 2 / (period + 1)
 *   @ref detail::smooth_factor for more details
 */
template <typename T>
class ema : public op_base<T> {
public:
  using base = op_base<T>;
  using typename base::data_type;

  explicit ema(data_type alpha) noexcept : val{}, alpha{detail::smooth_factor(alpha)}, initialised{} {}

  void on_data(data_type const *in) noexcept override {
    data_type const x = in[0];
    if (!initialised) {
      val = x;
      initialised = true;
      return;
    }
    val.add(x, alpha);
  }

  void value(data_type *out) const noexcept override { out[0] = val; }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(ema)

private:
  detail::smooth<data_type> val; ///< Current EMA value
  data_type const alpha;         ///< Smoothing factor
  bool initialised;              ///< Whether the first value has been processed
};

/**
 * @brief Unbiased Exponential Moving Average (EMA)
 *
 * @details ema_unbiased uses a bias correction to initialise EMA
 *
 * @ref https://arxiv.org/abs/1412.6980
 *
 * Info for coding agent:
 * - Input:
 *   0. x
 * - Output:
 *   0. ema
 * - Param:
 *   0. alpha: Smoothing factor, if alpha > 1, it is used as "period" in common tech indicator library
 *      where alpha = 2 / (period + 1)
 *   @ref detail::smooth_factor for more details
 */
template <typename T>
class ema_unbiased : public op_base<T> {
public:
  using base = op_base<T>;
  using typename base::data_type;

  explicit ema_unbiased(data_type alpha) noexcept : val{}, alpha{detail::smooth_factor(alpha)}, bias(1.) {}

  void on_data(data_type const *in) noexcept override {
    data_type const x = in[0];

    val.add(x, alpha);
    bias *= alpha;
  }

  void value(data_type *out) const noexcept override {
    *out = val / (data_type(1) - bias); // Apply bias correction
  }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(ema_unbiased)

private:
  detail::smooth<data_type> val; ///< Uncorrected EMA value
  data_type const alpha;         ///< Smoothing factor
  data_type bias;                ///< Bias correction factor
};

/**
 * @brief Time-weighted Exponential Moving Average (EMA)
 *
 * Info for coding agent:
 * - Input:
 *   0. x value
 *   1. dt delta time
 * - Output:
 *   0. ema
 * - Param:
 *   0. tau: Time constant
 */
template <typename T>
class ema_time : public op_base<T> {
public:
  using base = op_base<T>;
  using typename base::data_type;

  explicit ema_time(data_type tau) noexcept : val{}, inv_tau{data_type(1) / tau}, initialised{false} {}

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

  void value(data_type *out) const noexcept override { out[0] = val; }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(ema_time)

private:
  detail::smooth<data_type> val; ///< Current EMA value
  data_type inv_tau;             ///< 1. / Time constant
  bool initialised;              ///< Whether the first value has been processed
};

} // namespace opflow::op
