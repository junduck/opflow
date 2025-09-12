#pragma once

#include "opflow/def.hpp"
#include "opflow/op_base.hpp"

#include "opflow/detail/accum.hpp"

namespace opflow::op {
template <typename T, T zero_limit = feps100<T>>
class rsi : public op_base<T> {
public:
  using base = op_base<T>;
  using typename base::data_type;

  explicit rsi(data_type alpha = data_type(14))
      : avg_gain(), avg_loss(), last_x(), alpha(detail::smooth_wilders(alpha)), init(false) {}

  void on_data(data_type const *in) noexcept override {
    data_type const current = in[0];

    if (!init) {
      last_x = current;
      init = true;
      return;
    }

    if (current > last_x) {
      avg_gain.add(current - last_x, alpha); // Update average gain
      avg_loss.add(0, alpha);                // No loss
    } else if (current < last_x) {
      avg_loss.add(last_x - current, alpha); // Update average loss
      avg_gain.add(0, alpha);                // No gain
    } else {
      avg_gain.add(0, alpha); // No change
      avg_loss.add(0, alpha); // No change
    }
    last_x = current; // Update last value for next comparison
  }

  void value(data_type *out) const noexcept override {
    if (avg_loss < zero_limit) {
      out[0] = 100; // RSI is 100 when there are no losses
    } else {
      data_type rs = avg_gain / avg_loss;                               // Relative Strength
      out[0] = data_type(100) - (data_type(100) / (data_type(1) + rs)); // Calculate RSI
    }
  }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(rsi)

private:
  detail::smooth<data_type> avg_gain; ///< Average gain
  detail::smooth<data_type> avg_loss; ///< Average loss
  data_type last_x;                   ///< Last value for comparison
  data_type const alpha;
  bool init; ///< Initialization flag
};
} // namespace opflow::op
