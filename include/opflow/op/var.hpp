#pragma once

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/unary.hpp"

namespace opflow::op {
template <typename T>
struct moment2 : public detail::unary_op<T> {
  using base = detail::unary_op<T>;
  using base::pos;

  detail::smooth m; ///< mean
  detail::accum m2; ///< second moment
  size_t n;         ///< count of values processed

  explicit moment2(size_t pos = 0) noexcept : base{pos}, m{}, m2{}, n{} {}

  void init(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");

    n = 1;
    m = in[0][pos]; // Initialize with the first value
    m2 = 0.;        // Second moment starts at zero
  }

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    double const x = in[0][pos];

    ++n;
    double const d = x - m;
    m.add(x, 1.0 / n);   // update mean
    m2.add((x - m) * d); // update second moment
  }

  void inverse(T, double const *const rm) noexcept override {
    assert(rm && rm[0] && "NULL removal data.");
    double const x = rm[0][pos];

    --n;
    double const d = x - m;
    m.sub(x, 1.0 / n);   // undo mean update
    m2.sub((x - m) * d); // undo second moment update
  }

  // Currently we can not utilise rolling update due to engine design
  void roll(double x, double x0) noexcept {
    double const d = x - m;
    double const d0 = x0 - m;
    double const dx = x - x0;
    m.addsub(x, x0, 1.0 / n);
    m2.add(dx * (d - dx * (1.0 / n) + d0));
  }

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");

    out[0] = m;
    out[1] = m2;
  }

  size_t num_outputs() const noexcept override { return 2; }
};

template <typename T, bool Unbiased = true>
struct var : public moment2<T> {
  using base = moment2<T>;
  using base::base;

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");
    assert(this->n > 0 && "value called with empty state.");

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
  using base::pos;

  using base::base;

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");

    base::value(out);
    out[1] = std::sqrt(out[1]);
  }
};
} // namespace opflow::op
