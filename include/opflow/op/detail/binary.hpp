#pragma once

#include <algorithm>
#include <cassert>

#include "opflow/op_base.hpp"

namespace opflow::op::detail {
template <typename T, typename U>
struct binary_op : public op_base<T, U> {
  size_t pos0, pos1;

  explicit binary_op(size_t pos0 = 0, size_t pos1 = 0) : pos0{pos0}, pos1{pos1} {}

  size_t num_depends() const noexcept override { return 2; }
  size_t num_inputs(size_t pid) const noexcept override {
    assert(pid < 2 && "binary operator expects input from predecessor id 0 or 1");
    return pid == 0 ? pos0 + 1 : pos1 + 1;
  }
  size_t num_outputs() const noexcept override { return 1; }
};

template <typename T, typename U>
struct weighted_op : public op_base<T, U> {
  size_t pos, pos_weight;

  weighted_op(size_t pos = 0, size_t pos_weight = 1) noexcept : pos{pos}, pos_weight{pos_weight} {}

  size_t num_depends() const noexcept override { return 1; }
  size_t num_inputs(size_t pid) const noexcept override {
    assert(pid == 0 && "weighted operator expects input from predecessor id 0");
    return std::max(pos, pos_weight) + 1; // Expect at least pos + 1 and pos_weight + 1 inputs
  }
  size_t num_outputs() const noexcept override { return 1; }
};
} // namespace opflow::op::detail
