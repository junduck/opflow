#pragma once

#include <chrono>
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

// Concepts

template <typename R, typename U>
concept range_of = std::ranges::forward_range<R> && std::convertible_to<std::ranges::range_value_t<R>, U>;

template <typename R, typename U>
concept sized_range_of = range_of<R, U> && std::ranges::sized_range<R>;

template <typename R, typename U>
concept range_derived_from = std::ranges::forward_range<R> && std::derived_from<std::ranges::range_value_t<R>, U>;

template <typename T>
concept arithmetic = std::is_arithmetic_v<T>;

template <typename T>
concept dag_node = requires(T const *t) {
  typename T::data_type;
  { t->num_inputs() } -> std::convertible_to<size_t>;
  { t->num_outputs() } -> std::convertible_to<size_t>;
  // special in-place clone for arena/object pool
  { t->clone_at(std::declval<void *>()) };
  { t->clone_size() } -> std::convertible_to<size_t>;
  { t->clone_align() } -> std::convertible_to<size_t>;
};

template <typename T>
concept dag_node_ptr = (std::is_pointer_v<T> && dag_node<std::remove_pointer_t<T>>) ||
                       (detail::smart_ptr<T> && dag_node<typename std::remove_cvref_t<T>::element_type>);

// Utilities

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

template <dag_node T>
struct dag_root;

template <dag_node T>
using dag_root_type = typename dag_root<T>::type;
} // namespace opflow
