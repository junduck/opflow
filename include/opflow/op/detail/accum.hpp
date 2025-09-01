#pragma once

#include <cmath>
#include <concepts>

namespace opflow::op::detail {
/**
 * @brief Default accumulator
 *
 * This class uses Kahan summation to improve numerical stability
 *
 */
template <std::floating_point T>
class accum {
  T sum, carry;
  inline void _add(T x) {
    T const y = x - carry;
    T const t = sum + y;
    carry = (t - sum) - y;
    sum = t;
  }

public:
  void add(T x) noexcept { _add(x); }
  void sub(T x) noexcept { _add(-x); }
  void addsub(T x0, T x1) noexcept { _add(x0 - x1); }

  T value() const noexcept { return sum; }
  operator T() const noexcept { return sum; }

  accum &operator=(T s) noexcept {
    sum = s;
    carry = 0;
    return *this;
  }

  void reset() noexcept {
    sum = T{};
    carry = T{};
  }
};

/**
 * @brief Default smoothing accumulator
 *
 * This class uses fma (fused multiply-add) to improve numerical stability
 *
 */
template <std::floating_point T>
class smooth {
  T val;

public:
  void add(T x, T w) noexcept { val = std::fma(w, x - val, val); }
  void sub(T x, T w) noexcept { val = std::fma(w, val - x, val); }
  void addsub(T x0, T x1, T w) noexcept { val = std::fma(w, x0 - x1, val); }

  T value() const noexcept { return val; }
  operator T() const noexcept { return val; }

  smooth &operator=(T x) noexcept {
    val = x;
    return *this;
  }

  void reset() noexcept { val = T{}; }
};

/**
 * @brief Smooth factor calculation
 *
 * This function calculates the smooth factor based on the alpha value.
 * If alpha is greater than or equal to 1, it treats it as a period and
 * computes the smooth factor accordingly.
 *
 * @param alpha The alpha value (or period)
 * @return The computed smooth factor
 */
template <std::floating_point U>
U smooth_factor(U alpha) noexcept {
  if (alpha >= 1) {
    // alpha is actually a period
    return U(2) / (alpha + 1);
  }
  return alpha;
}

/**
 * @brief Wilders' smoothing factor calculation
 *
 * This function calculates the Wilders' smoothing factor based on the alpha value.
 * If alpha is greater than or equal to 1, it treats it as a period and computes
 * the smoothing factor accordingly.
 *
 * @param alpha The alpha value (or period)
 * @return The computed Wilders' smoothing factor
 */
template <std::floating_point U>
U smooth_wilders(U alpha) noexcept {
  if (alpha >= 1) {
    // alpha is actually a period
    return U(1) / (alpha + 1);
  }
  return alpha;
}
} // namespace opflow::op::detail
