#pragma once

#include "detail/unary.hpp"

namespace opflow::op {
enum class diff_init {
  zero,    ///< Initialise with zero
  epsilon, ///< Initialise with a small value (epsilon), useful for avoiding zero division
  first,   ///< Initialise with the first value
  nan,     ///< Initialise with NaN
};

template <typename T, std::floating_point U, diff_init Init = diff_init::zero>
struct diff : public detail::unary_op<T, U> {
  using base = detail::unary_op<T, U>;
  using base::pos;

  U last;           ///< Last value
  U delta;          ///< Current delta value
  bool initialised; ///< Whether the first value has been processed

  explicit diff(size_t pos = 0) noexcept : base{pos}, last{}, delta{}, initialised{false} {}

  void init(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    initialised = false;
    step(T{}, in);
  }

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    if (!initialised) {
      if constexpr (Init == diff_init::zero) {
        delta = 0.; // Initialise with zero
      } else if constexpr (Init == diff_init::epsilon) {
        delta = feps<U>; // Initialise with a small value
      } else if constexpr (Init == diff_init::first) {
        delta = in[0][pos]; // Initialise with the first value
      } else {
        delta = fnan<U>; // Initialise with NaN
      }
      initialised = true;
    } else {
      delta = in[0][pos] - last; // Calculate the difference
    }
    last = in[0][pos];
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output data.");
    out[pos] = delta; // Output the delta value
  }
};

template <typename T, std::floating_point U, diff_init Init = diff_init::epsilon>
  requires(Init != diff_init::first)
struct gain : public detail::unary_op<T, U> {
  using base = detail::unary_op<T, U>;
  using base::pos;

  U last;           ///< Last value
  U delta;          ///< Current delta value
  bool initialised; ///< Whether the first value has been processed

  explicit gain(size_t pos = 0) noexcept : base{pos}, last{}, delta{}, initialised{false} {}

  void init(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    initialised = false;
    step(T{}, in);
  }

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    if (!initialised) {
      if constexpr (Init == diff_init::zero) {
        delta = 0.; // Initialise with zero
      } else if constexpr (Init == diff_init::epsilon) {
        delta = feps<U>; // Initialise with a small value
      } else {
        delta = fnan<U>; // Initialise with NaN
      }
      initialised = true;
    } else {
      if (in[0][pos] < last) {
        delta = 0; // No gain if current value is less than last
      } else {
        delta = in[0][pos] - last;
      }
    }
    last = in[0][pos];
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output data.");
    out[pos] = delta;
  }
};

// RSI style loss
template <typename T, std::floating_point U, diff_init Init = diff_init::epsilon>
  requires(Init != diff_init::first)
struct loss : public detail::unary_op<T, U> {
  using base = detail::unary_op<T, U>;
  using base::pos;

  U last;           ///< Last value
  U delta;          ///< Current delta value
  bool initialised; ///< Whether the first value has been processed

  explicit loss(size_t pos = 0) noexcept : base{pos}, last{}, delta{}, initialised{false} {}

  void init(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    initialised = false;
    step(T{}, in);
  }

  void step(T, U const *const *in) noexcept override {
    assert(in && in[0] && "NULL input data.");
    if (!initialised) {
      if constexpr (Init == diff_init::zero) {
        delta = 0.; // Initialise with zero
      } else if constexpr (Init == diff_init::epsilon) {
        delta = feps<U>; // Initialise with a small value
      } else {
        delta = fnan<U>; // Initialise with NaN
      }
      initialised = true;
    } else {
      if (in[0][pos] > last) {
        delta = 0; // No loss if current value is greater than last
      } else {
        delta = last - in[0][pos];
      }
    }
    last = in[0][pos];
  }

  void value(U *out) noexcept override {
    assert(out && "NULL output data.");
    out[pos] = delta;
  }
};
} // namespace opflow::op
