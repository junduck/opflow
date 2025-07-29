#pragma once

#include <limits>
#include <stdexcept>
#include <utility>

namespace opflow {
template <typename Container, typename Key, typename Value>
concept associative = requires(Container const c) {
  // query using find
  { c.find(std::declval<Key>()) } -> std::same_as<typename Container::const_iterator>;
  { c.end() } -> std::same_as<typename Container::const_iterator>;

  // check iterator returns [key, value] pairs
  { std::declval<typename Container::const_iterator>()->first } -> std::convertible_to<Key const &>;
  { std::declval<typename Container::const_iterator>()->second } -> std::convertible_to<Value const &>;
};

template <typename Time>
Time min_time() noexcept {
  return Time::min(); // Use min time for non-arithmetic types
}

template <typename Time>
Time min_time() noexcept
  requires(std::is_arithmetic_v<Time>)
{
  return std::numeric_limits<Time>::min();
}

template <typename NodeType>
class node_error : public std::runtime_error {
public:
  using node_type = NodeType;

  node_error(auto &&msg, node_type const &node) : std::runtime_error(std::forward<decltype(msg)>(msg)), node(node) {}
  node_type const &get_node() const noexcept { return node; }

private:
  node_type node;
};

template <typename T>
using duration_t = decltype(std::declval<T>() - std::declval<T>());

// convenience constants

template <std::floating_point T>
constexpr inline T fnan = std::numeric_limits<T>::quiet_NaN(); ///< NaN constant
template <std::floating_point T>
constexpr inline T finf = std::numeric_limits<T>::infinity(); ///< Infinity constant
template <std::floating_point T>
constexpr inline T fmin = std::numeric_limits<T>::min(); ///< Minimum double value
template <std::floating_point T>
constexpr inline T fmax = std::numeric_limits<T>::max(); ///< Maximum double value
} // namespace opflow
