#pragma once

#include "detail/unary.hpp"

namespace opflow::op {
enum class lag_init {
  zero,    ///< Use zero as lagged value if first lagged value is not available
  epsilon, ///< Use a small value (feps) as lagged value if first lagged value is not available
  nan,     ///< Use NaN as lagged value if first lagged value is not available
  oldest,  ///< Use the oldest value as lagged value if first lagged value is not available
};

template <typename T, std::floating_point U, lag_init Init = lag_init::oldest>
struct lag : public detail::unary_op<T, U> {
  using base = detail::unary_op<T, U>;
  using base::pos;

  U lagged;   ///< Lagged value
  bool avail; ///< Whether the lagged value is available

  explicit lag(size_t pos = 0) noexcept : base{pos}, lagged{}, avail{false} {}

  void init(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    avail = false;
  }

  void step(T, U const *const *in) noexcept override {
    if constexpr (Init == lag_init::oldest) {
      if (!avail) {
        lagged = in[0][pos]; // set value, treat it as the lagged value
        avail = true;
      }
    }
  }

  void inverse(T, U const *const *rm) noexcept override {
    assert(rm && rm[0] && "NULL input data.");
    lagged = rm[0][pos]; // Store the lag value
    avail = true;        // Mark as available
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output data.");
    if (avail) {
      out[0] = lagged;
    } else {
      if constexpr (Init == lag_init::zero) {
        out[0] = U{}; // Initialise with zero
      } else if constexpr (Init == lag_init::epsilon) {
        out[0] = feps<U>; // Initialise with a small value
      } else {
        out[0] = fnan<U>; // Initialise with NaN
      }
    }
  }
};
} // namespace opflow::op
