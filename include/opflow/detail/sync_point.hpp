#pragma once

#include <atomic>

#include "utils.hpp"

namespace opflow::detail {
struct alignas(detail::cacheline_size) sync_point {
  std::atomic<size_t> seq;

  void enter() noexcept { seq.load(std::memory_order::acquire); }
  void exit() noexcept { seq.fetch_add(1, std::memory_order::release); }
};
} // namespace opflow::detail
