#pragma once

#include <cassert>
#include <ranges>
#include <span>
#include <vector>

#include "iterator.hpp"
#include "utils.hpp"

namespace opflow::detail {
template <typename T, std::unsigned_integral Index = uint32_t, typename Alloc = std::allocator<T>>
class flat_multivect {
  using offset_type = detail::offset_type<Index>;
  using data_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
  using offset_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<offset_type>;

  using flat_container = std::vector<T, data_alloc>;
  using idx_container = std::vector<offset_type, offset_alloc>;

  idx_container index;      ///< Offsets and lengths for each vector
  flat_container flat_data; ///< Flattened storage for all vectors

public:
  using value_type = std::span<T>;
  using const_value_type = std::span<const T>;
  using iterator = iterator_t<flat_multivect, false>;
  using const_iterator = iterator_t<flat_multivect, true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  flat_multivect() = default;
  flat_multivect(Alloc const &alloc) : index(alloc), flat_data(alloc) {}

  template <typename OtherIndex, typename OtherAlloc>
  flat_multivect(flat_multivect<T, OtherIndex, OtherAlloc> const &other, Alloc const &alloc = Alloc{})
      : index(alloc), flat_data(alloc) {
    // Reserve exact capacity to avoid any growth
    flat_data.reserve(other.total_size());
    auto other_flat = other.flat();
    flat_data.assign(other_flat.begin(), other_flat.end());

    // Reconstruct index information from public accessors (size and size(i)).
    index.reserve(other.size());
    size_t offset = 0;
    for (size_t i = 0; i < other.size(); ++i) {
      size_t len = other.size(i);
      index.emplace_back(offset, len);
      offset += len;
    }
  }

  template <std::ranges::forward_range R>
  size_t push_back(R &&range) {
    size_t const idx = index.size();
    size_t const off = flat_data.size();
    size_t const len = std::ranges::size(range);

    flat_data.insert(flat_data.end(), std::ranges::begin(range), std::ranges::end(range));
    index.emplace_back(off, len);

    return idx;
  }

  // Push a vector to the front (slow, linear time)
  template <std::ranges::forward_range R>
  size_t push_front(R &&range) {
    flat_data.insert(flat_data.begin(), std::ranges::begin(range), std::ranges::end(range));
    index.emplace(index.begin(), 0, std::ranges::size(range));
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
    size_t first_len = index.front().size;
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
    size_t len = index[idx].size;
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
    return value_type(flat_data).subspan(index[idx].offset, index[idx].size);
  }
  const_value_type operator[](size_t idx) const noexcept {
    assert(idx < index.size() && "Index out of bounds");
    return const_value_type(flat_data).subspan(index[idx].offset, index[idx].size);
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
    return index[idx].size;
  }

  bool empty() const noexcept { return index.empty(); }
  bool empty(size_t idx) const noexcept {
    assert(idx < index.size() && "Index out of bounds");
    return index[idx].size == 0;
  }

  void clear() noexcept {
    flat_data.clear();
    index.clear();
  }

  void reserve(size_t n_vect, size_t n_elem) {
    index.reserve(n_vect);
    flat_data.reserve(n_elem);
  }

  static size_t heap_alloc_size(size_t n_vect, size_t n_elem) noexcept {
    size_t data_size = aligned_size(sizeof(T) * n_elem, alignof(T));
    size_t idx_size = aligned_size(sizeof(offset_type) * n_vect, alignof(offset_type));
    return data_size + idx_size;
  }
};
} // namespace opflow::detail
