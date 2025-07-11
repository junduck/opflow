#pragma once

#include <optional>
#include <random>
#include <ranges>
#include <type_traits>

namespace utils {
namespace detail {
template <typename Func>
class gen_view_impl : public std::ranges::view_interface<gen_view_impl<Func>> {
  Func func_;
  std::size_t count_;

public:
  gen_view_impl() = default;
  gen_view_impl(Func func, std::size_t count) : func_(std::move(func)), count_(count) {}

  class iterator {
    Func *func_;
    std::size_t i_;

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::invoke_result_t<Func &>;
    using iterator_category = std::input_iterator_tag;

    iterator() = default;
    iterator(Func *func, std::size_t i) : func_(func), i_(i) {}

    value_type operator*() const { return (*func_)(); }

    iterator &operator++() {
      ++i_;
      return *this;
    }
    void operator++(int) { ++*this; }

    bool operator==(const iterator &other) const { return i_ == other.i_; }
    bool operator!=(const iterator &other) const { return !(*this == other); }
  };

  iterator begin() { return iterator(&func_, 0); }
  iterator end() { return iterator(&func_, count_); }
};

template <typename Func>
auto gen_view(Func func, std::size_t count) {
  return gen_view_impl<Func>(std::move(func), count);
}
} // namespace detail

template <typename T>
concept arithmetic = std::is_arithmetic_v<T>;

// Generate a uniform range of arithmetic values
template <arithmetic T>
auto make_unif_range(size_t n, T min, T max, std::optional<unsigned int> seed = std::nullopt) {
  std::mt19937 gen;
  if (seed.has_value()) {
    gen.seed(seed.value());
  } else {
    std::random_device rd;
    gen.seed(rd());
  }

  if constexpr (std::floating_point<T>) {
    std::uniform_real_distribution<T> dist(min, max);
    return detail::gen_view([gen = std::move(gen), dist]() mutable { return dist(gen); }, n);
  } else {
    std::uniform_int_distribution<T> dist(min, max);
    return detail::gen_view([gen = std::move(gen), dist]() mutable { return dist(gen); }, n);
  }
}

template <typename Range>
auto make_unif_choice(size_t n, Range &&choices, std::optional<unsigned int> seed = std::nullopt) {
  std::mt19937 gen;
  if (seed.has_value()) {
    gen.seed(seed.value());
  } else {
    std::random_device rd;
    gen.seed(rd());
  }

  std::uniform_int_distribution<size_t> dist(0, std::ranges::size(choices) - 1);
  return detail::gen_view(
      [gen = std::move(gen), dist, choices = std::forward<Range>(choices)]() mutable { return choices[dist(gen)]; }, n);
}

template <typename Range>
auto make_unif_shuffle(Range &&range, std::optional<unsigned int> seed = std::nullopt) {
  std::mt19937 gen;
  if (seed.has_value()) {
    gen.seed(seed.value());
  } else {
    std::random_device rd;
    gen.seed(rd());
  }

  auto shuffled_range = range;
  std::ranges::shuffle(shuffled_range, gen);
  return shuffled_range;
}
} // namespace utils
