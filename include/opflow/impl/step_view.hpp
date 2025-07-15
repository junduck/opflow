#pragma once

#include <span>

namespace opflow::impl {
template <typename T, typename U, bool IsConst>
struct step_view_t {
  using tick_type = T;
  using span_type = std::conditional_t<IsConst, std::span<U const>, std::span<U>>;

  tick_type tick;
  span_type data;

  step_view_t(tick_type t, span_type d) : tick(t), data(d) {}

  // Allow conversion from non-const to const
  template <bool OtherConst>
  step_view_t(step_view_t<T, U, OtherConst> const &other)
    requires(IsConst && !OtherConst)
      : tick(other.tick), data(other.data) {}
};
} // namespace opflow::impl
