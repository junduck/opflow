#pragma once

#include "../def.hpp"
#include "../op_base.hpp"

namespace opflow::op {
template <typename T, typename Alloc = std::allocator<T>>
class min : public simple_rollop<T> {
public:
  using base = simple_rollop<T>;
  using typename base::data_type;
  using typename base::win_type;

  template <std::integral U>
  explicit min(U period) : base(period), vec() {
    if (period == 0) {
      throw std::invalid_argument("min: period must be positive");
    }
    vec.reserve(static_cast<size_t>(period));
  }

  explicit min(data_type win_time, std::integral auto est_event_per_win) : base(win_time), vec() {
    if (win_time == data_type{}) {
      throw std::invalid_argument("min: window time must be positive");
    }
    vec.reserve(static_cast<size_t>(est_event_per_win));
  }

  void on_data(data_type const *in) noexcept override {
    auto const val = in[0];

    while (vec.size() > head_idx && vec.back() > val) {
      vec.pop_back();
    }

    vec.push_back(val);

    // linearise
    if (vec.capacity() == vec.size()) {
      vec.erase(vec.begin(), vec.begin() + static_cast<ptrdiff_t>(head_idx));
      head_idx = 0;
    }
  }

  void on_evict(data_type const *rm) noexcept override {
    assert(!vec.empty() && "[BUG] Evicting from empty window.");
    if (vec[head_idx] == rm[0])
      ++head_idx;
    assert(!head_idx < vec.size() && "[BUG] Eviction results in empty window.");
  }

  void value(data_type *out) const noexcept override { out[0] = vec[head_idx]; }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(min)

public:
  std::vector<data_type, Alloc> vec;
  size_t head_idx;
};

template <typename T, typename Alloc = std::allocator<T>>
class max : public simple_rollop<T> {
public:
  using base = simple_rollop<T>;
  using typename base::data_type;
  using typename base::win_type;

  template <std::integral U>
  explicit max(U period) : base(period), vec() {
    if (period == 0) {
      throw std::invalid_argument("max: period must be positive");
    }
    vec.reserve(static_cast<size_t>(period));
  }

  explicit max(data_type win_time, std::integral auto est_event_per_win) : base(win_time), vec() {
    if (win_time == data_type{}) {
      throw std::invalid_argument("max: window time must be positive");
    }
    vec.reserve(static_cast<size_t>(est_event_per_win));
  }

  void on_data(data_type const *in) noexcept override {
    auto const val = in[0];

    while (vec.size() > head_idx && vec.back() < val) {
      vec.pop_back();
    }

    vec.push_back(val);

    // linearise
    if (vec.capacity() == vec.size()) {
      vec.erase(vec.begin(), vec.begin() + static_cast<ptrdiff_t>(head_idx));
      head_idx = 0;
    }
  }

  void on_evict(data_type const *rm) noexcept override {
    assert(!vec.empty() && "[BUG] Evicting from empty window.");
    if (vec[head_idx] == rm[0])
      ++head_idx;
    assert(!head_idx < vec.size() && "[BUG] Eviction results in empty window.");
  }

  void value(data_type *out) const noexcept override { out[0] = vec[head_idx]; }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(max)

public:
  std::vector<data_type, Alloc> vec;
  size_t head_idx;
};
} // namespace opflow::op
