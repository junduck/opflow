#pragma once

#include <cassert>
#include <deque>
#include <span>
#include <vector>

#include "impl/iterator.hpp"

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
  using value_type = std::pair<T, std::span<U>>;
  using const_value_type = std::pair<T, std::span<U const>>;

  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

private:
  std::deque<T> tick;               ///< Ticks for each step
  std::deque<std::vector<U>> value; ///< Values for each step
  size_type value_size;             ///< Size of each value vector

public:
  explicit history_deque(size_type val_size) : value_size(val_size) {}

  history_deque() = default;

  void init(size_type val_size) {
    assert(val_size > 0 && "value size must be greater than 0");
    value_size = val_size;
    tick.clear();
    value.clear();
  }

  // Push data to back
  // @pre data.size() == value_size
  template <std::ranges::sized_range R>
  value_type push(T t, R &&data) {
    assert(value_size > 0 && "history buffer not initialised");
    assert(std::ranges::size(data) == value_size && "Wrong data dimension");

    tick.push_back(t);
    value.emplace_back(std::ranges::begin(data), std::ranges::end(data));

    return {t, {value.back().data(), value_size}};
  }

  // Push empty entry to back and return mutable span for in-place writing
  [[nodiscard]] value_type push(T t) {
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
  value_type operator[](size_type idx) {
    assert(idx < tick.size() && "Index out of bounds");
    return {tick[idx], {value[idx].data(), value_size}};
  }

  const_value_type operator[](size_type idx) const {
    assert(idx < tick.size() && "Index out of bounds");
    return {tick[idx], {value[idx].data(), value_size}};
  }

  // Access elements from the back (0 = back, size()-1 = front)
  value_type from_back(size_type back_idx) {
    assert(back_idx < tick.size() && "Index out of bounds");
    auto rev_idx = tick.size() - 1 - back_idx; // Reverse index
    return {tick[rev_idx], {value[rev_idx].data(), value_size}};
  }

  const_value_type from_back(size_type back_idx) const {
    assert(back_idx < tick.size() && "Index out of bounds");
    auto rev_idx = tick.size() - 1 - back_idx; // Reverse index
    return {tick[rev_idx], {value[rev_idx].data(), value_size}};
  }

  value_type front() {
    assert(!tick.empty() && "Index out of bounds");
    return {tick.front(), {value.front().data(), value_size}};
  }

  const_value_type front() const {
    assert(!tick.empty() && "Index out of bounds");
    return {tick.front(), {value.front().data(), value_size}};
  }

  value_type back() {
    assert(!tick.empty() && "Index out of bounds");
    return {tick.back(), {value.back().data(), value_size}};
  }

  const_value_type back() const {
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

  using iterator = impl::iterator_t<history_deque, false>;
  using const_iterator = impl::iterator_t<history_deque, true>;
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
} // namespace opflow
