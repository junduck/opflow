#pragma once

#include <utility>

#include "../def.hpp"

namespace opflow::detail {
template <typename Fn>
class finally {
public:
  finally(Fn f) noexcept : fn(std::move(f)), active(true) {}
  finally(finally &&other) noexcept : fn(std::move(other.fn)), active(other.active) { other.active = false; }
  ~finally() {
    if (active)
      fn();
  }

private:
  OPFLOW_NO_UNIQUE_ADDRESS Fn fn;
  bool active;
};
} // namespace opflow::detail
