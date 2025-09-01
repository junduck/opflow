#pragma once

#include "opflow/def.hpp"
#include "opflow/op_base.hpp"

#include "detail/accum.hpp"

namespace opflow::op {
template <typename T>
class avg : public win_base<T> {
public:
  using base = win_base<T>;
  using typename base::data_type;

  using base::base;

  void on_data(data_type const *in) noexcept override {
    auto const x = in[0];
    ++n;
    val.add(x, data_type(1) / n);
  }

  void on_evict(data_type const *rm) noexcept override {
    auto const x = rm[0];
    --n;
    val.add(x, data_type(1) / n);
  }

  void value(data_type *out) const noexcept override { out[0] = val; }

  void reset() noexcept override {
    val.reset();
    n = 0;
  }

  OPFLOW_INOUT(1, 1)
  OPFLOW_CLONEABLE(avg)

private:
  detail::smooth<data_type> val; ///< mean value
  size_t n;                      ///< count of values processed
};

static_assert(dag_node<avg<double>>);

template <typename T>
class avg_weighted : public win_base<T> {
public:
  using base = win_base<T>;
  using typename base::data_type;

  using base::base;

  void on_data(data_type const *in) noexcept override {
    auto const x = in[0];
    auto const w = in[1];
    w_sum.add(w);
    val.add(x, w / w_sum);
  }

  void on_evict(data_type const *rm) noexcept override {
    auto const x = rm[0];
    auto const w = rm[1];
    w_sum.sub(w);
    val.sub(x, w / w_sum);
  }

  void value(data_type *out) const noexcept override { out[0] = val; }

  void reset() noexcept override {
    val.reset();
    w_sum.reset();
  }

  OPFLOW_INOUT(2, 1)
  OPFLOW_CLONEABLE(avg_weighted)

private:
  detail::smooth<data_type> val;  ///< weighted mean value
  detail::accum<data_type> w_sum; ///< sum of weights
};

static_assert(dag_node<avg_weighted<double>>);
} // namespace opflow::op
