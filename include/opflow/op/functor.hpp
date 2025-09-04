#pragma once

#include <array>
#include <tuple>

#include "opflow/def.hpp"
#include "opflow/op_base.hpp"

#include "opflow/detail/callable_trait.hpp"

namespace opflow::op {
template <typename T, typename Fn>
class functor : public op_base<T> {
  using fn_trait = detail::callable_trait_t<Fn>;
  constexpr static size_t arity = fn_trait::arity;
  constexpr static size_t return_size = fn_trait::return_size;

public:
  using data_type = T;

  template <typename... Args>
  functor(Args &&...args) : fn(std::forward<Args>(args)...) {}

  void on_data(data_type const *in) noexcept override { on_data_impl(in, std::make_index_sequence<arity>{}); }
  void value(data_type *out) const noexcept override {
    data_type *OPFLOW_RESTRICT cast = out;
    for (size_t i = 0; i < return_size; ++i) {
      cast[i] = val[i];
    }
  }

  // TODO: we expect stateless functor here
  virtual void reset() noexcept override {}

  OPFLOW_INOUT(arity, return_size)
  OPFLOW_CLONEABLE(functor)

private:
  OPFLOW_NO_UNIQUE_ADDRESS Fn fn;
  std::array<data_type, return_size> val;

  template <size_t... Is>
  void on_data_impl(data_type const *in, std::index_sequence<Is...>) noexcept {
    write_val_impl(fn(in[Is]...));
  }

  void write_val_impl(data_type v) noexcept { val[0] = v; }

  template <typename... Ts>
  void write_val_impl(std::tuple<Ts...> const &v) noexcept {
    std::apply(
        [this](auto... vals) {
          size_t i = 0;
          ((val[i++] = vals), ...);
        },
        v);
  }
};
} // namespace opflow::op
