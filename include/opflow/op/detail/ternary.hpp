#pragma once

#include <cassert>

#include "opflow/op_base.hpp"

namespace opflow::op::detail {

template <typename T, std::floating_point U>
struct ternary_op : public op_base<T, U> {
  size_t pos0, pos1, pos2;

  explicit ternary_op(size_t pos0 = 0, size_t pos1 = 0, size_t pos2 = 0) : pos0{pos0}, pos1{pos1}, pos2{pos2} {}

  size_t num_depends() const noexcept override { return 3; }
  size_t num_inputs(size_t pid) const noexcept override {
    assert(pid < 3 && "ternary operator expects input from predecessor id 0, 1, or 2");
    switch (pid) {
    case 0:
      return pos0 + 1;
    case 1:
      return pos1 + 1;
    case 2:
      return pos2 + 1;
    }
  }
  size_t num_outputs() const noexcept override { return 1; }
};

} // namespace opflow::op::detail
