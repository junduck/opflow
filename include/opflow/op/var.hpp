#pragma once

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/unary.hpp"

#ifndef NDEBUG
namespace opflow::op {
template <std::floating_point T>
T var_naive(std::span<T const> data) {
  // Naive variance (auto-variance, lag-1)
  size_t n = data.size();
  if (n < 2)
    return T{0};
  T mean = T{0};
  for (T v : data)
    mean += v;
  mean /= static_cast<T>(n);
  T var = T{0};
  for (T v : data)
    var += (v - mean) * (v - mean);
  var /= static_cast<T>(n - 1);
  return var;
}
} // namespace opflow::op
#endif

namespace opflow::op {
template <typename T, std::floating_point U>
struct moment2 : public detail::unary_op<T, U> {
  using base = detail::unary_op<T, U>;
  using base::pos;

  detail::smooth<U> m; ///< mean
  detail::accum<U> m2; ///< second moment
  size_t n;            ///< count of values processed

  explicit moment2(size_t pos = 0) noexcept : base{pos}, m{}, m2{}, n{} {}

  void init(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");

    n = 1;
    m = in[0][pos]; // Initialize with the first value
    m2 = 0.;        // Second moment starts at zero
  }

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    U const x = in[0][pos];

    ++n;
    U const d = x - m;
    m.add(x, 1.0 / n);   // update mean
    m2.add((x - m) * d); // update second moment
  }

  void inverse(T, U const *const rm) noexcept override {
    assert(rm && rm[0] && "NULL removal data.");
    U const x = rm[0][pos];

    --n;
    U const d = x - m;
    m.sub(x, 1.0 / n);   // undo mean update
    m2.sub((x - m) * d); // undo second moment update
  }

  // Currently we can not utilise rolling update due to engine design
  void roll(U x, U x0) noexcept {
    U const d = x - m;
    U const d0 = x0 - m;
    U const dx = x - x0;
    m.addsub(x, x0, 1.0 / n);
    m2.add(dx * (d - dx * (1.0 / n) + d0));
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");

    out[0] = m;
    out[1] = m2;
  }

  size_t num_outputs() const noexcept override { return 2; }
};

template <typename T, std::floating_point U, bool Unbiased = true>
struct var : public moment2<T, U> {
  using base = moment2<T, U>;
  using base::base;

  void value(U *out) noexcept override {
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

template <typename T, std::floating_point U, bool Unbiased = true>
struct stddev : public var<T, U, Unbiased> {
  using base = var<T, U, Unbiased>;
  using base::pos;

  using base::base;

  void value(U *out) noexcept override {
    assert(out && "NULL output buffer.");

    base::value(out);
    out[1] = std::sqrt(out[1]);
  }
};
} // namespace opflow::op
