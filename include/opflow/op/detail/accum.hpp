#pragma once

#include <cmath>

namespace opflow::op::detail {
/**
 * @brief Default accumulator
 *
 * This class uses Kahan summation to improve numerical stability
 *
 */
class accum {
  double sum, carry;
  inline void _add(double x) {
    double const y = x - carry;
    double const t = sum + y;
    carry = (t - sum) - y;
    sum = t;
  }

public:
  void add(double x) noexcept { _add(x); }
  void sub(double x) noexcept { _add(-x); }
  void addsub(double x0, double x1) noexcept { _add(x0 - x1); }

  double value() const noexcept { return sum; }
  operator double() const noexcept { return sum; }

  accum &operator=(double s) noexcept {
    sum = s;
    carry = 0;
    return *this;
  }
};

/**
 * @brief Default smoothing accumulator
 *
 * This class uses fma (fused multiply-add) to improve numerical stability
 *
 */
class smooth {
  double val;

public:
  void add(double x, double w) noexcept { val = std::fma(w, x - val, val); }
  void sub(double x, double w) noexcept { val = std::fma(w, val - x, val); }
  void addsub(double x0, double x1, double w) noexcept { val = std::fma(w, x0 - x1, val); }

  double value() const noexcept { return val; }
  operator double() const noexcept { return val; }

  smooth &operator=(double x) noexcept {
    val = x;
    return *this;
  }
};
} // namespace opflow::op::detail
