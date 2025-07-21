#pragma once

#include <cassert>

#include "opflow/op_base.hpp"

namespace opflow::op::detail {
template <time_point_like T>
struct unary_op : public op_base<T> {
  size_t pos;

  explicit unary_op(size_t pos = 0) : pos{pos} {}

  size_t num_depends() const noexcept override { return 1; }
  size_t num_inputs(size_t pid) const noexcept override {
    assert(pid == 0 && "unary operator expects input from predecessor id 0");
    return pos + 1; // Expect at least pos + 1 inputs
  }
  size_t num_outputs() const noexcept override { return 1; }
};
} // namespace opflow::op::detail
