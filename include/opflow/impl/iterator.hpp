#pragma once

#include <cassert>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace opflow::impl {
template <typename Self, bool IsConst>
class iterator_t {
  using self_type = Self;
  using self_ptr = std::conditional_t<IsConst, self_type const *, self_type *>;

public:
  using iterator_category = std::random_access_iterator_tag;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = decltype(std::declval<self_ptr>()->operator[](0));
  using value_type = std::remove_cv_t<std::remove_reference_t<reference>>;
  using pointer = std::add_pointer_t<reference>;

private:
  self_ptr self;
  size_type index;

public:
  iterator_t() : self(nullptr), index(0) {}
  iterator_t(self_ptr p, size_type idx) : self(p), index(idx) {}

  template <bool OtherConst>
  iterator_t(iterator_t<Self, OtherConst> const &other)
    requires(IsConst && !OtherConst)
      : self(other.self), index(other.index) {}

  reference operator*() const {
    assert(self != nullptr && "Iterator dereferencing null pointer");
    return self->operator[](index);
  }
  pointer operator->() const
    requires std::is_reference_v<reference>
  {
    assert(self != nullptr && "Iterator dereferencing null pointer");
    return std::addressof(**this);
  }

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
    assert(index > 0 && "Iterator decrement underflow");
    --index;
    return *this;
  }
  iterator_t operator--(int) {
    assert(index > 0 && "Iterator decrement underflow");
    iterator_t tmp = *this;
    --index;
    return tmp;
  }
  iterator_t &operator+=(difference_type n) {
    auto new_index = static_cast<difference_type>(index) + n;
    assert(new_index >= 0 && "Iterator index underflow");
    index = static_cast<size_type>(new_index);
    return *this;
  }
  iterator_t &operator-=(difference_type n) {
    auto new_index = static_cast<difference_type>(index) - n;
    assert(new_index >= 0 && "Iterator index underflow");
    index = static_cast<size_type>(new_index);
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
    assert(self == other.self && "Iterators from different containers");
    return static_cast<difference_type>(index) - static_cast<difference_type>(other.index);
  }
  reference operator[](difference_type n) const {
    assert(self != nullptr && "Iterator dereferencing null pointer");
    auto new_index = static_cast<difference_type>(index) + n;
    assert(new_index >= 0 && "Iterator index underflow");
    return self->operator[](static_cast<size_type>(new_index));
  }
  auto operator<=>(iterator_t const &other) const {
    assert(self == other.self && "Comparing iterators from different containers");
    return index <=> other.index;
  }

  bool operator==(iterator_t const &other) const { return self == other.self && index == other.index; }

  // Free function operator+ for (n + iterator)
  friend iterator_t operator+(difference_type n, iterator_t const &it) { return it + n; }

  template <typename, bool>
  friend class iterator_t;
};

} // namespace opflow::impl
