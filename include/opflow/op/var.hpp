#pragma once

#include "opflow/def.hpp"
#include "opflow/op_base.hpp"

#include "opflow/detail/accum.hpp"

namespace opflow::op {
template <typename T>
struct moment2 : public win_base<T> {
public:
  using base = win_base<T>;
  using typename base::data_type;

  using base::base;

  void on_data(data_type const *in) noexcept override {
    data_type const x = in[0];

    ++n;
    data_type const d = x - m;
    m.add(x, 1.0 / n);   // update mean
    m2.add((x - m) * d); // update second moment
  }

  void on_evict(data_type const *rm) noexcept override {
    data_type const x = rm[0];

    --n;
    data_type const d = x - m;
    m.sub(x, 1.0 / n);   // undo mean update
    m2.sub((x - m) * d); // undo second moment update
  }

  // Currently we can not utilise rolling update due to engine design
  void roll(data_type x, data_type x0) noexcept {
    data_type const d = x - m;
    data_type const d0 = x0 - m;
    data_type const dx = x - x0;
    m.addsub(x, x0, 1.0 / n);
    m2.add(dx * (d - dx * (1.0 / n) + d0));
  }

  void value(data_type *out) const noexcept override {
    out[0] = m;
    out[1] = m2;
  }

  OPFLOW_INOUT(1, 2)
  OPFLOW_CLONEABLE(moment2)

protected:
  detail::smooth<data_type> m; ///< mean
  detail::accum<data_type> m2; ///< second moment
  size_t n;                    ///< count of values processed
};

template <typename T, bool Unbiased = true>
struct var : public moment2<T> {
  using base = moment2<T>;
  using typename base::data_type;

  using base::base;

  void value(data_type *out) const noexcept override {
    base::value(out);
    if constexpr (Unbiased) {
      if (this->n > 1) {
        out[1] /= (this->n - 1);
      } else {
        out[1] = 0.;
      }
    } else {
      out[1] /= this->n;
    }
  }
};

template <typename T, bool Unbiased = true>
struct stddev : public var<T, Unbiased> {
  using base = var<T, Unbiased>;
  using typename base::data_type;

  using base::base;

  void value(data_type *out) const noexcept override {
    base::value(out);
    out[1] = std::sqrt(out[1]);
  }
};
} // namespace opflow::op
