#pragma once

#include "../def.hpp"
#include "../op_base.hpp"

namespace opflow::op {
// This is a toy operator that demonstrate dynamic windowing
template <typename T>
class dynwin_avg : public simple_rollop<T> {
public:
  using base = simple_rollop<T>;
  using typename base::data_type;

  explicit dynwin_avg(size_t win_event, data_type thres) noexcept : base(win_event), thres(thres) {}
  explicit dynwin_avg(data_type win_time, data_type thres) noexcept : base(win_time), thres(thres) {}

  void on_data(data_type const *in) noexcept override {
    auto const x = in[0];
    sum += x;
    count++;
    auto const d = x - mean;
    mean = sum / count;
    m2 += d * (x - mean);
    if (m2 > thres * thres) {
      // Trigger dynamic window, double window size
      double_window();
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

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(dynwin_avg)

private:
  data_type sum, mean, m2, thres;
  size_t count;

  void double_window() {
    std::visit([](auto &v) { v *= 2; }, this->win_size);
  }
};
} // namespace opflow::op
