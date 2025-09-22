#pragma once

#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace opflow {
namespace detail {
template <typename T>
concept smart_ptr = requires(T t) {
  typename std::remove_cvref_t<T>::element_type;
  { t.get() } -> std::same_as<typename std::remove_cvref_t<T>::element_type *>;
  { t.operator->() } -> std::same_as<typename std::remove_cvref_t<T>::element_type *>;
};

constexpr inline char name_chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
} // namespace detail

using u32 = uint32_t;

// Concepts

// For disambiguation of string literals -> range_of<decltype("test"), size_t>
template <typename R>
concept range_idx = std::ranges::sized_range<R> && std::unsigned_integral<std::ranges::range_value_t<R>>;

template <typename R, typename U>
concept range_of = std::ranges::forward_range<R> && std::convertible_to<std::ranges::range_value_t<R>, U>;

template <typename R, typename U>
concept sized_range_of = range_of<R, U> && std::ranges::sized_range<R>;

template <typename R, typename U>
concept range_derived_from = std::ranges::forward_range<R> && std::derived_from<std::ranges::range_value_t<R>, U>;

template <typename T>
concept arithmetic = std::is_arithmetic_v<T>;

template <typename T>
concept dag_node_base = requires(T const *t) {
  typename T::data_type;
  { t->num_inputs() } -> std::convertible_to<size_t>;
  { t->num_outputs() } -> std::convertible_to<size_t>;
  // special in-place clone for arena/object pool
  { t->clone_at(std::declval<void *>()) };
  { t->clone_size() } -> std::convertible_to<size_t>;
  { t->clone_align() } -> std::convertible_to<size_t>;
};

template <typename T>
concept win_node_base = requires(T *t) {
  typename T::data_type;
  typename T::spec_type;
  {
    t->on_data(std::declval<typename T::data_type>(), std::declval<typename T::data_type const *>())
  } -> std::convertible_to<bool>;
  { t->emit() } -> std::same_as<typename T::spec_type>;

  // special in-place clone for arena/object pool
  { t->clone_at(std::declval<void *>()) };
  { t->clone_size() } -> std::convertible_to<size_t>;
  { t->clone_align() } -> std::convertible_to<size_t>;
};

template <typename T>
concept dag_node_ptr = (std::is_pointer_v<T> && dag_node_base<std::remove_pointer_t<T>>) ||
                       (detail::smart_ptr<T> && dag_node_base<typename std::remove_cvref_t<T>::element_type>);

template <typename T>
concept dag_node_impl = !std::is_abstract_v<T> && dag_node_base<T>;

// Utilities

template <std::floating_point T>
constexpr inline T feps = std::numeric_limits<T>::epsilon(); ///< Epsilon constant
template <std::floating_point T>
constexpr inline T feps10 = T(10) * feps<T>; ///< Epsilon constant scaled by 10
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

template <typename T>
constexpr bool very_small(T v) {
  return v == T{};
}

template <std::floating_point T>
constexpr bool very_small(T v) {
  return std::abs(v) < feps100<T>;
}

template <size_t N = 6, std::uniform_random_bit_generator G>
std::string random_string(G &gen, std::string_view prefix = "") {
  std::uniform_int_distribution<size_t> dist(0, sizeof(detail::name_chars) - 2); // Exclude null terminator

  std::string str(prefix.size() + N, '\0');
  std::copy(prefix.begin(), prefix.end(), str.begin());
  for (size_t i = prefix.size(); i < str.size(); ++i) {
    str[i] = detail::name_chars[dist(gen)];
  }
  return str;
}

template <dag_node_base T>
struct dag_root;

template <dag_node_base T>
using dag_root_type = typename dag_root<T>::type;

struct ctor_args_tag {};
constexpr inline ctor_args_tag ctor_args{};

template <typename G>
concept executable_graph = requires(G const g) {
  typename G::key_type;
  typename G::edge_type;   // structural binding -> [key_type, u32]
  typename G::NodeSet;     // range of key_type
  typename G::NodeArgsSet; // range of edge_type
  typename G::NodeMap;     // associate container key_type -> NodeSet
  typename G::NodeArgsMap; // associate container key_type -> NodeArgsSet

  typename G::node_type;
  typename G::aux_type;
  typename G::shared_node_ptr;
  typename G::shared_aux_ptr;

  { g.node(std::declval<typename G::key_type>()) } -> dag_node_ptr;
  { g.aux() } -> std::convertible_to<typename G::shared_aux_ptr>;

  { g.pred() } -> std::convertible_to<typename G::NodeMap>;
  { g.args() } -> std::convertible_to<typename G::NodeArgsMap>;
  { g.succ() } -> std::convertible_to<typename G::NodeMap>;

  { g.aux_args() } -> std::convertible_to<typename G::NodeArgsSet>;
  { g.output() } -> std::convertible_to<typename G::NodeArgsSet>;
};
} // namespace opflow
