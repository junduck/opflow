#pragma once

#include <chrono>
#include <utility>

namespace opflow::chrono {
template <typename Time>
using duration_t = decltype(std::declval<Time>() - std::declval<Time>());

template <typename Time>
constexpr Time min_time() noexcept {
  if constexpr (std::is_arithmetic_v<Time>) {
    return std::numeric_limits<Time>::lowest();
  } else {
    return Time::min(); // Use min time for non-arithmetic types
  }
}

template <typename Time>
constexpr Time max_time() noexcept {
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
using conv_us_t = chrono_conv<Data, std::micro>;

template <typename Data>
constexpr inline auto conv_us = conv_us_t<Data>{};

template <typename Data>
using conv_ms_t = chrono_conv<Data, std::milli>;

template <typename Data>
constexpr inline auto conv_ms = conv_ms_t<Data>{};

template <typename Data>
using conv_s_t = chrono_conv<Data, std::ratio<1>>;

template <typename Data>
constexpr inline auto conv_s = conv_s_t<Data>{};

template <typename Data>
using conv_min_t = chrono_conv<Data, std::chrono::minutes::period>;

template <typename Data>
constexpr inline auto conv_min = conv_min_t<Data>{};

template <typename Data>
using conv_h_t = chrono_conv<Data, std::chrono::hours::period>;

template <typename Data>
constexpr inline auto conv_h = conv_h_t<Data>{};

template <typename Data>
using conv_d_t = chrono_conv<Data, std::chrono::days::period>;

template <typename Data>
constexpr inline auto conv_d = conv_d_t<Data>{};
} // namespace opflow::chrono
