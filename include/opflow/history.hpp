#pragma once

#include <bit>
#include <cassert>
#include <limits>
#include <span>
#include <vector>

namespace opflow {

template <typename T, typename U>
struct history {
private:
  std::vector<T> tick;  ///< Ticks for each step, circular buffer
  std::vector<U> value; ///< Values for each step, circular buffer
  size_t value_size;    ///< Size of each value vector
  size_t capacity;      ///< Current capacity of the circular buffer (always power of 2)
  size_t head;          ///< Index of the first valid element
  size_t count;         ///< Number of valid elements

  // Helper function to find next power of 2
  static constexpr size_t next_pow2(size_t n) noexcept { return n == 0 ? 1 : std::bit_ceil(n); }

public:
  struct step_view {
    T tick;
    std::span<U const> data;
  };

  explicit history(size_t val_size, size_t initial_capacity = 16) : value_size(val_size), head(0), count(0) {
    if (val_size == 0) {
      // Allow zero value_size but warn that it may not be useful
    }
    capacity = next_pow2(initial_capacity);
    tick.resize(capacity);

    // Check for potential overflow in value allocation
    if (value_size > 0 && capacity > std::numeric_limits<size_t>::max() / value_size) {
      throw std::bad_alloc();
    }
    value.resize(capacity * value_size);
  }

  // Push data to back, if buffer is full, we should ALLOCATE more space
  // @pre data.size() == value_size
  void push(T t, std::span<U const> data) {
    assert(data.size() == value_size);

    if (count == capacity) {
      // Check for potential overflow before doubling capacity
      if (capacity > std::numeric_limits<size_t>::max() / 2) {
        throw std::bad_alloc();
      }
      // Need to grow the buffer (double capacity, which maintains power of 2)
      resize(capacity * 2);
    }

    size_t tail_idx = (head + count) & (capacity - 1); // Use mask for fast modulo
    tick[tail_idx] = t;

    // Copy data to the value buffer
    size_t value_start = tail_idx * value_size;
    std::copy(data.begin(), data.end(), value.begin() + static_cast<std::ptrdiff_t>(value_start));

    ++count;
  }

  // Pop front, free up space that can be reused
  void pop() {
    if (count == 0)
      return;

    head = (head + 1) & (capacity - 1);
    --count;
  }

  // Return data at slot idx (0 = front, size()-1 = back)
  step_view operator[](size_t idx) const {
    assert(idx < count);

    size_t actual_idx = (head + idx) & (capacity - 1);
    size_t value_start = actual_idx * value_size;
    return {tick[actual_idx], std::span<U const>(value.data() + value_start, value_size)};
  }

  step_view front() const {
    assert(count > 0);
    return {tick[head], std::span<U const>(value.data() + head * value_size, value_size)};
  }

  step_view back() const noexcept {
    assert(count > 0);
    size_t back_idx = (head + count - 1) & (capacity - 1);
    size_t value_start = back_idx * value_size;
    return {tick[back_idx], std::span<U const>(value.data() + value_start, value_size)};
  }

  // Utilities
  size_t size() const { return count; }

  bool empty() const { return count == 0; }

  void clear() {
    head = 0;
    count = 0;
  }

  void reserve(size_t new_capacity) {
    if (new_capacity > capacity) {
      resize(next_pow2(new_capacity));
    }
  }

  // Iterator support for range-based loops
  struct iterator {
    history const *hist;
    size_t index;

    iterator(const history *h, size_t idx) : hist(h), index(idx) {}

    step_view operator*() const { return hist->operator[](index); }

    iterator &operator++() {
      ++index;
      return *this;
    }
    iterator operator++(int) {
      iterator tmp = *this;
      ++index;
      return tmp;
    }
    bool operator==(const iterator &other) const { return index == other.index; }
    bool operator!=(const iterator &other) const { return index != other.index; }
  };

  iterator begin() const { return iterator(this, 0); }
  iterator end() const { return iterator(this, count); }

private:
  void resize(size_t new_capacity) {
    assert((new_capacity & (new_capacity - 1)) == 0 && "new_capacity must be power of 2");

    std::vector<T> new_tick(new_capacity);
    std::vector<U> new_value(new_capacity * value_size);

    // Copy existing data in order using at most 2 copies
    if (count > 0) {
      size_t tail_idx = (head + count - 1) & (capacity - 1);

      if (head <= tail_idx) {
        // Data is contiguous: [head...tail]
        // Copy ticks
        std::copy(tick.begin() + static_cast<std::ptrdiff_t>(head),
                  tick.begin() + static_cast<std::ptrdiff_t>(tail_idx + 1), new_tick.begin());

        // Copy values
        size_t head_value_start = head * value_size;
        size_t value_count = count * value_size;
        std::copy(value.begin() + static_cast<std::ptrdiff_t>(head_value_start),
                  value.begin() + static_cast<std::ptrdiff_t>(head_value_start + value_count), new_value.begin());
      } else {
        // Data wraps around: [head...end] + [0...tail]
        size_t first_part_count = capacity - head;
        size_t second_part_count = count - first_part_count;

        // Copy first part of ticks [head...end]
        std::copy(tick.begin() + static_cast<std::ptrdiff_t>(head), tick.end(), new_tick.begin());

        // Copy second part of ticks [0...tail]
        std::copy(tick.begin(), tick.begin() + static_cast<std::ptrdiff_t>(second_part_count),
                  new_tick.begin() + static_cast<std::ptrdiff_t>(first_part_count));

        // Copy first part of values [head...end]
        size_t head_value_start = head * value_size;
        size_t first_part_value_count = first_part_count * value_size;
        std::copy(value.begin() + static_cast<std::ptrdiff_t>(head_value_start),
                  value.begin() + static_cast<std::ptrdiff_t>(head_value_start + first_part_value_count),
                  new_value.begin());

        // Copy second part of values [0...tail]
        size_t second_part_value_count = second_part_count * value_size;
        std::copy(value.begin(), value.begin() + static_cast<std::ptrdiff_t>(second_part_value_count),
                  new_value.begin() + static_cast<std::ptrdiff_t>(first_part_value_count));
      }
    }

    tick = std::move(new_tick);
    value = std::move(new_value);
    capacity = new_capacity;
    head = 0; // Reset head since we linearized the data
  }
};
} // namespace opflow
