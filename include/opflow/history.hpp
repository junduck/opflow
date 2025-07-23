#pragma once

#include <cassert>
#include <span>

namespace opflow {
template <typename T, typename Tick, typename Data>
concept history_view = requires(T view) {
  {
    [](T v) {
      auto [tick, data] = v;
      return std::make_tuple(tick, std::span<Data>(data));
    }(view)
  } -> std::same_as<std::tuple<Tick, std::span<Data>>>;
};

template <typename T, typename Tick, typename Data>
concept history_container =
    // constructor: default constructed and intialised in later stage
    std::is_default_constructible_v<T> &&
    // size is known
    std::ranges::sized_range<T> &&
    // can iterate as history_view
    history_view<std::ranges::range_value_t<T>, Tick, Data> &&
    // other operations
    requires(T &hist) {
      // initialise
      { hist.init(std::declval<size_t>()) };
      // can push back (new entry)
      { hist.push(std::declval<Tick>(), std::declval<std::span<Data const>>()) } -> history_view<Tick, Data>;
      { hist.push(std::declval<Tick>()) } -> history_view<Tick, Data>;
      // can pop front (oldest)
      { hist.pop() };
      // can query front (oldest) and back (newest) steps
      { hist.front() } -> history_view<Tick, Data>;
      { hist.back() } -> history_view<Tick, Data>;
      // can reset
      { hist.clear() };
    };
} // namespace opflow
