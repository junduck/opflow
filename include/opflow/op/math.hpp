#pragma once

#include <cmath>

#include "opflow/op/detail/binary.hpp"
#include "opflow/op/detail/unary.hpp"

namespace opflow::op {
namespace detail {
using unary_math_fptr = double (*)(double);

template <typename T, unary_math_fptr Fn>
struct math_op : public unary_op<T> {
  using base = unary_op<T>;
  using base::pos;

  double val;

  using base::base;

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    val = Fn(in[0][pos]);
  }

  void inverse(T, double const *const *) noexcept override {}

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");
    *out = val;
  }
};

using binary_math_fptr = double (*)(double, double);

template <typename T, binary_math_fptr Fn>
struct math_bin_op : public binary_op<T> {
  using base = binary_op<T>;
  using base::pos0;
  using base::pos1;

  double val;

  using base::base;

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    val = Fn(in[0][pos0], in[1][pos1]);
  }

  void inverse(T, double const *const *) noexcept override {}

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");
    *out = val;
  }
};

inline double inv(double a) noexcept { return 1.0 / a; }
inline double neg(double a) noexcept { return -a; }

inline double add(double a, double b) noexcept { return a + b; }
inline double sub(double a, double b) noexcept { return a - b; }
inline double mul(double a, double b) noexcept { return a * b; }
inline double div(double a, double b) noexcept { return a / b; }
inline double fmod(double a, double b) noexcept { return std::fmod(a, b); }

} // namespace detail

// basic arithmetic operations

template <typename T>
using add = detail::math_bin_op<T, detail::add>;

template <typename T>
using sub = detail::math_bin_op<T, detail::sub>;

template <typename T>
using mul = detail::math_bin_op<T, detail::mul>;

template <typename T>
using div = detail::math_bin_op<T, detail::div>;

template <typename T>
using fmod = detail::math_bin_op<T, detail::fmod>;

template <typename T>
using inv = detail::math_op<T, detail::inv>;

template <typename T>
using neg = detail::math_op<T, detail::neg>;

// from <cmath>

template <typename T>
using abs = detail::math_op<T, std::abs>;

template <typename T>
using exp = detail::math_op<T, std::exp>;

template <typename T>
using expm1 = detail::math_op<T, std::expm1>;

template <typename T>
using log = detail::math_op<T, std::log>;

template <typename T>
using log10 = detail::math_op<T, std::log10>;

template <typename T>
using log2 = detail::math_op<T, std::log2>;

template <typename T>
using log1p = detail::math_op<T, std::log1p>;

template <typename T>
using sqrt = detail::math_op<T, std::sqrt>;

template <typename T>
using cbrt = detail::math_op<T, std::cbrt>;

template <typename T>
using sin = detail::math_op<T, std::sin>;

template <typename T>
using cos = detail::math_op<T, std::cos>;

template <typename T>
using tan = detail::math_op<T, std::tan>;

template <typename T>
using asin = detail::math_op<T, std::asin>;

template <typename T>
using acos = detail::math_op<T, std::acos>;

template <typename T>
using atan = detail::math_op<T, std::atan>;

template <typename T>
using sinh = detail::math_op<T, std::sinh>;

template <typename T>
using cosh = detail::math_op<T, std::cosh>;

template <typename T>
using tanh = detail::math_op<T, std::tanh>;

template <typename T>
using asinh = detail::math_op<T, std::asinh>;

template <typename T>
using acosh = detail::math_op<T, std::acosh>;

template <typename T>
using atanh = detail::math_op<T, std::atanh>;

template <typename T>
using erf = detail::math_op<T, std::erf>;

template <typename T>
using erfc = detail::math_op<T, std::erfc>;

template <typename T>
using tgamma = detail::math_op<T, std::tgamma>;

template <typename T>
using lgamma = detail::math_op<T, std::lgamma>;

template <typename T>
using ceil = detail::math_op<T, std::ceil>;

template <typename T>
using floor = detail::math_op<T, std::floor>;

template <typename T>
using trunc = detail::math_op<T, std::trunc>;

template <typename T>
using round = detail::math_op<T, std::round>;

template <typename T>
struct lerp : public detail::binary_op<T> {
  using base = detail::binary_op<T>;
  using base::pos0;
  using base::pos1;

  double t;
  double val;

  explicit lerp(double t, size_t pos0 = 0, size_t pos1 = 0) : base{pos0, pos1}, t{t} {}

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    val = std::lerp(in[0][pos0], in[1][pos1], t);
  }

  void inverse(T, double const *const *) noexcept override {}

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");
    *out = val;
  }
};

} // namespace opflow::op
