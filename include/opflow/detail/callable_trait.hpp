#pragma once

#include <tuple>

namespace opflow::detail {
namespace impl {

template <typename... Args>
struct first_arg;

template <typename Arg0, typename... Args>
struct first_arg<Arg0, Args...> {
  using type = Arg0;
};

template <>
struct first_arg<> {
  using type = void;
};

template <typename... Args>
using first_arg_t = typename first_arg<Args...>::type;

template <typename Ret, typename... Args>
struct callable_trait_impl {
  using return_type = Ret;
  using data_type = first_arg_t<Args...>;

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

template <typename Fn>
concept unary_functor =
    (callable_trait_t<Fn>::arity == 1) &&
    (std::same_as<typename callable_trait_t<Fn>::return_type, typename callable_trait_t<Fn>::data_type>);

template <typename Fn>
concept binary_functor =
    (callable_trait_t<Fn>::arity == 2) &&
    (std::same_as<typename callable_trait_t<Fn>::return_type, typename callable_trait_t<Fn>::data_type>);
} // namespace opflow::detail
