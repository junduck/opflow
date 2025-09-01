#pragma once

#include <algorithm>
#include <cmath>

#include "opflow/def.hpp"
#include "opflow/fn_base.hpp"

#include "functor.hpp"

#include "opflow/detail/utils_math.hpp"

#ifndef OPFLOW_FN_UNARY_FN
#define OPFLOW_FN_UNARY_FN(name, fn)                                                                                   \
  template <std::floating_point T>                                                                                     \
  struct name : public fn_base<T> {                                                                                    \
    using base = fn_base<T>;                                                                                           \
    using base::base;                                                                                                  \
    using typename base::data_type;                                                                                    \
    uint32_t const n_input;                                                                                            \
    name(uint32_t n = 1) noexcept : n_input(n) {}                                                                      \
    void on_data(data_type const *in, data_type *out) noexcept override {                                              \
      data_type *OPFLOW_RESTRICT cast = out;                                                                           \
      for (size_t i = 0; i < n_input; ++i) {                                                                           \
        cast[i] = fn(in[i]);                                                                                           \
      }                                                                                                                \
    }                                                                                                                  \
    size_t num_inputs() const noexcept override { return n_input; }                                                    \
    size_t num_outputs() const noexcept override { return n_input; }                                                   \
    OPFLOW_CLONEABLE(name)                                                                                             \
  };
#endif

#ifndef OPFLOW_FN_BINARY_FN
#define OPFLOW_FN_BINARY_FN(name, fn)                                                                                  \
  template <std::floating_point T>                                                                                     \
  struct name : public fn_base<T> {                                                                                    \
    using base = fn_base<T>;                                                                                           \
    using base::base;                                                                                                  \
    using typename base::data_type;                                                                                    \
    void on_data(data_type const *in, data_type *out) noexcept override {                                              \
      data_type *OPFLOW_RESTRICT cast = out;                                                                           \
      cast[0] = fn(in[0], in[1]);                                                                                      \
    }                                                                                                                  \
    size_t num_inputs() const noexcept override { return 2; }                                                          \
    size_t num_outputs() const noexcept override { return 1; }                                                         \
    OPFLOW_CLONEABLE(name)                                                                                             \
  };
#endif

namespace opflow::fn {

// Unary functors can be vectorised by compiler, so they take an n_input ctor arg.

OPFLOW_FN_UNARY_FN(neg, detail::neg);
OPFLOW_FN_UNARY_FN(inv, detail::inv);
OPFLOW_FN_UNARY_FN(abs, std::abs)
OPFLOW_FN_UNARY_FN(exp, std::exp);
OPFLOW_FN_UNARY_FN(expm1, std::expm1);
OPFLOW_FN_UNARY_FN(log, std::log);
OPFLOW_FN_UNARY_FN(log10, std::log10);
OPFLOW_FN_UNARY_FN(log2, std::log2);
OPFLOW_FN_UNARY_FN(log1p, std::log1p);
OPFLOW_FN_UNARY_FN(sqrt, std::sqrt);
OPFLOW_FN_UNARY_FN(cbrt, std::cbrt);
OPFLOW_FN_UNARY_FN(sin, std::sin);
OPFLOW_FN_UNARY_FN(cos, std::cos);
OPFLOW_FN_UNARY_FN(tan, std::tan);
OPFLOW_FN_UNARY_FN(asin, std::asin);
OPFLOW_FN_UNARY_FN(acos, std::acos);
OPFLOW_FN_UNARY_FN(atan, std::atan);
OPFLOW_FN_UNARY_FN(sinh, std::sinh);
OPFLOW_FN_UNARY_FN(cosh, std::cosh);
OPFLOW_FN_UNARY_FN(tanh, std::tanh);
OPFLOW_FN_UNARY_FN(asinh, std::asinh);
OPFLOW_FN_UNARY_FN(acosh, std::acosh);
OPFLOW_FN_UNARY_FN(atanh, std::atanh);
OPFLOW_FN_UNARY_FN(erf, std::erf);
OPFLOW_FN_UNARY_FN(erfc, std::erfc);
OPFLOW_FN_UNARY_FN(tgamma, std::tgamma);
OPFLOW_FN_UNARY_FN(lgamma, std::lgamma);
OPFLOW_FN_UNARY_FN(ceil, std::ceil);
OPFLOW_FN_UNARY_FN(floor, std::floor);
OPFLOW_FN_UNARY_FN(trunc, std::trunc);
OPFLOW_FN_UNARY_FN(round, std::round);

template <std::floating_point T>
struct clamp : public fn_base<T> {
  using base = fn_base<T>;
  using base::base;
  using typename base::data_type;

  data_type const lo;
  data_type const hi;
  uint32_t const n_input;

  explicit clamp(data_type lo, data_type hi, uint32_t n = 1) noexcept : lo(lo), hi(hi), n_input(n) {}

  void on_data(data_type const *in, data_type *out) noexcept override {
    data_type *OPFLOW_RESTRICT cast = out;
    for (size_t i = 0; i < n_input; ++i) {
      cast[i] = std::clamp(in[i], lo, hi);
    }
  }

  size_t num_inputs() const noexcept override { return n_input; }
  size_t num_outputs() const noexcept override { return n_input; }

  OPFLOW_CLONEABLE(clamp)
};

// Due to memory layout constraints, we can't vectorise binary functors.

OPFLOW_FN_BINARY_FN(add, detail::add);
OPFLOW_FN_BINARY_FN(sub, detail::sub);
OPFLOW_FN_BINARY_FN(mul, detail::mul);
OPFLOW_FN_BINARY_FN(div, detail::div);
OPFLOW_FN_BINARY_FN(fmod, detail::fmod);

template <std::floating_point T>
using lerp = functor<T, detail::lerp<T>>;

} // namespace opflow::fn
