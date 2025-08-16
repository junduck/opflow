#pragma once

#include <algorithm>
#include <array>

#include "opflow/op_base.hpp"

namespace opflow::op {
/**
 * @brief Simple root node that copies input data to its output.
 *
 */
template <std::floating_point U>
struct simple_root : op_base<U> {
  using base = op_base<U>;
  using typename base::data_type;

  data_type const *mem;
  size_t const input_size;

  explicit simple_root(size_t n) : mem(nullptr), input_size(n) {}

  void on_data(data_type const *in) noexcept override { mem = in; }
  void value(data_type *out) const noexcept override { std::copy(mem, mem + input_size, out); }
  void reset() noexcept override {}

  size_t num_inputs() const noexcept override { return input_size; }
  size_t num_outputs() const noexcept override { return input_size; }
};

template <std::floating_point U>
struct ohlcv_root : op_base<U> {
  using base = op_base<U>;
  using typename base::data_type;

  std::array<data_type, 5> mem;    // Open, High, Low, Close, Volume
  std::array<size_t, 5> const idx; // Indexes for each OHLCV component

  ohlcv_root(size_t open_idx, size_t high_idx, size_t low_idx, size_t close_idx, size_t volume_idx)
      : mem{}, idx{open_idx, high_idx, low_idx, close_idx, volume_idx} {}

  void on_data(data_type const *in) noexcept override {
    for (size_t i = 0; i < mem.size(); ++i) {
      mem[i] = in[idx[i]];
    }
  }

  void value(data_type *out) const noexcept override {
    for (size_t i = 0; i < mem.size(); ++i) {
      out[i] = mem[i];
    }
  }

  void reset() noexcept override { mem.fill(0); }

  size_t num_inputs() const noexcept override { return 5; }
  size_t num_outputs() const noexcept override { return 5; }
};
} // namespace opflow::op
