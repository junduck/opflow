#pragma once

#include <cstddef>

namespace opflow {
struct cloneable {
  virtual cloneable *clone_at(void *mem) const noexcept = 0;
  virtual size_t clone_size() const noexcept = 0;
  virtual size_t clone_align() const noexcept = 0;

  virtual ~cloneable() = default;
};
} // namespace opflow
