#pragma once

#include <bit>
#include <cassert>
#include <type_traits>
#include <vector>

#include "history.hpp"
#include "impl/step_view.hpp"

namespace opflow {
/**
 * @brief Memory-efficient history container using ring buffer storage
 *
 * This implementation uses a ring buffer with power-of-2 capacity for efficient
 * memory usage and fast modulo operations. All values are stored in a single
 * contiguous buffer, making it cache-friendly and suitable for high-performance
 * scenarios.
 *
 * @tparam T Tick type (e.g., time, step counter)
 * @tparam U Value type for data elements
 */
template <typename T, typename U>
class history_ringbuf {
public:
  template <bool IsConst>
  using step_view_t = impl::step_view_t<T, U, IsConst>;
  using step_view = step_view_t<false>;
  using const_step_view = step_view_t<true>;

  using value_type = const_step_view;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

private:
  std::vector<T> tick;  ///< Ticks for each step, ring buffer
  std::vector<U> value; ///< Values for each step, ring buffer
  size_type value_size; ///< Size of each value vector
  size_type capacity;   ///< Current capacity of the ring buffer (always power of 2)
  size_type head;       ///< Index of the first valid element
  size_type count;      ///< Number of valid elements

  // Helper function to find next power of 2
  static constexpr size_type next_pow2(size_type n) noexcept { return n == 0 ? 1 : std::bit_ceil(n); }

public:
  explicit history_ringbuf(size_type val_size, size_type initial_capacity = 16)
      : value_size(val_size), head(0), count(0) {
    if (val_size == 0) {
      // Allow zero value_size but warn that it may not be useful
    }
    capacity = next_pow2(initial_capacity);
    tick.resize(capacity);

    // Check for potential overflow in value allocation
    if (value_size > 0 && capacity > std::numeric_limits<size_type>::max() / value_size) {
      throw std::bad_alloc();
    }
    value.resize(capacity * value_size);
  }

  // Push data to back, if buffer is full, we should ALLOCATE more space
  // @pre data.size() == value_size
  template <std::ranges::sized_range R>
  step_view push(T t, R &&data) {
    assert(std::ranges::size(data) == value_size && "Wrong data dimension");

    if (count == capacity) {
      // Check for potential overflow before doubling capacity
      if (capacity > std::numeric_limits<size_type>::max() / 2) {
        throw std::bad_alloc();
      }
      // Need to grow the buffer (double capacity, which maintains power of 2)
      resize(capacity * 2);
    }

    size_type tail_idx = (head + count) & (capacity - 1); // Use mask for fast modulo
    tick[tail_idx] = t;

    // Copy data to the value buffer
    size_type value_start = tail_idx * value_size;
    std::copy(std::ranges::begin(data), std::ranges::end(data), value.data() + value_start);

    ++count;

    return {t, {value.data() + value_start, value_size}};
  }

  // Push empty entry to back and return mutable span for in-place writing
  // Returns a step_view with mutable span to write data directly
  [[nodiscard]] step_view push(T t) {
    if (count == capacity) {
      // Check for potential overflow before doubling capacity
      if (capacity > std::numeric_limits<size_type>::max() / 2) {
        throw std::bad_alloc();
      }
      // Need to grow the buffer (double capacity, which maintains power of 2)
      resize(capacity * 2);
    }

    size_type tail_idx = (head + count) & (capacity - 1); // Use mask for fast modulo
    tick[tail_idx] = t;

    // Get mutable span for the value buffer
    size_type value_start = tail_idx * value_size;

    ++count;

    return {t, {value.data() + value_start, value_size}};
  }

  // Pop the front element
  void pop() noexcept {
    if (count == 0)
      return;

    head = (head + 1) & (capacity - 1);
    --count;
  }

  // Return data at slot idx (0 = front, size()-1 = back)
  step_view operator[](size_type idx) {
    assert(idx < count && "Index out of bounds");

    size_type actual_idx = (head + idx) & (capacity - 1);
    size_type value_start = actual_idx * value_size;
    return {tick[actual_idx], {value.data() + value_start, value_size}};
  }

  const_step_view operator[](size_type idx) const {
    assert(idx < count && "Index out of bounds");

    size_type actual_idx = (head + idx) & (capacity - 1);
    size_type value_start = actual_idx * value_size;
    return {tick[actual_idx], {value.data() + value_start, value_size}};
  }

  step_view front() {
    assert(count > 0 && "Index out of bounds");
    return {tick[head], {value.data() + head * value_size, value_size}};
  }

