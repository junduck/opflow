#pragma once

#include <cmath>
#include <deque>

#include "opflow/op/detail/binary.hpp"
#include "opflow/op/detail/ternary.hpp"
#include "opflow/op/detail/unary.hpp"

#define DEF_MATH_OP(name, fn)                                                                                          \
  template <typename T, std::floating_point U>                                                                         \
  struct name : public detail::unary_op<T, U> {                                                                        \
    using base = detail::unary_op<T, U>;                                                                               \
    using base::pos;                                                                                                   \
    U val;                                                                                                             \
    void step(T, U const *const *in) noexcept override {                                                               \
      assert(in && in[0] && "NULL input data.");                                                                       \
      val = fn(in[0][pos]);                                                                                            \
    }                                                                                                                  \
    void value(U *out) noexcept override {                                                                             \
      assert(out && "NULL output buffer.");                                                                            \
      *out = val;                                                                                                      \
    }                                                                                                                  \
  }

#define DEF_MATH_BIN_OP(name, fn)                                                                                      \
  template <typename T, std::floating_point U>                                                                         \
  struct name : public detail::binary_op<T, U> {                                                                       \
    using base = detail::binary_op<T, U>;                                                                              \
    using base::pos0;                                                                                                  \
    using base::pos1;                                                                                                  \
    using base::base;                                                                                                  \
    U val;                                                                                                             \
    void step(T, U const *const *in) noexcept override {                                                               \
      assert(in && in[0] && in[1] && "NULL input data.");                                                              \
      val = fn(in[0][pos0], in[1][pos1]);                                                                              \
    }                                                                                                                  \
    void value(U *out) noexcept override {                                                                             \
      assert(out && "NULL output buffer.");                                                                            \
      *out = val;                                                                                                      \
    }                                                                                                                  \
  }

namespace opflow::op {
namespace detail {
template <std::floating_point U>
inline U inv(U a) noexcept {
  return 1.0 / a;
}
template <std::floating_point U>
inline U neg(U a) noexcept {
  return -a;
}

template <std::floating_point U>
inline U add(U a, U b) noexcept {
  return a + b;
}
template <std::floating_point U>
inline U sub(U a, U b) noexcept {
  return a - b;
}
template <std::floating_point U>
inline U mul(U a, U b) noexcept {
  return a * b;
}
template <std::floating_point U>
inline U div(U a, U b) noexcept {
  return a / b;
}
template <std::floating_point U>
inline U fmod(U a, U b) noexcept {
  return std::fmod(a, b);
}
} // namespace detail

// basic arithmetic operations

DEF_MATH_BIN_OP(add, detail::add);
DEF_MATH_BIN_OP(sub, detail::sub);
DEF_MATH_BIN_OP(mul, detail::mul);
DEF_MATH_BIN_OP(div, detail::div);
DEF_MATH_BIN_OP(fmod, detail::fmod);
DEF_MATH_OP(inv, detail::inv);
DEF_MATH_OP(neg, detail::neg);

// from <cmath>

DEF_MATH_OP(abs, std::abs);
DEF_MATH_OP(exp, std::exp);
DEF_MATH_OP(expm1, std::expm1);
DEF_MATH_OP(log, std::log);
DEF_MATH_OP(log10, std::log10);
DEF_MATH_OP(log2, std::log2);
DEF_MATH_OP(log1p, std::log1p);
DEF_MATH_OP(sqrt, std::sqrt);
DEF_MATH_OP(cbrt, std::cbrt);
DEF_MATH_OP(sin, std::sin);
DEF_MATH_OP(cos, std::cos);
DEF_MATH_OP(tan, std::tan);
DEF_MATH_OP(asin, std::asin);
DEF_MATH_OP(acos, std::acos);
DEF_MATH_OP(atan, std::atan);
DEF_MATH_OP(sinh, std::sinh);
DEF_MATH_OP(cosh, std::cosh);
DEF_MATH_OP(tanh, std::tanh);
DEF_MATH_OP(asinh, std::asinh);
DEF_MATH_OP(acosh, std::acosh);
DEF_MATH_OP(atanh, std::atanh);
DEF_MATH_OP(erf, std::erf);
DEF_MATH_OP(erfc, std::erfc);
DEF_MATH_OP(tgamma, std::tgamma);
DEF_MATH_OP(lgamma, std::lgamma);
DEF_MATH_OP(ceil, std::ceil);
DEF_MATH_OP(floor, std::floor);
DEF_MATH_OP(trunc, std::trunc);
DEF_MATH_OP(round, std::round);

template <typename T, std::floating_point U>
struct lerp : public detail::binary_op<T, U> {
  using base = detail::binary_op<T, U>;
  using base::pos0;
  using base::pos1;

  U t;
  U val;

  explicit lerp(U t, size_t pos0 = 0, size_t pos1 = 0) : base{pos0, pos1}, t{t} {}

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    val = std::lerp(in[0][pos0], in[1][pos1], t);
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    *out = val;
  }
};

template <typename T, std::floating_point U>
struct clamp : public detail::unary_op<T, U> {
  using base = detail::unary_op<T, U>;
  using base::pos;

