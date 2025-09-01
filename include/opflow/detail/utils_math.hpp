#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>

namespace opflow::detail {
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

template <std::floating_point U>
struct lerp {
  U const t;
  explicit lerp(U t) noexcept : t(t) {}
  U operator()(U a, U b) const noexcept { return std::lerp(a, b, t); }
};

template <std::floating_point U>
struct clamp {
  U const lo;
  U const hi;
  explicit clamp(U lo, U hi) noexcept : lo(lo), hi(hi) {}
  U operator()(U a) const noexcept { return std::clamp(a, lo, hi); }
};
} // namespace opflow::detail
