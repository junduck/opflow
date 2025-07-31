#pragma once

#include "detail/accum.hpp"
#include "detail/binary.hpp"

namespace opflow::op {
template <typename T, typename U>
struct macd : public detail::binary_op<T, U> {
  detail::smooth<U> sig; ///< Signal line value
  U cd;                  ///< Current difference
  U hist;                ///< Histogram value
  U alpha;
  bool init; ///< Initialization flag

  using base = detail::binary_op<T, U>;
  using base::pos0;
  using base::pos1;

  macd(U alpha, size_t pos0 = 0, size_t pos1 = 0)
      : base(pos0, pos1), sig(), cd(), hist(), alpha(detail::smooth_factor(alpha)), init(false) {}

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    U const mfast = in[0][pos0];
    U const mslow = in[1][pos1];

    cd = mfast - mslow; // Calculate the current difference
    if (!init) {
      sig = cd;
      init = true;
    } else {
      sig.add(cd, alpha); // Update the signal line with the current difference
    }
    hist = cd - sig; // Calculate the histogram value
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");
    out[0] = cd;   // Output the current difference
    out[1] = sig;  // Output the signal line value
    out[2] = hist; // Output the histogram value
  }

  size_t num_outputs() const noexcept override {
    return 3; // Three outputs: current difference, signal line, and histogram
  }
};

/*
Equivalent:
auto ma_fast = make_shared<op::ema<T, U>>(12);
auto ma_slow = make_shared<op::ema<T, U>>(26);
g.add_vertex(ma_fast, vec{in});
g.add_vertex(ma_slow, vec{in});
auto cd = make_shared<op::sub<T, U>>();
g.add_vertex(cd, vec{ma_fast, ma_slow});
auto sig = make_shared<op::ema<T, U>>(9);
g.add_vertex(sig, vec{cd});
auto hist = make_shared<op::sub<T, U>>();
g.add_vertex(hist, vec{cd, sig});
*/
} // namespace opflow::op
