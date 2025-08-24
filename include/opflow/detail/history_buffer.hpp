#pragma once

#include <bit>
#include <ranges>
#include <span>
#include <vector>

#include "iterator.hpp"
#include "opflow/common.hpp"

namespace opflow::detail {
template <typename T, typename Alloc = std::allocator<T>>
class history_buffer {
public:
  using value_type = std::pair<T, std::span<T>>;
  using const_value_type = std::pair<T, std::span<T const>>;

  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

private:
  std::vector<T, Alloc> buffer; ///< Circular buffer for storing records: record = [time, data...]
  size_type record_size;        ///< Size of each record
  size_type capacity;           ///< Current capacity of the buffer (in records)
  size_type head;               ///< Index of the head element
  size_type count;              ///< Number of valid elements

  static constexpr size_t next_pow2(size_type n) noexcept { return n == 0 ? 1 : std::bit_ceil(n); }

public:
  history_buffer() = default;

  history_buffer(size_type val_size, size_type init_cap, Alloc const &alloc = Alloc())
      : buffer(alloc), record_size(val_size + 1), capacity(next_pow2(init_cap ? init_cap : 1)), head(0), count(0) {
    // Check for potential overflow in value allocation
    if (record_size == 0 || capacity > std::numeric_limits<size_type>::max() / record_size) {
      throw std::bad_alloc();
    }
    buffer.resize(capacity * record_size);
  }

  // Push back a new record
  template <sized_range_of<T> R>
  value_type push(T t, R &&data) {
    if (count == capacity) {
      // Check for potential overflow before doubling capacity
      if (capacity > std::numeric_limits<size_type>::max() / 2) {
        throw std::bad_alloc();
      }
      resize(capacity * 2);
    }

    size_type record_start = record_offset(count);
    std::span<T> record{buffer.data() + record_start, record_size};
    record[0] = t;
    std::copy(std::ranges::begin(data), std::ranges::end(data), record.subspan(1).begin());

    ++count;

    return {t, record.subspan(1)};
  }

  /// Push back a new record with timestamp only, no data
  [[nodiscard]] value_type push(T t) {
    if (count == capacity) {
      // Check for potential overflow before doubling capacity
      if (capacity > std::numeric_limits<size_type>::max() / 2) {
        throw std::bad_alloc();
      }
      resize(capacity * 2);
    }

    size_type record_start = record_offset(count);
    std::span<T> record{buffer.data() + record_start, record_size};
    record[0] = t;

    ++count;

    return {t, record.subspan(1)};
  }

  void pop() noexcept {
    if (count == 0)
      return;
    head = (head + 1) & (capacity - 1);
    --count;
  }

  value_type operator[](size_type idx) { return get_record(idx); }
  const_value_type operator[](size_type idx) const { return get_record(idx); }

  value_type from_back(size_type back_idx) { return operator[](count - 1 - back_idx); }
  const_value_type from_back(size_type back_idx) const { return operator[](count - 1 - back_idx); }

  value_type front() { return get_record(0); }
  const_value_type front() const { return get_record(0); }

  value_type back() { return get_record(count - 1); }
  const_value_type back() const { return get_record(count - 1); }

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

  size_type max_size() const noexcept { return std::numeric_limits<size_type>::max() / sizeof(T) / record_size; }

  using iterator = detail::iterator_t<history_buffer, false>;
  using const_iterator = detail::iterator_t<history_buffer, true>;
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
    std::vector<T, Alloc> new_buffer(new_capacity * record_size, buffer.get_allocator());

    // Copy existing data in order using at most 2 copies
    if (count > 0) {
      size_type tail_idx = (head + count - 1) & (capacity - 1);

      if (head <= tail_idx) {
        // Data is contiguous: [head...tail]
        size_type head_buffer_start = head * record_size;
        size_type buffer_count = count * record_size;
        std::copy(buffer.begin() + static_cast<difference_type>(head_buffer_start),
                  buffer.begin() + static_cast<difference_type>(head_buffer_start + buffer_count), new_buffer.begin());
      } else {
        // Data wraps around: [head...end] + [0...tail]
        size_type first_part_count = capacity - head;
        size_type second_part_count = count - first_part_count;
        // Copy first part of buffer [head...end]
        size_type head_buffer_start = head * record_size;
        size_type first_part_buffer_count = first_part_count * record_size;
        std::copy(buffer.begin() + static_cast<difference_type>(head_buffer_start),
                  buffer.begin() + static_cast<difference_type>(head_buffer_start + first_part_buffer_count),
                  new_buffer.begin());

        // Copy second part of buffer [0...tail]
        size_type second_part_buffer_count = second_part_count * record_size;
        std::copy(buffer.begin(), buffer.begin() + static_cast<difference_type>(second_part_buffer_count),
                  new_buffer.begin() + static_cast<difference_type>(first_part_buffer_count));
      }
    }
    buffer = std::move(new_buffer);
    capacity = new_capacity;
    head = 0; // Reset head since we linearised the data
  }

  size_type record_offset(size_type idx) const noexcept {
    size_type actual_idx = (head + idx) & (capacity - 1);
    return actual_idx * record_size;
  }

  value_type get_record(size_type idx) noexcept {
    auto offset = record_offset(idx);
    std::span<T> record{buffer.data() + offset, record_size};
    return {record[0], record.subspan(1)};
  }

  const_value_type get_record(size_type idx) const noexcept {
    auto offset = record_offset(idx);
    std::span<T const> record{buffer.data() + offset, record_size};
    return {record[0], record.subspan(1)};
  }
};
} // namespace opflow::detail
