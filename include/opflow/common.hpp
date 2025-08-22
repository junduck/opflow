#pragma once

#include <chrono>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace opflow {
// Time

template <typename Time>
using duration_t = decltype(std::declval<Time>() - std::declval<Time>());

template <typename Time>
Time min_time() noexcept {
  if constexpr (std::is_arithmetic_v<Time>) {
    return std::numeric_limits<Time>::min();
  } else {
    return Time::min(); // Use min time for non-arithmetic types
  }
}

template <typename Time>
Time max_time() noexcept {
  if constexpr (std::is_arithmetic_v<Time>) {
    return std::numeric_limits<Time>::max();
  } else {
    return Time::max(); // Use max time for non-arithmetic types
  }
}

template <typename Data>
struct static_cast_conv {
  template <typename T>
  auto operator()(T v) const noexcept {
    return static_cast<Data>(v);
  }
};

template <typename Data, typename Ratio>
struct chrono_conv {
  template <typename T>
  auto operator()(T timestamp) const noexcept {
    using target_t = std::chrono::duration<Data, Ratio>;
    auto dur_epoch = timestamp.time_since_epoch();
    auto dur_target = std::chrono::duration_cast<target_t>(dur_epoch);
    return dur_target.count();
  }
};

template <typename Data>
using chrono_us_conv = chrono_conv<Data, std::micro>;

template <typename Data>
using chrono_ms_conv = chrono_conv<Data, std::milli>;

template <typename Data>
using chrono_s_conv = chrono_conv<Data, std::ratio<1>>;

template <typename Data>
using chrono_min_conv = chrono_conv<Data, std::chrono::minutes::period>;

template <typename Data>
using chrono_h_conv = chrono_conv<Data, std::chrono::hours::period>;

// Error handling

template <typename NodeType>
class node_error : public std::runtime_error {
public:
  using node_type = NodeType;

  node_error(auto &&msg, node_type const &node) : std::runtime_error(std::forward<decltype(msg)>(msg)), node(node) {}
  node_type const &get_node() const noexcept { return node; }

private:
  node_type node;
};

// Concepts

template <typename R, typename U>
concept range_of = std::ranges::forward_range<R> && std::convertible_to<std::ranges::range_value_t<R>, U>;

template <typename R, typename U>
concept range_derived_from = std::ranges::forward_range<R> && std::derived_from<std::ranges::range_value_t<R>, U>;

template <typename T>
concept arithmetic = std::is_arithmetic_v<T>;

template <typename Container, typename Key, typename Value>
concept associative = requires(Container const c) {
  // query using find
  { c.find(std::declval<Key>()) } -> std::same_as<typename Container::const_iterator>;
  { c.end() } -> std::sentinel_for<typename Container::const_iterator>;
} && requires(std::ranges::range_value_t<Container> v) {
  // value type
  { std::get<0>(v) } -> std::convertible_to<Key const &>;
  { std::get<1>(v) } -> std::convertible_to<Value const &>;
};

template <typename T>
concept smart_ptr = requires(T t) {
  typename std::remove_cvref_t<T>::element_type;
  { t.get() } -> std::same_as<typename std::remove_cvref_t<T>::element_type *>;
  { t.operator->() } -> std::same_as<typename std::remove_cvref_t<T>::element_type *>;
};

template <typename T>
concept dag_node = requires(T const *t) {
  typename T::data_type;
  // observer, should return base const pointer, don't have type here yet
  { t->observer() };
  // special in-place clone for arena/object pool
  { t->clone_at(std::declval<void *>()) };
  { t->clone_size() } -> std::convertible_to<size_t>;
  { t->clone_align() } -> std::convertible_to<size_t>;
};

template <typename T>
concept dag_node_ptr = (std::is_pointer_v<T> && dag_node<std::remove_pointer_t<T>>) ||
                       (smart_ptr<T> && dag_node<typename std::remove_cvref_t<T>::element_type>);

template <std::floating_point T>
constexpr inline T feps = std::numeric_limits<T>::epsilon(); ///< Epsilon constant
template <std::floating_point T>
constexpr inline T feps100 = T(100) * feps<T>; ///< Epsilon constant scaled by 100
template <std::floating_point T>
constexpr inline T fnan = std::numeric_limits<T>::quiet_NaN(); ///< NaN constant
template <std::floating_point T>
constexpr inline T finf = std::numeric_limits<T>::infinity(); ///< Infinity constant
template <std::floating_point T>
constexpr inline T fmin = std::numeric_limits<T>::min(); ///< Minimum double value
template <std::floating_point T>
constexpr inline T fmax = std::numeric_limits<T>::max(); ///< Maximum double value

// Utilities

constexpr inline size_t aligned_size(size_t size, size_t align) noexcept { return (size + align - 1) & ~(align - 1); }

constexpr inline char name_chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";

struct str_hash {
  using is_transparent = void; ///< Enable transparent hashing
  std::size_t operator()(std::string_view str) const noexcept { return std::hash<std::string_view>{}(str); }
  std::size_t operator()(std::string const &str) const noexcept { return std::hash<std::string>{}(str); }
  std::size_t operator()(char const *str) const noexcept { return std::hash<std::string_view>{}(str); }
};

template <size_t N = 6, std::uniform_random_bit_generator G>
std::string random_string(G &gen, std::string_view prefix = "") {
  std::uniform_int_distribution<size_t> dist(0, sizeof(name_chars) - 2); // Exclude null terminator

  std::string str(prefix.size() + N, '\0');
  std::copy(prefix.begin(), prefix.end(), str.begin());
  for (size_t i = prefix.size(); i < str.size(); ++i) {
    str[i] = name_chars[dist(gen)];
  }
  return str;
}

struct strict_bool {
  bool value;

  constexpr strict_bool() noexcept : value(false) {}
  constexpr strict_bool(bool v) noexcept : value(v) {}

  template <typename T>
  constexpr strict_bool(T) = delete;

  template <typename T>
  strict_bool &operator=(T) = delete;

  constexpr explicit operator bool() const noexcept { return value; }
  constexpr bool operator!() const noexcept { return !value; }

  friend constexpr bool operator==(strict_bool const &lhs, strict_bool const &rhs) = default;
};

template <typename... Ts>
struct overload : Ts... {
  using Ts::operator()...;
};
template <typename... Ts>
overload(Ts...) -> overload<Ts...>;
} // namespace opflow
