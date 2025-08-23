#pragma once

#include <cassert>
#include <ranges>
#include <span>
#include <vector>

#include "iterator.hpp"

namespace opflow::detail {
template <typename T, typename Alloc = std::allocator<T>>
class flat_multivect {
  struct idx_t {
    size_t offset; ///< Offset in the flat data
    size_t length; ///< Length of the vector at this position
  };
  using idx_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<idx_t>;

  using flat_container = std::vector<T, Alloc>;
  using idx_container = std::vector<idx_t, idx_alloc>;

  flat_container flat_data; ///< Flattened storage for all vectors
  idx_container index;      ///< Offsets and lengths for each vector

public:
  using value_type = std::span<T>;
  using const_value_type = std::span<const T>;
  using iterator = iterator_t<flat_multivect, false>;
  using const_iterator = iterator_t<flat_multivect, true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  flat_multivect() = default;
  flat_multivect(Alloc const &alloc) : flat_data(alloc), index(alloc) {}

  template <typename OtherAlloc>
  flat_multivect(flat_multivect<T, OtherAlloc> const &other, Alloc const &alloc = Alloc{})
      : flat_data(alloc), index(alloc) {
    // Reserve exact capacity to avoid any growth
    flat_data.reserve(other.total_size());
    auto other_flat = other.flat();
    flat_data.assign(other_flat.begin(), other_flat.end());

    // Reconstruct index information from public accessors (size and size(i)).
    index.reserve(other.size());
    size_t offset = 0;
    for (size_t i = 0; i < other.size(); ++i) {
      size_t len = other.size(i);
      index.push_back({offset, len});
      offset += len;
    }
  }

  template <std::ranges::forward_range R>
  size_t push_back(R &&range) {
    size_t const idx = index.size();
    size_t const off = flat_data.size();
    size_t const len = std::ranges::size(range);

    flat_data.insert(flat_data.end(), std::ranges::begin(range), std::ranges::end(range));
    index.push_back({off, len});

    return idx;
  }

  // Push a vector to the front (slow, linear time)
  template <std::ranges::forward_range R>
  size_t push_front(R &&range) {
    flat_data.insert(flat_data.begin(), std::ranges::begin(range), std::ranges::end(range));
    index.insert(index.begin(), {0, std::ranges::size(range)});
    // shift offsets
    size_t const n = std::ranges::size(range);
    for (size_t i = 1; i < index.size(); ++i) {
      index[i].offset += n;
    }
    return 0;
  }

  void pop_back() {
    assert(!index.empty() && "pop_back on empty flat_multivect");
    size_t last_off = index.back().offset;
    flat_data.resize(last_off); // Remove elements of last vector
    index.pop_back();
  }

  // Pop the front element (slow, linear time)
  void pop_front() {
    assert(!index.empty() && "pop_front on empty flat_multivect");
    size_t first_len = index.front().length;
    flat_data.erase(flat_data.begin(),
                    flat_data.begin() + static_cast<typename flat_container::difference_type>(first_len));
    index.erase(index.begin());
    // Fix up offsets after pop
    for (size_t i = 0; i < index.size(); ++i) {
      index[i].offset -= first_len;
    }
  }

  // Erase a vector at idx (slow, linear time)
  void erase(size_t idx) {
    assert(idx < index.size() && "Index out of bounds");
    size_t off = index[idx].offset;
    size_t len = index[idx].length;
    // Remove elements from flat_data
    flat_data.erase(flat_data.begin() + static_cast<typename flat_container::difference_type>(off),
                    flat_data.begin() + static_cast<typename flat_container::difference_type>(off + len));
    // Remove offset/length
    index.erase(index.begin() + static_cast<typename idx_container::difference_type>(idx));
    // Fix up offsets after idx
    for (size_t i = idx; i < index.size(); ++i) {
      index[i].offset -= len;
    }
  }

  void shrink_to_fit() {
    flat_data.shrink_to_fit();
    index.shrink_to_fit();
  }

  value_type operator[](size_t idx) noexcept {
    assert(idx < index.size() && "Index out of bounds");
    return value_type(flat_data).subspan(index[idx].offset, index[idx].length);
  }
  const_value_type operator[](size_t idx) const noexcept {
    assert(idx < index.size() && "Index out of bounds");
    return const_value_type(flat_data).subspan(index[idx].offset, index[idx].length);
  }

  value_type flat() noexcept { return value_type(flat_data); }
  const_value_type flat() const noexcept { return const_value_type(flat_data); }

  T *data() noexcept { return flat_data.data(); }
  T const *data() const noexcept { return flat_data.data(); }

  // iterator interface
  iterator begin() { return iterator(this, 0); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator cbegin() const { return const_iterator(this, 0); }

  iterator end() { return iterator(this, index.size()); }
  const_iterator end() const { return const_iterator(this, index.size()); }
  const_iterator cend() const { return const_iterator(this, index.size()); }

  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
  const_reverse_iterator crbegin() const { return const_reverse_iterator(cend()); }

  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
  const_reverse_iterator crend() const { return const_reverse_iterator(cbegin()); }

  size_t total_size() const noexcept { return flat_data.size(); }

  size_t size() const noexcept { return index.size(); }
  size_t size(size_t idx) const noexcept {
    assert(idx < index.size() && "Index out of bounds");
    return index[idx].length;
  }

  bool empty() const noexcept { return index.empty(); }
  bool empty(size_t idx) const noexcept {
    assert(idx < index.size() && "Index out of bounds");
    return index[idx].length == 0;
  }

  void clear() noexcept {
    flat_data.clear();
    index.clear();
  }

  void reserve(size_t n_vect, size_t n_elem) {
    flat_data.reserve(n_elem);
    index.reserve(n_vect);
  }
};
} // namespace opflow::detail
