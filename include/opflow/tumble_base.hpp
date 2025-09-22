#pragma once

#include <cstddef>

namespace opflow {
template <typename T>
struct tumble_spec {
  T timestamp;  ///< Timestamp associated with this window
  bool include; ///< Whether to include the current data point in the window
};

template <typename T>
struct tumble_base {
  using data_type = T;
  using spec_type = tumble_spec<T>;

  virtual bool on_data(data_type t, data_type const *in) noexcept = 0;
  virtual spec_type emit() noexcept = 0;

  virtual tumble_base *clone_at(void *mem) const noexcept = 0;
  virtual size_t clone_size() const noexcept = 0;
  virtual size_t clone_align() const noexcept = 0;

  virtual ~tumble_base() noexcept = default;
};
} // namespace opflow
