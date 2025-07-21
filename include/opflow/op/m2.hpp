#pragma once

#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/binary.hpp"
#include "opflow/op/detail/unary.hpp"
#include "opflow/op_base.hpp"

namespace opflow::op {
template <time_point_like T>
struct moment2 : public detail::unary_op<T> {
  using base = detail::unary_op<T>;
  using base::pos;

  detail::smooth m; ///< mean
  detail::accum m2; ///< second moment
  size_t n;         ///< count of values processed

  explicit moment2(size_t pos = 0) noexcept : base{pos}, m{}, m2{}, n{} {}

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

template <time_point_like T, bool Unbiased = true>
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

template <time_point_like T, bool Unbiased = true>
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

template <time_point_like T>
struct s2vw : public detail::weighted_op<T> {
  using base = detail::weighted_op<T>;
  using base::pos;
  using base::pow_weight;

  detail::smooth m;     ///< mean
  detail::accum w_sum;  ///< sum of weights
  detail::accum w2_sum; ///< sum of squared weights
  detail::accum m2;     ///< second moment
  size_t n;             ///< count of values processed

  explicit s2vw(size_t pos = 0, size_t pow_weight = 1) noexcept
      : base{pos, pow_weight}, m{}, w_sum{}, w2_sum{}, m2{}, n{} {}

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    double const x = in[0][pos];
    double const w = in[0][pow_weight];

    ++n;
    w_sum.add(w);
    w2_sum.add(w * w);

    double const d = x - m;
    m.add(x, w / w_sum);
    m2.add((x - m) * d * w);
  }

  void inverse(T, double const *const rm) noexcept override {
    assert(rm && rm[0] && "NULL removal data.");
    double const x = rm[0][pos];
    double const w = rm[0][pow_weight];

    --n;
    w_sum.sub(w);
    w2_sum.sub(w * w);

    double const d = x - m;
    m.sub(x, w / w_sum);
    m2.sub((x - m) * d * w);
  }

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");
    assert(this->n > 0 && "value called with empty state.");

    out[0] = m;
    if (n == 1) [[unlikely]] {
      out[1] = 0.;
    } else {
      double const rel_weight = w_sum - w2_sum / w_sum;
      out[1] = m2 / rel_weight;
    }
  }
};

template <time_point_like T>
struct stdvw : public s2vw<T> {
  using base = s2vw<T>;
  using base::base;

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");

    base::value(out);
    out[1] = std::sqrt(out[1]);
  }
};

template <time_point_like T>
struct s2ew : detail::unary_op<T> {
  using base = detail::unary_op<T>;
  using base::pos;
  detail::smooth m;  ///< mean
  detail::smooth s2; ///< variance
  double alpha;      ///< smoothing factor
  bool init;         ///< whether the first value has been processed

  explicit s2ew(double alpha, size_t pos = 0) noexcept : base{pos}, m{}, s2{}, alpha{alpha}, init{false} {}
  explicit s2ew(size_t period, size_t pos = 0) noexcept : s2ew{2.0 / (period + 1), pos} {}

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    double const x = in[0][pos];

    if (!init) [[unlikely]] {
      m = x; // Initialize with the first value
      init = true;
      return;
    }

    double const d = x - m;
    // double const a1 = 1.0 - alpha;
    m.add(x, alpha);
    // s2.add(a1 * d * d, alpha);
    double const d2 = x - m;
    s2.add(d * d2, alpha); // use Welford-like method for one less multiplication
  }

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");

    out[0] = m;
    out[1] = s2;
  }
};

template <time_point_like T>
struct stdew : public s2ew<T> {
  using base = s2ew<T>;
  using base::base;

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");

    base::value(out);
    out[1] = std::sqrt(out[1]);
  }
};

template <time_point_like T>
struct cov : public detail::binary_op<T> {
  using base = detail::binary_op<T>;
  using base::base;
  using base::pos0;
  using base::pos1;

  detail::smooth mx; ///< mean of first input
  detail::smooth my; ///< mean of second input
  detail::accum mxy; ///< m2 of cross product
  size_t n;          ///< count of values processed

  explicit cov(size_t pos0 = 0, size_t pos1 = 1) noexcept : base{pos0, pos1}, mx{}, my{}, mxy{}, n{} {}

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && in[1] && "NULL input data.");
    double const x = in[0][pos0];
    double const y = in[1][pos1];

    ++n;
    double const a = 1.0 / n;
    double const dy = y - my;
    mx.add(x, a);
    my.add(y, a);
    mxy.add((x - mx) * dy);
  }

  void inverse(T, double const *const rm) noexcept override {
    assert(rm && rm[0] && rm[1] && "NULL removal data.");
    double const x = rm[0][pos0];
    double const y = rm[1][pos1];

    --n;
    double const a = 1.0 / n;
    double const dy = y - my;
    mx.sub(x, a);
    my.sub(y, a);
    mxy.sub((x - mx) * dy);
  }

  // Currently we can not utilise rolling update due to engine design
  void roll(double x, double x0, double y, double y0) noexcept {
    double const dy = y - my;
    double const dy0 = y0 - my;
    double const a = 1.0 / n;
    mx.addsub(x, x0, a);
    my.addsub(y, y0, a);
    mxy.addsub((x - mx) * dy, (x0 - mx) * dy0);
  }

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");
    assert(this->n > 0 && "value called with empty state.");

    out[0] = mx; // mean of first input
    out[1] = my; // mean of second input
    if (n == 1) [[unlikely]] {
      out[2] = 0.; // covariance is zero if only one sample
    } else {
      out[2] = mxy / (n - 1); // unbiased covariance
    }
  }

  size_t num_outputs() const noexcept override {
    return 3; // mx, my, cov
  }
};
} // namespace opflow::op
