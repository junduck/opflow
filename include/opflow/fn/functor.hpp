#pragma once

#include <tuple>

#include "opflow/def.hpp"
#include "opflow/detail/callable_trait.hpp"
#include "opflow/fn_base.hpp"

namespace opflow::fn {
template <typename T, typename Fn>
class functor : public fn_base<T> {
public:
  using data_type = T;

  template <typename... Args>
  functor(Args &&...args) : fn(std::forward<Args>(args)...) {}

  void on_data(data_type const *in, data_type *out) noexcept override {
    call_impl(in, out, std::make_index_sequence<detail::callable_trait_t<Fn>::arity>{});
  }

  size_t num_inputs() const noexcept override { return detail::callable_trait_t<Fn>::arity; }
  size_t num_outputs() const noexcept override {
    using trait = detail::callable_trait_t<Fn>;
    if constexpr (detail::is_tuple_v<typename trait::return_type>) {
      return std::tuple_size_v<typename trait::return_type>;
    } else {
      return 1;
    }
  }

  OPFLOW_CLONEABLE(functor)

private:
  OPFLOW_NO_UNIQUE_ADDRESS Fn fn;

  template <size_t... Is>
  void call_impl(data_type const *in, data_type *out, std::index_sequence<Is...>) noexcept {
    data_type *OPFLOW_RESTRICT cast = out;
    write_out_impl(cast, fn(in[Is]...));
  }

  void write_out_impl(data_type *OPFLOW_RESTRICT out, data_type v) noexcept { out[0] = v; }

  template <typename... Ts>
  void write_out_impl(data_type *OPFLOW_RESTRICT out, std::tuple<Ts...> const &v) noexcept {
    write_out_tup_impl(out, v, std::index_sequence_for<Ts...>{});
  }

  template <size_t... Is, typename Tup>
  void write_out_tup_impl(data_type *OPFLOW_RESTRICT out, Tup const &v, std::index_sequence<Is...>) noexcept {
    ((out[Is] = std::get<Is>(v)), ...);
  }
};
} // namespace opflow::fn
