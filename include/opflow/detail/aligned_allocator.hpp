#pragma once

#include "utils.hpp"

namespace opflow::detail {
template <typename T, std::size_t Alignment>
struct aligned_allocator {
  static_assert(Alignment && (Alignment & (Alignment - 1)) == 0, "Alignment must be a power of two");

  using value_type = T;
  using pointer = T *;
  using const_pointer = T const *;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  template <class U>
  struct rebind {
    using other = aligned_allocator<U, Alignment>;
  };

  constexpr aligned_allocator() noexcept = default;

  template <typename U>
  constexpr aligned_allocator(aligned_allocator<U, Alignment> const &) noexcept {}

  pointer allocate(size_type n) {
    if (n == 0)
      return nullptr;

    // std::aligned_alloc requires size to be a multiple of alignment
    size_type aligned_bytes = aligned_size(n * sizeof(T), Alignment);
    return static_cast<pointer>(std::aligned_alloc(Alignment, aligned_bytes));
  }

  void deallocate(pointer ptr, size_type) noexcept {
    if (ptr == nullptr)
      return;

    std::free(ptr);
  }

  size_type max_size() const noexcept { return (std::numeric_limits<size_type>::max)() / sizeof(T); }

  template <typename U>
  constexpr bool operator==(aligned_allocator<U, Alignment> const &) const noexcept {
    return true;
  }

  template <typename U>
  constexpr bool operator!=(aligned_allocator<U, Alignment> const &) const noexcept {
    return false;
  }
};

template <typename T>
using cacheline_aligned_alloc = aligned_allocator<T, cacheline_size>;
} // namespace opflow::detail
