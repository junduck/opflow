#pragma once

#include <vector>

#include "agg_base.hpp"
#include "transform_base.hpp"
#include "window_base.hpp"

namespace opflow {
template <typename T>
struct aggregation : public transform_base<T> {
  using base = transform_base<T>;
  using typename base::data_type;

  std::shared_ptr<window_base<data_type>> window;
  std::vector<std::shared_ptr<agg_base<data_type>>> aggs;

  aggregation(std::shared_ptr<window_base<data_type>> window) : window(window) {}
};
} // namespace opflow
