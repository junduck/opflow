#pragma once

#include <tuple>

namespace opflow::detail {
namespace impl {
template <typename Ret, typename... Args>
struct callable_trait_impl {
  using return_type = Ret;

  constexpr static size_t arity = sizeof...(Args);
};

template <typename Ret, typename... Args>
struct callable_trait_impl<Ret (*)(Args...)> : callable_trait_impl<Ret, Args...> {
  using signature = Ret (*)(Args...);
};

template <typename Ret, typename Cls, typename... Args>
struct callable_trait_impl<Ret (Cls::*)(Args...)> : callable_trait_impl<Ret, Args...> {
  using signature = Ret (Cls::*)(Args...);
};

template <typename Ret, typename Cls, typename... Args>
struct callable_trait_impl<Ret (Cls::*)(Args...) const> : callable_trait_impl<Ret, Args...> {
  using signature = Ret (Cls::*)(Args...) const;
};
} // namespace impl

template <typename T>
concept functor = requires() { &T::operator(); };

template <typename T>
struct callable_trait {
  using type = impl::callable_trait_impl<std::decay_t<T>>;
};

template <functor T>
struct callable_trait<T> {
  using type = impl::callable_trait_impl<decltype(&T::operator())>;
};

template <typename T>
using callable_trait_t = typename callable_trait<T>::type;

template <typename T>
struct is_tuple : std::false_type {};

template <typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};

template <typename T>
constexpr inline bool is_tuple_v = is_tuple<T>::value;

template <typename T>
concept can_use_ebo = !std::is_final_v<T> && std::is_empty_v<T>;

} // namespace opflow::detail
