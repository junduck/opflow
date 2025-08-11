#pragma once

#include <algorithm>
#include <cstddef>
#include <span>

namespace opflow {
template <typename Data>
struct agg_base {
  using data_type = Data;

  /// @note by definition on_data should be pure (const) as an aggregator but we relax this for flexibility

  virtual void on_data(size_t n, data_type const *const *in, data_type *out) noexcept = 0;

  virtual size_t num_inputs() const noexcept = 0;
  virtual size_t num_outputs() const noexcept = 0;

  virtual void reset() noexcept {}

  virtual ~agg_base() noexcept = default;
};

namespace agg {
template <typename Data>
struct ohlc : public agg_base<Data> {
  using data_type = Data;

  void on_data(size_t n, data_type const *const *in, data_type *out) noexcept override {
    std::span<data_type const> input(in[0], n);
    auto const o = input.front();
    auto const h = *std::ranges::max_element(input);
    auto const l = *std::ranges::min_element(input);
    auto const c = input.back();

    out[0] = o;
    out[1] = h;
    out[2] = l;
    out[3] = c;
  }

  size_t num_inputs() const noexcept override { return 1; }
  size_t num_outputs() const noexcept override { return 4; }
};
} // namespace agg
} // namespace opflow
