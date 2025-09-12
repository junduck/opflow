#pragma once

#include <bit>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace opflow::detail {
template <std::unsigned_integral T>
struct offset_type {
  T offset; ///< Pointer offset
  T size;   ///< Size

  friend bool operator==(offset_type const &lhs, offset_type const &rhs) noexcept = default;
};

template <typename... Ts>
struct overload : Ts... {
  using Ts::operator()...;
};
template <typename... Ts>
overload(Ts...) -> overload<Ts...>;

constexpr inline size_t aligned_size(size_t size, size_t align) noexcept { return (size + align - 1) & ~(align - 1); }

template <typename T>
struct ptr_hash {
  using is_transparent = void;

  std::size_t operator()(T *ptr) const noexcept { return std::hash<T const *>()(ptr); }
  std::size_t operator()(T const *ptr) const noexcept { return std::hash<T const *>()(ptr); }
  std::size_t operator()(std::shared_ptr<T> const &ptr) const noexcept { return std::hash<T const *>()(ptr.get()); }
  std::size_t operator()(std::unique_ptr<T> const &ptr) const noexcept { return std::hash<T const *>()(ptr.get()); }
};

struct str_hash {
  using is_transparent = void; ///< Enable transparent hashing
  std::size_t operator()(std::string_view str) const noexcept { return std::hash<std::string_view>{}(str); }
  std::size_t operator()(std::string const &str) const noexcept { return std::hash<std::string>{}(str); }
  std::size_t operator()(char const *str) const noexcept { return std::hash<std::string_view>{}(str); }
};

#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201703L
// std::hardware_destructive_interference_size reports 64 for Apple Silicon as of Apple clang version 17.0.0
// (clang-1700.0.13.5), but 128 should be used as reported by sysctl: hw.cachelinesize: 128
#if defined(__APPLE__) && defined(__arm64__)
constexpr inline size_t cacheline_size = 128;
#else
constexpr inline size_t cacheline_size = std::hardware_destructive_interference_size;
#endif
#else
constexpr inline size_t cacheline_size = 64; // Default to 64 bytes
#endif

// Fast bit operations for cacheline size (which is always a power of 2)
constexpr inline size_t cacheline_shift = std::countr_zero(cacheline_size);
constexpr inline size_t cacheline_mask = cacheline_size - 1;

template <typename T, typename A>
size_t heap_alloc_size(std::vector<T, A> const &, size_t intended_size) noexcept {
  return aligned_size(sizeof(T) * intended_size, alignof(T));
}

template <typename T>
concept trivial = std::is_trivial_v<T>;

template <typename T, size_t Align>
struct alignas(Align) aligned_type : public T {
  using T::T;
};

template <typename Alloc, typename T>
using rebind_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;

template <typename T>
concept string_like = std::convertible_to<T, std::string_view>;

template <typename T>
concept has_data_type = requires { typename T::data_type; };
} // namespace opflow::detail
