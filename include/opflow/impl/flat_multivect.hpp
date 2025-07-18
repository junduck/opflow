#pragma once

#include <cassert>
#include <ranges>
#include <span>
#include <vector>

#include "iterator.hpp"

namespace opflow::impl {

template <typename T, typename Container = std::vector<T>>
class flat_multivect {
  using flat_container = Container;
  using idx_container = std::vector<size_t>;

  flat_container flat_data;     ///< Flattened storage for all vectors
  idx_container offset, length; ///< Offsets and lengths for each vector

public:
  using value_type = std::span<T>;
  using const_value_type = std::span<const T>;
  using iterator = iterator_t<flat_multivect, false>;
  using const_iterator = iterator_t<flat_multivect, true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  flat_multivect() = default;

  template <std::ranges::forward_range R>
  size_t push_back(R &&range) {
    size_t const idx = offset.size();
    size_t const off = flat_data.size();
    size_t const len = std::ranges::size(range);

    flat_data.insert(flat_data.end(), std::ranges::begin(range), std::ranges::end(range));
    offset.push_back(off);
    length.push_back(len);

    return idx;
  }

  // Push a vector to the front (slow, linear time)
  template <std::ranges::forward_range R>
  size_t push_front(R &&range) {
    flat_data.insert(flat_data.begin(), std::ranges::begin(range), std::ranges::end(range));
    offset.insert(offset.begin(), 0);
    length.insert(length.begin(), std::ranges::size(range));
    // shift offsets
    size_t const n = std::ranges::size(range);
    for (size_t i = 1; i < offset.size(); ++i) {
      offset[i] += n;
    }
    return 0;
  }

  void pop_back() {
    assert(!offset.empty() && "pop_back on empty flat_multivect");
    size_t last_off = offset.back();
    flat_data.resize(last_off); // Remove elements of last vector
    offset.pop_back();
    length.pop_back();
  }

  // Pop the front element (slow, linear time)
  void pop_front() {
    assert(!offset.empty() && "pop_front on empty flat_multivect");
    size_t first_len = length.front();
    flat_data.erase(flat_data.begin(),
                    flat_data.begin() + static_cast<typename flat_container::difference_type>(first_len));
    offset.erase(offset.begin());
    length.erase(length.begin());
    // Fix up offsets after pop
    for (size_t i = 0; i < offset.size(); ++i) {
      offset[i] -= first_len;
    }
  }

  // Erase a vector at idx (slow, linear time)
  void erase(size_t idx) {
    assert(idx < offset.size() && "Index out of bounds");
    size_t off = offset[idx];
    size_t len = length[idx];
    // Remove elements from flat_data
    flat_data.erase(flat_data.begin() + static_cast<typename flat_container::difference_type>(off),
                    flat_data.begin() + static_cast<typename flat_container::difference_type>(off + len));
    // Remove offset/length
    offset.erase(offset.begin() + static_cast<typename idx_container::difference_type>(idx));
    length.erase(length.begin() + static_cast<typename idx_container::difference_type>(idx));
    // Fix up offsets after idx
    for (size_t i = idx; i < offset.size(); ++i) {
      offset[i] -= len;
    }
  }

  void shrink_to_fit() {
    flat_data.shrink_to_fit();
    offset.shrink_to_fit();
    length.shrink_to_fit();
  }

  value_type operator[](size_t idx) noexcept {
    assert(idx < offset.size() && "Index out of bounds");
    return value_type(flat_data).subspan(offset[idx], length[idx]);
  }
  const_value_type operator[](size_t idx) const noexcept {
    assert(idx < offset.size() && "Index out of bounds");
    return const_value_type(flat_data).subspan(offset[idx], length[idx]);
  }

  value_type flat() noexcept { return value_type(flat_data); }
  const_value_type flat() const noexcept { return const_value_type(flat_data); }

  T *data() noexcept
    requires requires { std::declval<Container>().data(); }
  {
    return flat_data.data();
  }
  T const *data() const noexcept
    requires requires { std::declval<Container const>().data(); }
  {
    return flat_data.data();
  }

  // iterator interface
  iterator begin() { return iterator(this, 0); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator cbegin() const { return const_iterator(this, 0); }

  iterator end() { return iterator(this, offset.size()); }
  const_iterator end() const { return const_iterator(this, offset.size()); }
  const_iterator cend() const { return const_iterator(this, offset.size()); }

  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
  const_reverse_iterator crbegin() const { return const_reverse_iterator(cend()); }

  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
  const_reverse_iterator crend() const { return const_reverse_iterator(cbegin()); }

  size_t total_size() const noexcept { return flat_data.size(); }

  size_t size() const noexcept { return offset.size(); }
  size_t size(size_t idx) const noexcept {
    assert(idx < offset.size() && "Index out of bounds");
    return length[idx];
  }

  bool empty() const noexcept { return offset.empty(); }
  bool empty(size_t idx) const noexcept {
    assert(idx < offset.size() && "Index out of bounds");
    return length[idx] == 0;
  }

  void clear() noexcept {
    flat_data.clear();
    offset.clear();
    length.clear();
  }

  void reserve(size_t n_vect, size_t n_elem) {
    flat_data.reserve(n_elem);
    offset.reserve(n_vect);
    length.reserve(n_vect);
  }
};
} // namespace opflow::impl
