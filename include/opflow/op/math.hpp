#pragma once

#include <cmath>

#include "opflow/op/detail/binary.hpp"
#include "opflow/op/detail/unary.hpp"
#include "opflow/op_base.hpp"

namespace opflow::op {
namespace detail {
using unary_math_fptr = double (*)(double);

template <time_point_like T, unary_math_fptr Fn>
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

template <time_point_like T, binary_math_fptr Fn>
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

template <time_point_like T>
using add = detail::math_bin_op<T, detail::add>;

template <time_point_like T>
using sub = detail::math_bin_op<T, detail::sub>;

template <time_point_like T>
using mul = detail::math_bin_op<T, detail::mul>;

template <time_point_like T>
using div = detail::math_bin_op<T, detail::div>;

template <time_point_like T>
using fmod = detail::math_bin_op<T, detail::fmod>;

template <time_point_like T>
using inv = detail::math_op<T, detail::inv>;

template <time_point_like T>
using neg = detail::math_op<T, detail::neg>;

// from <cmath>

template <time_point_like T>
using abs = detail::math_op<T, std::abs>;

template <time_point_like T>
using exp = detail::math_op<T, std::exp>;

template <time_point_like T>
using expm1 = detail::math_op<T, std::expm1>;

template <time_point_like T>
using log = detail::math_op<T, std::log>;

template <time_point_like T>
using log10 = detail::math_op<T, std::log10>;

template <time_point_like T>
using log2 = detail::math_op<T, std::log2>;

template <time_point_like T>
using log1p = detail::math_op<T, std::log1p>;

template <time_point_like T>
using sqrt = detail::math_op<T, std::sqrt>;

template <time_point_like T>
using cbrt = detail::math_op<T, std::cbrt>;

template <time_point_like T>
using sin = detail::math_op<T, std::sin>;

template <time_point_like T>
using cos = detail::math_op<T, std::cos>;

template <time_point_like T>
using tan = detail::math_op<T, std::tan>;

template <time_point_like T>
using asin = detail::math_op<T, std::asin>;

template <time_point_like T>
using acos = detail::math_op<T, std::acos>;

template <time_point_like T>
using atan = detail::math_op<T, std::atan>;

template <time_point_like T>
using sinh = detail::math_op<T, std::sinh>;

template <time_point_like T>
using cosh = detail::math_op<T, std::cosh>;

template <time_point_like T>
using tanh = detail::math_op<T, std::tanh>;

template <time_point_like T>
using asinh = detail::math_op<T, std::asinh>;

template <time_point_like T>
using acosh = detail::math_op<T, std::acosh>;

template <time_point_like T>
using atanh = detail::math_op<T, std::atanh>;

template <time_point_like T>
using erf = detail::math_op<T, std::erf>;

template <time_point_like T>
using erfc = detail::math_op<T, std::erfc>;

template <time_point_like T>
using tgamma = detail::math_op<T, std::tgamma>;

template <time_point_like T>
using lgamma = detail::math_op<T, std::lgamma>;

template <time_point_like T>
using ceil = detail::math_op<T, std::ceil>;

template <time_point_like T>
using floor = detail::math_op<T, std::floor>;

template <time_point_like T>
using trunc = detail::math_op<T, std::trunc>;

template <time_point_like T>
using round = detail::math_op<T, std::round>;

template <time_point_like T>
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
