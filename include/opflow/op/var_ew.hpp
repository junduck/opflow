
#include "opflow/op/detail/accum.hpp"
#include "opflow/op/detail/unary.hpp"

namespace opflow::op {
template <typename T>
struct var_ew : detail::unary_op<T> {
  using base = detail::unary_op<T>;
  using base::pos;
  detail::smooth m;  ///< mean
  detail::smooth s2; ///< variance
  double alpha;      ///< smoothing factor
  bool initialised;  ///< whether the first value has been processed

  explicit var_ew(double alpha, size_t pos = 0) noexcept : base{pos}, m{}, s2{}, alpha{alpha}, initialised{false} {
    assert(alpha > 0. && "alpha/period must be positive.");
    if (alpha >= 1.) {
      // alpha is actually a period
      this->alpha = 2.0 / (alpha + 1);
    }
  }

  void init(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    double const x = in[0][pos];

    m = x;              // Initialize with the first value
    s2 = 0.;            // Second moment starts at zero
    initialised = true; // Mark as initialized
  }

  void step(T, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    double const x = in[0][pos];

    if (!initialised) [[unlikely]] {
      m = x; // Initialize with the first value
      initialised = true;
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

template <typename T>
struct std_ew : public var_ew<T> {
  using base = var_ew<T>;
  using base::base;

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");

    base::value(out);
    out[1] = std::sqrt(out[1]);
  }
};
} // namespace opflow::op
