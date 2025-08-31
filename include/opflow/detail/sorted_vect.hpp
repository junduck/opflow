#pragma once

#include <algorithm>
#include <vector>

namespace opflow::detail {
template <typename T, size_t BIN_THRES = 100, typename Alloc = std::allocator<T>>
class sorted_vect : public std::vector<T, Alloc> {
  using base = std::vector<T, Alloc>;
  using base::push_back;

public:
  using base::base;

  size_t rank(T const &value) const {
    ptrdiff_t dist = static_cast<ptrdiff_t>(base::size());
    if (base::size() > BIN_THRES) {
      auto it = std::lower_bound(base::begin(), base::end(), value);
      if (it != base::end() && *it == value) {
        dist = std::distance(base::begin(), it);
      }
    } else {
      auto it = std::find(base::begin(), base::end(), value);
      dist = std::distance(base::begin(), it);
    }
    return static_cast<size_t>(dist);
  }

  void push(T const &value) {
    if (base::size() > BIN_THRES) {
      auto it = std::lower_bound(base::begin(), base::end(), value);
      base::insert(it, value);
    } else {
      auto it = std::find_if(base::begin(), base::end(), [&value](T const &elem) { return elem >= value; });
      base::insert(it, value);
    }
  }

  void push(T &&value) {
    if (base::size() > BIN_THRES) {
      auto it = std::lower_bound(base::begin(), base::end(), value);
      base::insert(it, std::move(value));
    } else {
      auto it = std::find_if(base::begin(), base::end(), [&value](T const &elem) { return elem >= value; });
      base::insert(it, std::move(value));
    }
  }

  void erase(T const &value) {
    if (base::size() > BIN_THRES) {
      auto it = std::lower_bound(base::begin(), base::end(), value);
      if (it != base::end() && *it == value) {
        base::erase(it);
      }
    } else {
      auto it = std::find(base::begin(), base::end(), value);
      if (it != base::end()) {
        base::erase(it);
      }
    }
  }

  void erase_rank(size_t rank) {
    if (rank < base::size()) {
      base::erase(base::begin() + static_cast<ptrdiff_t>(rank));
    }
  }
};
} // namespace opflow::detail
