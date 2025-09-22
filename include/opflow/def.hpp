#pragma once

#include <new> // IWYU pragma: keep

#ifndef OPFLOW_RESTRICT
#if defined(_MSC_VER)
#define OPFLOW_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#define OPFLOW_RESTRICT __restrict__
#else
#define OPFLOW_RESTRICT
#endif
#endif

#ifndef OPFLOW_CLONEABLE
#ifndef OPFLOW_CLONEABLE
#define OPFLOW_CLONEABLE(Type__)                                                                                       \
  Type__ *clone_at(void *mem__) const noexcept override { return new (mem__) Type__(*this); }                          \
  size_t clone_size() const noexcept override { return sizeof(Type__); }                                               \
  size_t clone_align() const noexcept override { return alignof(Type__); }
#endif
#endif

#ifndef OPFLOW_INOUT
#define OPFLOW_INOUT(InVal__, OutVal__)                                                                                \
  size_t num_inputs() const noexcept override { return InVal__; }                                                      \
  size_t num_outputs() const noexcept override { return OutVal__; }
#endif

#ifndef OPFLOW_NO_UNIQUE_ADDRESS
#if defined(_MSC_VER)
// [[no_unique_address]] is ignored by MSVC even in C++20 mode; instead, [[msvc::no_unique_address]] is provided.
// Ref: https://en.cppreference.com/w/cpp/language/attributes/no_unique_address
#define OPFLOW_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define OPFLOW_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif
#endif
