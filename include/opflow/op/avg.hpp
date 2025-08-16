#pragma once

#include "detail/accum.hpp"
#include "detail/static_win.hpp"

namespace opflow::op {
template <std::floating_point U, window_domain WIN = window_domain::event>
struct avg : public detail::static_win<U, WIN> {
  using base = detail::static_win<U, WIN>;
  using typename base::data_type;

  detail::smooth<U> val; ///< mean value
  size_t n;              ///< count of values processed

  using base::base;

  void on_data(data_type const *in) noexcept override {
    auto const x = in[0];
    ++n;
    val.add(x, U(1) / n);
  }

  void on_evict(data_type const *rm) noexcept override {
    auto const x = rm[0];
    --n;
    val.add(x, U(1) / n);
  }

  void value(data_type *out) const noexcept override { out[0] = val; }

  void reset() noexcept override {
    val.reset();
    n = 0;
  }

  size_t num_inputs() const noexcept override { return 1; }
  size_t num_outputs() const noexcept override { return 1; }
};

template <std::floating_point U, window_domain WIN = window_domain::event>
struct avg_weighted : public detail::static_win<U, WIN> {
  using base = detail::static_win<U, WIN>;
  using typename base::data_type;

  detail::smooth<U> val;  ///< weighted mean value
  detail::accum<U> w_sum; ///< sum of weights

  using base::base;

  void on_data(data_type const *in) noexcept override {
    auto const x = in[0];
    auto const w = in[1];
    w_sum.add(w);
    val.add(x, w / w_sum);
  }

  void on_evict(data_type const *rm) noexcept override {
    auto const x = rm[0];
    auto const w = rm[1];
    w_sum.sub(w);
    val.sub(x, w / w_sum);
  }

  void value(data_type *out) const noexcept override { out[0] = val; }

  void reset() noexcept override {
    val.reset();
    w_sum.reset();
  }

  size_t num_inputs() const noexcept override { return 2; }
  size_t num_outputs() const noexcept override { return 1; }
};
} // namespace opflow::op
