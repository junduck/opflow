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
#define OPFLOW_CLONEABLE(Type)                                                                                         \
  Type *clone_at(void *mem_) const noexcept override { return new (mem_) Type(*this); }                                \
  size_t clone_size() const noexcept override { return sizeof(Type); }                                                 \
  size_t clone_align() const noexcept override { return alignof(Type); }
#endif
