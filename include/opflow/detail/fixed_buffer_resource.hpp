#pragma once

#include <memory_resource>

#include "utils.hpp"

namespace opflow::detail {
class fixed_buffer_resource : public std::pmr::memory_resource {
  std::byte *buffer_;
  std::byte *curr_;
  std::byte *end_;

public:
  fixed_buffer_resource(void *buffer, std::size_t capacity)
      : buffer_(static_cast<std::byte *>(buffer)), curr_(buffer_), end_(buffer_ + capacity) {}

protected:
  void *do_allocate(std::size_t bytes, std::size_t alignment) override {
    auto uptr = reinterpret_cast<uintptr_t>(curr_);
    auto aligned = reinterpret_cast<std::byte *>(aligned_size(uptr, alignment));
    auto new_curr = aligned + bytes;
    if (new_curr > end_) {
      throw std::bad_alloc(); // Out of memory
    }
    curr_ = new_curr;
    return aligned;
  }

  // No-op: monotonic, no deallocation
  void do_deallocate(void *, std::size_t, std::size_t) noexcept override {}

  bool do_is_equal(std::pmr::memory_resource const &other) const noexcept override { return this == &other; }
};

template <typename T>
struct arena_deleter {
  void operator()(T *ptr) const noexcept {
    // memory is reclaimed when arena is destroyed, no dealloc needed
    ptr->~T();
  }
};
} // namespace opflow::detail
