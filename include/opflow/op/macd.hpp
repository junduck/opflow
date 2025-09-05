#pragma once

#include "opflow/def.hpp"
#include "opflow/op_base.hpp"

#include "opflow/detail/accum.hpp"

namespace opflow::op {
template <typename T>
class macd : public op_base<T> {
public:
  using base = op_base<T>;
  using typename base::data_type;

  explicit macd(data_type alpha) : sig(), cd(), hist(), alpha(detail::smooth_factor(alpha)), init(false) {}

  void on_data(data_type const *in) noexcept override {
    data_type const mfast = in[0];
    data_type const mslow = in[1];

    cd = mfast - mslow; // Calculate the current difference
    if (!init) {
      sig = cd;
      init = true;
    } else {
      sig.add(cd, alpha); // Update the signal line with the current difference
    }
    hist = cd - sig; // Calculate the histogram value
  }

  void value(data_type *out) const noexcept override {
    out[0] = cd;   // Output the current difference
    out[1] = sig;  // Output the signal line value
    out[2] = hist; // Output the histogram value
  }

  void reset() noexcept override {
    sig.reset();
    cd = 0.;
    hist = 0.;
    init = false;
  }

  OPFLOW_INOUT(1, 3)
  OPFLOW_CLONEABLE(macd)

private:
  detail::smooth<data_type> sig; ///< Signal line value
  data_type cd;                  ///< Current difference
  data_type hist;                ///< Histogram value
  data_type const alpha;
  bool init; ///< Initialization flag
};

static_assert(dag_node<macd<double>>);
} // namespace opflow::op
