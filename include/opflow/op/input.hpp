#pragma once

#include <cassert>

#include "opflow/op_base.hpp"

namespace opflow::op {
template <typename T>
struct root_input : public op_base<T> {
  double const *mem;
  size_t input_size;

  explicit root_input(size_t n) : mem(), input_size(n) {}

  void step(T tick, double const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    std::ignore = tick; // Unused in root_input
    mem = in[0];        // Store the input data pointer
  }

  void value(double *out) noexcept override {
    assert(out && "NULL output buffer.");
    assert(mem && "No input data available.");
    std::copy(mem, mem + input_size, out);
  }

  size_t num_depends() const noexcept override { return 0; } // Root input has no dependencies
  size_t num_outputs() const noexcept override { return input_size; }
  size_t num_inputs(size_t) const noexcept override { return 0; } // No inputs
};
} // namespace opflow::op
