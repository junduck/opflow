#pragma once

#include <chrono>
#include <cstddef>
#include <vector>

#include "common.hpp"

namespace opflow {
/**
 * @brief Base class for all transform operations.
 *
 * Abstracts 1:1 transformation and N:1 transformation (aggregation)
 *
 * @tparam Time The time type used in the transform operation.
 * @tparam Data The data type used in the transform operation.
 */
template <typename Time, typename Data>
struct transform_base {
  using time_type = Time;
  using data_type = Data;

  /**
   * @brief Process incoming data.
   *
   * @param t The time associated with the incoming data.
   * @param in The input data to process.
   * @return true if an output is ready to be produced, false otherwise.
   */
  virtual bool on_data(time_type t, data_type const *in) noexcept = 0;

  /**
   * @brief Get the output value.
   *
   * This function is called to retrieve the output value after on_data() returns true.
   *
   * @param out The output buffer to fill.
   * @return time_type The time associated with the output.
   */
  virtual time_type value(data_type *out) noexcept = 0;

  /**
   * @brief Reset the transform state.
   *
   * This function is called to reset the internal state of the transform.
   */
  virtual void reset() noexcept = 0;

  /**
   * @brief Get the size of input.
   *
   * @return size_t The size of input.
   */
  virtual size_t num_inputs() const noexcept = 0;

  /**
   * @brief Get the size of output.
   *
   * @return size_t The size of output.
   */
  virtual size_t num_outputs() const noexcept = 0;

  virtual ~transform_base() noexcept = default;

  /**
   * @brief Check if this transform can be chained after prev.
   *
   * @param prev The previous transform to check compatibility with.
   * @return true If the transforms are compatible.
   * @return false If the transforms are not compatible.
   */
  bool compatible(transform_base const *prev) const noexcept { return prev->num_outputs() == num_inputs(); }
};

namespace trans {

template <typename Data>
struct static_cast_conv {
  template <typename T>
  auto operator()(T v) const noexcept {
    return static_cast<Data>(v);
  }
};

template <typename Data, typename Ratio>
struct chrono_conv {
  template <typename T>
  auto operator()(T timestamp) const noexcept {
    using target_t = std::chrono::duration<Data, Ratio>;
    auto dur_epoch = timestamp.time_since_epoch();
    auto dur_target = std::chrono::duration_cast<target_t>(dur_epoch);
    return dur_target.count();
  }
};

template <typename Data>
using chrono_us_conv = chrono_conv<Data, std::micro>;

template <typename Data>
using chrono_ms_conv = chrono_conv<Data, std::milli>;

template <typename Data>
using chrono_s_conv = chrono_conv<Data, std::ratio<1>>;

template <typename Data>
using chrono_min_conv = chrono_conv<Data, std::chrono::minutes::period>;

template <typename Data>
using chrono_h_conv = chrono_conv<Data, std::chrono::hours::period>;

/**
 * @brief Lagged delta time transform
 *
 * This transform calculates the lagged delta of the input data with respect to the time dimension.
 *
 * t_n, [data...(n)] -> t_n, [dT, data...(n - 1)] where dT = t_n - t_(n-1)
 * dT is prepended to the output data, as by opflow convention weight is root[0]
 *
 * @tparam Time The time type used in the transform operation.
 * @tparam Data The data type used in the transform operation.
 * @tparam ConvTime The time conversion function used in the transform operation.
 */
template <typename Time, typename Data, typename ConvTime>
struct with_time_delta : public transform_base<Time, Data> {
  using time_type = Time;
  using dura_type = duration_t<time_type>;
  using data_type = Data;

  std::vector<data_type> buf; // size: 2 * (in_size + 1)
  size_t tick;
  time_type timestamp;
  const size_t in_size;

  with_time_delta(size_t in_size) : buf(2 * (in_size + 1)), tick(0), in_size(in_size) {}

  bool on_data(time_type t, data_type const *in) noexcept override {
    using diff_t = std::vector<data_type>::difference_type;
    size_t const stride = in_size + 1;
    size_t const curr = (tick & 1) ? stride : 0;
    size_t const prev = (tick & 1) ? 0 : stride;

    timestamp = t;

    buf[curr] = ConvTime{}(t);
    auto base = buf.begin();
    std::copy(in, in + in_size, base + static_cast<diff_t>(curr + 1));

    if (tick++ == 0) {
      return false; // no output yet
    }

    buf[prev] = buf[curr] - buf[prev]; // calculate dT

    return true; // output is ready
  }

  time_type value(data_type *out) noexcept override {
    using diff_t = std::vector<data_type>::difference_type;
    diff_t const stride = static_cast<diff_t>(in_size) + 1;
    diff_t const ready = (tick & 1) ? stride : 0;

    auto base = buf.begin();
    std::copy(base + ready, base + ready + stride, out);
    return timestamp;
  }

  void reset() noexcept override { tick = 0; }

  size_t num_inputs() const noexcept override { return in_size; }
  size_t num_outputs() const noexcept override { return in_size + 1; }
};
} // namespace trans
} // namespace opflow
