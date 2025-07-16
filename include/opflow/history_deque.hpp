#pragma once

#include <cassert>
#include <deque>
#include <vector>

#include "history.hpp"
#include "impl/step_view.hpp"

namespace opflow {
/**
 * @brief History container implementation using std::deque for storage
 *
 * This implementation provides dynamic growth without the memory layout constraints
 * of a ring buffer. Each value is stored as a separate std::vector, making it
 * suitable for scenarios where memory usage is less critical than implementation
 * simplicity.
 *
 * @tparam T Tick type (e.g., time, step counter)
 * @tparam U Value type for data elements
 */
template <typename T, typename U>
class history_deque {
public:
  template <bool IsConst>
  using step_view_t = impl::step_view_t<T, U, IsConst>;
  using step_view = step_view_t<false>;
  using const_step_view = step_view_t<true>;

  using value_type = const_step_view;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

private:
  std::deque<T> tick;               ///< Ticks for each step
  std::deque<std::vector<U>> value; ///< Values for each step
  size_type value_size;             ///< Size of each value vector

public:
  explicit history_deque(size_type val_size, size_type initial_capacity = 16) : value_size(val_size) {
    // Reserve space if initial_capacity is provided (deque doesn't have reserve, but we can hint)
    std::ignore = initial_capacity;
  }

  // Push data to back
  // @pre data.size() == value_size
  template <std::ranges::sized_range R>
  step_view push(T t, R &&data) {
    assert(std::ranges::size(data) == value_size && "Wrong data dimension");

    tick.push_back(t);
    value.emplace_back(std::ranges::begin(data), std::ranges::end(data));

    return {t, {value.back().data(), value_size}};
  }

  // Push empty entry to back and return mutable span for in-place writing
  [[nodiscard]] step_view push(T t) {
    tick.push_back(t);
    value.emplace_back(value_size); // Create vector with value_size elements

    return {t, {value.back().data(), value_size}};
  }

  // Pop the front element
  void pop() noexcept {
    if (!tick.empty()) {
      tick.pop_front();
      value.pop_front();
    }
  }

  // Return data at slot idx (0 = front, size()-1 = back)
  step_view operator[](size_type idx) {
    assert(idx < tick.size() && "Index out of bounds");
    return {tick[idx], {value[idx].data(), value_size}};
  }

  const_step_view operator[](size_type idx) const {
    assert(idx < tick.size() && "Index out of bounds");
    return {tick[idx], {value[idx].data(), value_size}};
  }

  step_view front() {
    assert(!tick.empty() && "Index out of bounds");
    return {tick.front(), {value.front().data(), value_size}};
  }

  const_step_view front() const {
    assert(!tick.empty() && "Index out of bounds");
    return {tick.front(), {value.front().data(), value_size}};
  }

  step_view back() {
    assert(!tick.empty() && "Index out of bounds");
    return {tick.back(), {value.back().data(), value_size}};
  }

  const_step_view back() const {
    assert(!tick.empty() && "Index out of bounds");
    return {tick.back(), {value.back().data(), value_size}};
  }

  // Utilities
  size_type size() const noexcept { return tick.size(); }

  bool empty() const noexcept { return tick.empty(); }

  void clear() noexcept {
    tick.clear();
    value.clear();
  }

  void reserve(size_type new_capacity) {
    // std::deque doesn't have reserve, but we can document the intention
    // This method exists for API compatibility with history_ringbuf
    std::ignore = new_capacity;
  }

  // Additional standard container methods
  size_type max_size() const noexcept { return std::min(tick.max_size(), value.max_size()); }

  // Iterator support for range-based loops
  template <bool IsConst>
  class iterator_t {
  public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = step_view_t<IsConst>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type *;
    using reference = value_type;

  private:
    using history_ptr = std::conditional_t<IsConst, history_deque const *, history_deque *>;
    history_ptr hist;
    size_type index;

  public:
    iterator_t() : hist(nullptr), index(0) {}
    iterator_t(history_ptr h, size_type idx) : hist(h), index(idx) {}

    // Allow conversion from non-const to const iterator
    template <bool OtherConst>
    iterator_t(iterator_t<OtherConst> const &other)
      requires(IsConst && !OtherConst)
        : hist(other.hist), index(other.index) {}

    reference operator*() const { return hist->operator[](index); }

    iterator_t &operator++() {
      ++index;
      return *this;
    }

    iterator_t operator++(int) {
      iterator_t tmp = *this;
      ++index;
      return tmp;
    }

    iterator_t &operator--() {
      --index;
      return *this;
    }

    iterator_t operator--(int) {
      iterator_t tmp = *this;
      --index;
      return tmp;
    }

    iterator_t &operator+=(difference_type n) {
      index = static_cast<size_type>(static_cast<difference_type>(index) + n);
      return *this;
    }

    iterator_t &operator-=(difference_type n) {
      index = static_cast<size_type>(static_cast<difference_type>(index) - n);
      return *this;
    }

    iterator_t operator+(difference_type n) const {
      iterator_t tmp = *this;
      tmp += n;
      return tmp;
    }

    iterator_t operator-(difference_type n) const {
      iterator_t tmp = *this;
      tmp -= n;
      return tmp;
    }

    difference_type operator-(iterator_t const &other) const {
      return static_cast<difference_type>(index) - static_cast<difference_type>(other.index);
    }

    reference operator[](difference_type n) const {
      iterator_t tmp = *this;
      tmp += n;
      return *tmp;
    }

    auto operator<=>(iterator_t const &other) const = default;

    template <bool OtherConst>
    friend class iterator_t;
  };

  using iterator = iterator_t<false>;
  using const_iterator = iterator_t<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, size()); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, size()); }
  const_iterator cbegin() const { return const_iterator(this, 0); }
  const_iterator cend() const { return const_iterator(this, size()); }

  reverse_iterator rbegin() { return reverse_iterator(end()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
  const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
  const_reverse_iterator crbegin() const { return const_reverse_iterator(cend()); }
  const_reverse_iterator crend() const { return const_reverse_iterator(cbegin()); }
};

static_assert(history_container<history_deque<int, double>, int, double>);
} // namespace opflow
