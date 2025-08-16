#pragma once

#include <cassert>

#include "opflow/op_base.hpp"

namespace opflow::op::detail {
template <std::floating_point Data, window_domain WIN>
struct static_win : public op_base<Data> {
  using base = op_base<Data>;
  using typename base::data_type;
  union {
    size_t win_event;
    data_type win_time;
  };

  explicit static_win(size_t win_event) noexcept
    requires(WIN == window_domain::event)
      : win_event(win_event) {}

  explicit static_win(data_type win_time) noexcept
    requires(WIN == window_domain::time)
      : win_time(win_time) {}

  bool is_cumulative() const noexcept override {
    if constexpr (WIN == window_domain::event) {
      return win_event == 0;

    } else {
      return win_time == data_type{};
    }
  }

  size_t window_size(event_domain_tag) const noexcept override {
    if constexpr (WIN == window_domain::event) {
      return win_event;
    } else {
      assert(false && "[BUG] Graph executor calls window_size(event_domain_tag) on time domain op.");
      // std::unreachable();
      return {};
    }
  }

  data_type window_size(time_domain_tag) const noexcept override {
    if constexpr (WIN == window_domain::time) {
      return win_time;
    } else {
      assert(false && "[BUG] Graph executor calls window_size(time_domain_tag) on event domain op.");
      // std::unreachable();
      return {};
    }
  }
};
} // namespace opflow::op::detail
