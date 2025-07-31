#pragma once

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

// Convenience constants

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

constexpr inline char name_chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";

struct str_hash {
  using is_transparent = void; ///< Enable transparent hashing
  std::size_t operator()(std::string_view str) const noexcept { return std::hash<std::string_view>{}(str); }
  std::size_t operator()(std::string const &str) const noexcept { return std::hash<std::string>{}(str); }
  std::size_t operator()(char const *str) const noexcept { return std::hash<std::string_view>{}(str); }
};

template <size_t N = 6, std::uniform_random_bit_generator G>
std::string random_string(G &gen) {
  std::uniform_int_distribution<size_t> dist(0, sizeof(name_chars) - 2); // Exclude null terminator

  std::string str(N, '\0');
  for (size_t i = 0; i < N; ++i) {
    str[i] = name_chars[dist(gen)];
  }
  return str;
}

template <size_t N = 6, std::uniform_random_bit_generator G>
std::string random_name(std::string_view prefix, G &gen) {
  std::uniform_int_distribution<size_t> dist(0, sizeof(name_chars) - 2); // Exclude null terminator

  std::string name(prefix.size() + N, '\0');
  std::copy(prefix.begin(), prefix.end(), name.begin());
  for (size_t i = prefix.size(); i < name.size(); ++i) {
    name[i] = name_chars[dist(gen)];
  }
  return name;
}

template <typename Container, typename Key, typename Value>
concept associative = requires(Container const c) {
  // query using find
  { c.find(std::declval<Key>()) } -> std::same_as<typename Container::const_iterator>;
  { c.end() } -> std::same_as<typename Container::const_iterator>;

  // check iterator returns [key, value] pairs
  { std::declval<typename Container::const_iterator>()->first } -> std::convertible_to<Key const &>;
  { std::declval<typename Container::const_iterator>()->second } -> std::convertible_to<Value const &>;
};
} // namespace opflow
