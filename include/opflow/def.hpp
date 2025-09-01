#pragma once

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
#define OPFLOW_CLONEABLE(Type)                                                                                         \
  Type *clone_at(void *mem_) const noexcept override { return new (mem_) Type(*this); }                                \
  size_t clone_size() const noexcept override { return sizeof(Type); }                                                 \
  size_t clone_align() const noexcept override { return alignof(Type); }
#endif
#endif

#ifndef OPFLOW_INOUT
#define OPFLOW_INOUT(InVal, OutVal)                                                                                    \
  size_t num_inputs() const noexcept override { return InVal; }                                                        \
  size_t num_outputs() const noexcept override { return OutVal; }
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
