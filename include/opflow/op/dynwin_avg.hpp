#pragma once

#include "opflow/def.hpp"
#include "opflow/op_base.hpp"

namespace opflow::op {
// This is a toy operator that demonstrate dynamic windowing
template <typename T, win_type WIN = win_type::event>
struct dynwin_avg : public win_base<T, WIN> {
  using base = win_base<T, WIN>;
  using typename base::data_type;

  data_type sum, mean, m2, thres;
  size_t count;

  explicit dynwin_avg(size_t win_event, data_type thres) noexcept
    requires(WIN == win_type::event)
      : base(win_event), thres(thres) {}

  explicit dynwin_avg(data_type win_time, data_type thres) noexcept
    requires(WIN == win_type::time)
      : base(win_time), thres(thres) {}

  void on_data(data_type const *in) noexcept override {
    auto const x = in[0];
    sum += x;
    count++;
    auto const d = x - mean;
    mean = sum / count;
    m2 += d * (x - mean);
    if (m2 > thres * thres) {
      // Trigger dynamic window, double window size
      if constexpr (WIN == win_type::event) {
        this->win_event *= 2;
      } else if constexpr (WIN == win_type::time) {
        this->win_time *= 2;
      }
      m2 = 0; // Reset M2 after triggering
    }
  }

  void on_evict(data_type const *rm) noexcept override {
    auto const x = rm[0];
    sum -= x;
    count--;
    auto const d = x - mean;
    mean = sum / count;
    m2 += d * (x - mean);
    // we dont trigger dynamic window on eviction
  }

  void value(data_type *out) const noexcept override { out[0] = mean; }

  void reset() noexcept override {
    sum = 0;
    count = 0;
    mean = 0;
    m2 = 0;
  }

  bool is_dynamic() const noexcept override { return true; }
  size_t num_inputs() const noexcept override { return 1; }
  size_t num_outputs() const noexcept override { return 1; }

  OPFLOW_CLONEABLE(dynwin_avg)
};
} // namespace opflow::op
