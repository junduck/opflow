#pragma once

#include <tuple>

#include "opflow/def.hpp"
#include "opflow/fn_base.hpp"

#include "opflow/detail/callable_trait.hpp"

namespace opflow::fn {
template <typename T, typename Fn>
class functor : public fn_base<T> {
  using fn_trait = detail::callable_trait_t<Fn>;
  constexpr static size_t arity = fn_trait::arity;
  constexpr static size_t return_size = fn_trait::return_size;

public:
  using data_type = T;

  template <typename... Args>
  functor(Args &&...args) : fn(std::forward<Args>(args)...) {}

  void on_data(data_type const *in, data_type *out) noexcept override {
    on_data_impl(in, out, std::make_index_sequence<detail::callable_trait_t<Fn>::arity>{});
  }

  OPFLOW_INOUT(arity, return_size)
  OPFLOW_CLONEABLE(functor)

private:
  OPFLOW_NO_UNIQUE_ADDRESS Fn fn;

  template <size_t... Is>
  void on_data_impl(data_type const *in, data_type *out, std::index_sequence<Is...>) noexcept {
    data_type *OPFLOW_RESTRICT cast = out;
    write_out_impl(cast, fn(in[Is]...));
  }

  void write_out_impl(data_type *OPFLOW_RESTRICT out, data_type v) noexcept { out[0] = v; }

  template <typename... Ts>
  void write_out_impl(data_type *OPFLOW_RESTRICT out, std::tuple<Ts...> const &v) noexcept {
    std::apply(
        [out](auto... vals) {
          size_t i = 0;
          ((out[i++] = vals), ...);
        },
        v);
  }
};
} // namespace opflow::fn