  U lo;
  U hi;
  U val;

  clamp(U lo, U hi, size_t pos = 0) : base{pos}, lo{lo}, hi{hi} {}

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    val = std::clamp(in[0][pos], lo, hi);
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    *out = val;
  }
};

template <typename T, std::floating_point U>
struct rollmin : public detail::unary_op<T, U> {
  using base = detail::unary_op<T, U>;
  using base::pos;

  std::deque<U> deq;

  explicit rollmin(size_t pos = 0) : base{pos}, deq{} {}

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    U val = in[0][pos];
    while (!deq.empty() && deq.back() > val) {
      deq.pop_back();
    }
    deq.push_back(val);
  }

  void inverse(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    U val = in[0][pos];
    if (!deq.empty() && deq.front() == val) {
      deq.pop_front();
    }
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    assert(!deq.empty() && "value called with empty state.");
    out[0] = deq.front();
  }
};

template <typename T, std::floating_point U>
struct rollmax : public detail::unary_op<T, U> {
  using base = detail::unary_op<T, U>;
  using base::pos;

  std::deque<U> deq;

  explicit rollmax(size_t pos = 0) : base{pos}, deq{} {}

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    U val = in[0][pos];
    while (!deq.empty() && deq.back() < val) {
      deq.pop_back();
    }
    deq.push_back(val);
  }

  void inverse(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    U val = in[0][pos];
    if (!deq.empty() && deq.front() == val) {
      deq.pop_front();
    }
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    assert(!deq.empty() && "value called with empty state.");
    out[0] = deq.front();
  }
};

template <typename T, std::floating_point U>
struct custom_unary_op : public detail::unary_op<T, U> {
  using base = detail::unary_op<T, U>;
  using base::pos;

  U val;                    ///< Value to store the result
  std::function<U(U)> func; ///< Custom function to apply

  explicit custom_unary_op(std::function<U(U)> func, size_t pos = 0) : base{pos}, func{std::move(func)} {}

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    val = func(in[0][pos]);
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    out[0] = val;
  }
};

/*
if (cond(pred0)) {
  return fn_true(pred1);
} else {
  return fn_false(pred1);
}
*/

template <typename T, std::floating_point U>
struct condition_unary_op : public detail::binary_op<T, U> {
  using base = detail::binary_op<T, U>;
  using base::pos0;
  using base::pos1;

  U val;                        ///< Value to store the result
  std::function<bool(U)> cond;  ///< Condition function to apply
  std::function<U(U)> fn_true;  ///< Function to apply if condition is true
  std::function<U(U)> fn_false; ///< Function to apply if condition is false

  explicit condition_unary_op(std::function<bool(U)> cond, std::function<U(U)> fn_true, std::function<U(U)> fn_false,
                              size_t pos0 = 0, size_t pos1 = 0)
      : base{pos0, pos1}, cond{std::move(cond)}, fn_true{std::move(fn_true)}, fn_false{std::move(fn_false)} {}

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    val = cond(in[0][pos0]) ? fn_true(in[1][pos1]) : fn_false(in[1][pos1]);
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    out[0] = val;
  }
};

template <typename T, std::floating_point U>
struct custom_binary_op : public detail::binary_op<T, U> {
  using base = detail::binary_op<T, U>;
  using base::pos0;
  using base::pos1;

  U val;                       ///< Value to store the result
  std::function<U(U, U)> func; ///< Custom function to apply

  explicit custom_binary_op(std::function<U(U, U)> func, size_t pos0 = 0, size_t pos1 = 0)
      : base{pos0, pos1}, func{std::move(func)} {}

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    val = func(in[0][pos0], in[1][pos1]);
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    out[0] = val;
  }
};

/*
if (cond(pred0)) {
  return fn_true(pred1, pred2);
} else {
  return fn_false(pred1, pred2);
}
*/

template <typename T, std::floating_point U>
struct condition_binary_op : public detail::ternary_op<T, U> {
  using base = detail::ternary_op<T, U>;
  using base::pos0;
  using base::pos1;
  using base::pos2;

  U val;                           ///< Value to store the result
  std::function<bool(U)> cond;     ///< Condition function to apply
  std::function<U(U, U)> fn_true;  ///< Function to apply if condition is true
  std::function<U(U, U)> fn_false; ///< Function to apply if condition is false

  explicit condition_binary_op(std::function<bool(U)> cond, std::function<U(U, U)> fn_true,
                               std::function<U(U, U)> fn_false, size_t pos0 = 0, size_t pos1 = 0, size_t pos2 = 0)
      : base{pos0, pos1, pos2}, cond{std::move(cond)}, fn_true{std::move(fn_true)}, fn_false{std::move(fn_false)} {}

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && in[1] && in[2] && "NULL input data.");
    val = cond(in[0][pos0]) ? fn_true(in[1][pos1], in[2][pos2]) : fn_false(in[1][pos1], in[2][pos2]);
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    out[0] = val;
  }
};
} // namespace opflow::op

#undef DEF_MATH_OP
#undef DEF_MATH_BIN_OP
