#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <limits>
#include <vector>

#include "iterator.hpp"

namespace opflow::impl {
template <typename T>
class ringbuf_vect {
public:
  using value_type = T;
  using const_value_type = const T;

  using reference = T &;
  using const_reference = const T &;

  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  ringbuf_vect(const ringbuf_vect &) = default;
  ringbuf_vect &operator=(const ringbuf_vect &) = default;
  ringbuf_vect(ringbuf_vect &&) noexcept = default;
  ringbuf_vect &operator=(ringbuf_vect &&) noexcept = default;

  explicit ringbuf_vect(size_type initial_capacity = 8) : head(0), count(0), cap(initial_capacity) {
    cap = cap == 0 ? 8 : next_pow2(cap);
    if (cap > std::numeric_limits<size_type>::max() / sizeof(T)) {
      throw std::bad_alloc();
    }
    data.resize(cap);
  }

  // push back
  template <typename U>
  void push(U &&value) {
    if (count == cap) {
      if (cap > std::numeric_limits<size_type>::max() / 2) {
        throw std::bad_alloc();
      }
      size_type new_cap = cap * 2;
      resize(new_cap);
    }
    auto tail = (head + count) & (cap - 1);
    data[tail] = std::forward<U>(value);
    ++count;
  }

  // pop front
  void pop() {
    if (count == 0)
      return;
    head = (head + 1) & (cap - 1);
    --count;
  }

  reference front() {
    assert(count > 0 && "Index out of bounds");
    return data[head];
  }

  const_reference front() const {
    assert(count > 0 && "Index out of bounds");
    return data[head];
  }

  reference back() {
    assert(count > 0 && "Index out of bounds");
    auto tail = (head + count - 1) & (cap - 1);
    return data[tail];
  }

  const_reference back() const {
    assert(count > 0 && "Index out of bounds");
    auto tail = (head + count - 1) & (cap - 1);
    return data[tail];
  }

  reference operator[](size_type idx) {
    assert(idx < count && "Index out of bounds");
    return data[(head + idx) & (cap - 1)];
  }

  const_reference operator[](size_type idx) const {
    assert(idx < count && "Index out of bounds");
    return data[(head + idx) & (cap - 1)];
  }

  size_type size() const noexcept { return count; }
  bool empty() const noexcept { return count == 0; }
  void clear() noexcept {
    head = 0;
    count = 0;
  }
  void reserve(size_type new_capacity) {
    if (new_capacity > cap) {
      resize(next_pow2(new_capacity));
    }
  }

  using iterator = impl::iterator_t<ringbuf_vect, false>;
  using const_iterator = impl::iterator_t<ringbuf_vect, true>;
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
  // Helper function to find next power of 2
  static constexpr size_type next_pow2(size_type n) noexcept { return n == 0 ? 1 : std::bit_ceil(n); }

  void resize(size_type new_capacity) {
    assert((new_capacity & (new_capacity - 1)) == 0 && "new_capacity must be power of 2");
    assert(new_capacity >= count && "new_capacity must be at least current size");

    std::vector<T> new_data(new_capacity);
    if (count > 0) {
      size_type tail_idx = (head + count - 1) & (cap - 1);
      if (head <= tail_idx) {
        // Data is contiguous: [head...tail]
        std::copy(data.begin() + static_cast<difference_type>(head),
                  data.begin() + static_cast<difference_type>(tail_idx + 1), new_data.begin());
      } else {
        // Data wraps around: [head...end] + [0...tail]
        size_type first_part_count = cap - head;
        size_type second_part_count = count - first_part_count;

        // Copy first part of data [head...end]
        std::copy(data.begin() + static_cast<difference_type>(head), data.end(), new_data.begin());
        // Copy second part of data [0...tail]
        std::copy(data.begin(), data.begin() + static_cast<difference_type>(second_part_count),
                  new_data.begin() + static_cast<difference_type>(first_part_count));
      }
    }
    data.swap(new_data);
    cap = new_capacity;
    head = 0;
  }

  std::vector<T> data;
  size_type head;
  size_type count;
  size_type cap;
};
} // namespace opflow::impl