  const_step_view front() const {
    assert(count > 0 && "Index out of bounds");
    return {tick[head], {value.data() + head * value_size, value_size}};
  }

  step_view back() {
    assert(count > 0 && "Index out of bounds");
    size_type back_idx = (head + count - 1) & (capacity - 1);
    size_type value_start = back_idx * value_size;
    return {tick[back_idx], {value.data() + value_start, value_size}};
  }

  const_step_view back() const {
    assert(count > 0 && "Index out of bounds");
    size_type back_idx = (head + count - 1) & (capacity - 1);
    size_type value_start = back_idx * value_size;
    return {tick[back_idx], {value.data() + value_start, value_size}};
  }

  // Utilities
  size_type size() const noexcept { return count; }

  bool empty() const noexcept { return count == 0; }

  void clear() noexcept {
    head = 0;
    count = 0;
  }

  void reserve(size_type new_capacity) {
    if (new_capacity > capacity) {
      resize(next_pow2(new_capacity));
    }
  }

  // Additional standard container methods
  size_type max_size() const noexcept {
    return std::min(std::numeric_limits<size_type>::max() / sizeof(T),
                    std::numeric_limits<size_type>::max() / sizeof(U) / value_size);
  }

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
    using history_ptr = std::conditional_t<IsConst, history_ringbuf const *, history_ringbuf *>;
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
      if (n < 0) {
        index -= static_cast<size_type>(-n);
      } else {
        index += static_cast<size_type>(n);
      }
      return *this;
    }

    iterator_t &operator-=(difference_type n) {
      if (n < 0) {
        index += static_cast<size_type>(-n);
      } else {
        index -= static_cast<size_type>(n);
      }
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
  iterator end() { return iterator(this, count); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, count); }
  const_iterator cbegin() const { return const_iterator(this, 0); }
  const_iterator cend() const { return const_iterator(this, count); }

  reverse_iterator rbegin() { return reverse_iterator(end()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
  const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
  const_reverse_iterator crbegin() const { return const_reverse_iterator(cend()); }
  const_reverse_iterator crend() const { return const_reverse_iterator(cbegin()); }

private:
  void resize(size_type new_capacity) {
    assert((new_capacity & (new_capacity - 1)) == 0 && "new_capacity must be power of 2");

    std::vector<T> new_tick(new_capacity);
    std::vector<U> new_value(new_capacity * value_size);

    // Copy existing data in order using at most 2 copies
    if (count > 0) {
      size_type tail_idx = (head + count - 1) & (capacity - 1);

      if (head <= tail_idx) {
        // Data is contiguous: [head...tail]
        // Copy ticks
        std::copy(tick.begin() + static_cast<difference_type>(head),
                  tick.begin() + static_cast<difference_type>(tail_idx + 1), new_tick.begin());

        // Copy values
        size_type head_value_start = head * value_size;
        size_type value_count = count * value_size;
        std::copy(value.begin() + static_cast<difference_type>(head_value_start),
                  value.begin() + static_cast<difference_type>(head_value_start + value_count), new_value.begin());
      } else {
        // Data wraps around: [head...end] + [0...tail]
        size_type first_part_count = capacity - head;
        size_type second_part_count = count - first_part_count;

        // Copy first part of ticks [head...end]
        std::copy(tick.begin() + static_cast<difference_type>(head), tick.end(), new_tick.begin());

        // Copy second part of ticks [0...tail]
        std::copy(tick.begin(), tick.begin() + static_cast<difference_type>(second_part_count),
                  new_tick.begin() + static_cast<difference_type>(first_part_count));

        // Copy first part of values [head...end]
        size_type head_value_start = head * value_size;
        size_type first_part_value_count = first_part_count * value_size;
        std::copy(value.begin() + static_cast<difference_type>(head_value_start),
                  value.begin() + static_cast<difference_type>(head_value_start + first_part_value_count),
                  new_value.begin());

        // Copy second part of values [0...tail]
        size_type second_part_value_count = second_part_count * value_size;
        std::copy(value.begin(), value.begin() + static_cast<difference_type>(second_part_value_count),
                  new_value.begin() + static_cast<difference_type>(first_part_value_count));
      }
    }

    tick = std::move(new_tick);
    value = std::move(new_value);
    capacity = new_capacity;
    head = 0; // Reset head since we linearized the data
  }
};

static_assert(history_container<history_ringbuf<int, double>, int, double>);
} // namespace opflow
