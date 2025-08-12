#pragma once

#include <vector>

#include "opflow/transform_base.hpp"

namespace opflow::trans {
/**
 * @brief Lagged delta transform
 *
 * t (n), data... (n) -> t (n), dT, data... (n - 1) where dT = t (n) - t (n - 1)
 * dT is PREPENDED to the output data
 *
 * @tparam Time The time type used in the transform operation.
 * @tparam Data The data type used in the transform operation.
 * @tparam ConvTime The time conversion function used in the transform operation.
 */
template <typename Time, typename Data, typename ConvTime>
struct with_time_delta : public transform_base<Time, Data> {
  using base = transform_base<Time, Data>;
  using typename base::data_type;
  using typename base::time_type;

  std::vector<data_type> buf; // size: 2 * (in_size + 1)
  size_t tick;
  time_type timestamp;
  size_t const in_size;

  with_time_delta(size_t in_size) : buf(2 * (in_size + 1)), tick(0), timestamp(), in_size(in_size) {}

  bool on_data(time_type t, data_type const *in) noexcept override {
    using diff_t = std::vector<data_type>::difference_type;
    size_t const stride = in_size + 1;
    size_t const curr = (tick & 1) ? stride : 0;
    size_t const prev = (tick & 1) ? 0 : stride;

    timestamp = t;
    buf[curr] = ConvTime{}(t);
    std::copy(in, in + in_size, buf.begin() + static_cast<diff_t>(curr + 1));

    if (tick++ == 0) {
      return false; // no output yet
    }

    buf[prev] = buf[curr] - buf[prev]; // calculate dT

    return true; // output is ready
  }

  time_type value(data_type *out) const noexcept override {
    using diff_t = std::vector<data_type>::difference_type;
    diff_t const stride = static_cast<diff_t>(in_size) + 1;
    diff_t const ready = (tick & 1) ? stride : 0;

    std::copy(buf.begin() + ready, buf.begin() + ready + stride, out);
    return timestamp;
  }

  void reset() noexcept override { tick = 0; }

  size_t num_inputs() const noexcept override { return in_size; }
  size_t num_outputs() const noexcept override { return in_size + 1; }
};

/**
 * @brief Lagged delta transform
 *
 * t (n), data... (n) -> t (n), dX, data... (n - 1) where dX = data[i] (n) - data[i] (n - 1)
 * dX is PREPENDED to the output data
 *
 * @tparam Time The time type used in the transform operation.
 * @tparam Data The data type used in the transform operation.
 */
template <typename Time, typename Data>
struct with_delta : public transform_base<Time, Data> {
  using base = transform_base<Time, Data>;
  using typename base::data_type;
  using typename base::time_type;

  std::vector<data_type> buf; // size: 2 * (in_size + 1)
  size_t tick;
  time_type timestamp;
  size_t const idx;
  size_t const in_size;

  with_delta(size_t in_size, size_t inspect_index)
      : buf(2 * (in_size + 1)), tick(0), timestamp(), idx(inspect_index), in_size(in_size) {}

  bool on_data(time_type t, data_type const *in) noexcept override {
    using diff_t = std::vector<data_type>::difference_type;
    size_t const stride = in_size + 1;
    size_t const curr = (tick & 1) ? stride : 0;
    size_t const prev = (tick & 1) ? 0 : stride;

    timestamp = t;
    buf[curr] = in[idx];
    std::copy(in, in + in_size, buf.begin() + static_cast<diff_t>(curr + 1));

    if (tick++ == 0) {
      return false; // no lagged value yet
    }

    buf[prev] = buf[curr] - buf[prev]; // calculate lagged delta

    return true; // output is ready
  }

  time_type value(data_type *out) const noexcept override {
    using diff_t = std::vector<data_type>::difference_type;
    diff_t const stride = static_cast<diff_t>(in_size) + 1;
    diff_t const ready = (tick & 1) ? stride : 0;

    std::copy(buf.begin() + ready, buf.begin() + ready + stride, out);
    return timestamp;
  }

  void reset() noexcept override { tick = 0; }

  size_t num_inputs() const noexcept override { return in_size; }
  size_t num_outputs() const noexcept override { return in_size + 1; }
};
} // namespace opflow::trans
