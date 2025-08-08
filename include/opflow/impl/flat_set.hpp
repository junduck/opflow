#pragma once

#include <vector>

#include "iterator.hpp"

namespace opflow::impl {
/**
 * @brief A flat set implementation using custom container as underlying storage.
 *
 * @note This container behaves differently from std::flat_set. It preserves insertion order, and returns
 * the index of the element on emplace/insert.
 *
 * @tparam T The type of elements stored in the set.
 * @tparam Container The container type used to store the elements.
 */
template <typename T, typename Container = std::vector<T>>
class flat_set {
public:
  using container_type = Container;
  using size_type = typename container_type::size_type;
  using difference_type = typename container_type::difference_type;
  using value_type = T;
  using reference = T const &; ///< We do not support mutable references
  using const_reference = T const &;
  using iterator = iterator_t<flat_set, false>;
  using const_iterator = iterator_t<flat_set, true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  const_reference operator[](size_t idx) const {
    assert(idx < data.size() && "Index out of bounds");
    return data[idx];
  }

  // iterator interface

  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator cbegin() const { return const_iterator(this, 0); }

  const_iterator end() const { return const_iterator(this, data.size()); }
  const_iterator cend() const { return const_iterator(this, data.size()); }

  const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
  const_reverse_iterator crbegin() const { return const_reverse_iterator(cend()); }

  const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
  const_reverse_iterator crend() const { return const_reverse_iterator(cbegin()); }

  // capacity

  bool empty() const { return data.empty(); }
  size_type size() const { return data.size(); }
  size_type max_size() const { return data.max_size(); }

  // modifiers

  /**
   * @brief Inserts a new element into the set.
   *
   * @warning This method behaves differently from std::flat_set. It returns the index of the inserted element.
   *
   * @tparam Args
   * @param args
   * @return index of the inserted/existing element
   */
  template <typename... Args>
  size_type emplace(Args &&...args) {
    data.emplace_back(std::forward<Args>(args)...);
    auto it = std::ranges::find(data, data.back());
    auto dist = static_cast<size_type>(std::ranges::distance(data.begin(), it));
    if (dist != data.size() - 1) {
      // Remove last element if duplicated
      data.pop_back();
    }
    return dist;
  }

  size_type insert(T const &val) { return emplace(val); }
  size_type insert(T &&val) { return emplace(std::move(val)); }

  const_iterator erase(T const &val) {
    if (auto it = std::ranges::find(data, val); it != data.end()) {
      auto dist = std::ranges::distance(data.begin(), it);
      data.erase(it);
      return const_iterator(this, static_cast<size_type>(dist));
    }
    return end();
  }

  const_iterator erase(const_iterator pos) {
    if (pos < end()) {
      auto dist = std::ranges::distance(begin(), pos);
      data.erase(data.begin() + dist);
      return const_iterator(this, static_cast<size_type>(dist));
    }
    return end();
  }

  template <typename Pred>
  size_type erase_if(Pred pred) {
    size_type count = 0;
    auto it = std::remove_if(data.begin(), data.end(), [&](T const &val) {
      if (pred(val)) {
        ++count;
        return true; // Remove this element
      }
      return false; // Keep this element
    });
    data.erase(it, data.end());
    return count;
  }

  void clear() { data.clear(); }

  container_type &&extract() { return std::move(data); }

  // lookup

  const_iterator find(T const &val) const {
    auto it = std::ranges::find(data, val);
    auto dist = std::ranges::distance(data.begin(), it);
    return const_iterator(this, static_cast<size_type>(dist));
  }

  bool contains(T const &val) const { return std::ranges::find(data, val) != std::ranges::end(data); }

  size_type count(T const &val) const { return std::ranges::count(data, val); }

  template <typename Pred>
  size_type count_if(Pred pred) const {
    return std::ranges::count_if(data, pred);
  }

  friend auto operator<=>(flat_set const &lhs, flat_set const &rhs) = default;

  friend void swap(flat_set &lhs, flat_set &rhs) noexcept(noexcept(lhs.data.swap(rhs.data))) {
    lhs.data.swap(rhs.data);
  }

private:
  container_type data; ///< Underlying storage for the set
};

} // namespace opflow::impl
