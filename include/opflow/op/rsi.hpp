#pragma once

#include "detail/accum.hpp"
#include "detail/unary.hpp"

namespace opflow::op {
template <typename T, typename U, U zero_limit = feps100<U>>
struct rsi : public detail::unary_op<T, U> {
  detail::smooth<U> avg_gain; ///< Average gain
  detail::smooth<U> avg_loss; ///< Average loss
  U last_x;                   ///< Last value for comparison
  U alpha;
  bool init; ///< Initialization flag

  using base = detail::unary_op<T, U>;
  using base::pos;

  rsi(U alpha = U(14), size_t pos = 0)
      : base(pos), avg_gain(), avg_loss(), last_x(), alpha(detail::smooth_wilders(alpha)), init(false) {}

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    U const current = in[0][pos];

    if (!init) {
      avg_gain = U{};
      avg_loss = U{};
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

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    if (avg_loss < zero_limit) {
      out[0] = 100; // RSI is 100 when there are no losses
    } else {
      U rs = avg_gain / avg_loss;               // Relative Strength
      out[0] = U(100) - (U(100) / (U(1) + rs)); // Calculate RSI
    }
  }
};

/*
Equivalent :
auto gain = make_shared<op::gain<T, U>>();
g.add_vertex(gain, vec{in});
auto loss = make_shared<op::loss<T, U>>();
g.add_vertex(loss, vec{in});
auto avg_gain = make_shared<op::ema<T, U>>(detail::smooth_wilders(14));
g.add_vertex(avg_gain, vec{gain});
auto avg_loss = make_shared<op::ema<T, U>>(detail::smooth_wilders(14));
g.add_vertex(avg_loss, vec{loss});
auto rsi = make_shared<op::custom_binary_op<T, U>>([](U g, U l) {
  if (l < feps100<U>) {
    return U(100);
  }
  U rs = g / l;
  return U(100) - (U(100) / (U(1) + rs));
});
g.add_vertex(rsi, vec{avg_gain, avg_loss});
*/
} // namespace opflow::op
