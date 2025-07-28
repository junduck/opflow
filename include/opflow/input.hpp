#pragma once

#include <algorithm>
#include <tuple>

#include "op_base.hpp"

namespace opflow {
template <typename Time, typename Data>
struct noop_input_transform {
  using time_type = Time;
  using data_type = Data;

  noop_input_transform() noexcept = default;

  size_t n;

  noop_input_transform(size_t size) noexcept : n(size) {
    if (n == 0) {
      throw std::invalid_argument("Input size must be greater than zero.");
    }
  }

  // No transformation, just return the input as is
  bool transform(time_type time, data_type const *data, data_type *output) const noexcept {
    std::ignore = time; // Unused in noop transform
    if (!data || !output) {
      return false; // Invalid input or output
    }
    std::copy(data, data + n, output);
    return true;
  }

  size_t num_inputs() const noexcept {
    return n; // Number of inputs is the size of the data
  }

  size_t num_outputs() const noexcept {
    return n; // Number of outputs is the same as inputs
  }
};

template <typename Time, typename Data>
struct aggr_input_transform {
  using time_type = Time;
  using data_type = Data;

  std::vector<std::shared_ptr<op_base<time_type, data_type>>> agg;

  template <std::ranges::forward_range R>
  aggr_input_transform(R &&agg_ops) noexcept : agg(std::ranges::begin(agg_ops), std::ranges::end(agg_ops)) {
    if (agg.empty()) {
      throw std::invalid_argument("Aggregation operations cannot be empty.");
    }
    n = agg[0]->num_outputs();
    for (const auto &op : agg) {
      if (op->num_outputs() != n) {
        throw std::invalid_argument("All aggregation operations must have the same number of outputs.");
      }
    }
  }

  size_t n;

  aggr_input_transform(size_t size) noexcept : n(size) {
    if (n == 0) {
      throw std::invalid_argument("Input size must be greater than zero.");
    }
  }

  // Aggregate inputs by summing them
  bool transform(time_type time, data_type const *data, data_type *output) const noexcept {
    std::ignore = time; // Unused in aggr transform
    if (!data || !output) {
      return false; // Invalid input or output
    }
    std::fill(output, output + n, data_type{});
    for (size_t i = 0; i < n; ++i) {
      output[i] += data[i];
    }
    return true;
  }

  size_t num_inputs() const noexcept {
    return n; // Number of inputs is the size of the data
  }

  size_t num_outputs() const noexcept {
    return n; // Number of outputs is the same as inputs
  }
};
} // namespace opflow
