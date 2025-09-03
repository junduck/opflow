#pragma once

#include "common.hpp"
#include "def.hpp"
#include "graph_agg.hpp"
#include "window_base.hpp"

#include "detail/agg_store.hpp"
#include "detail/aligned_allocator.hpp"
#include "detail/column_store.hpp"
#include "detail/vector_store.hpp"

namespace opflow {

/**
 * @brief Multi-group aggregation executor
 *
 * This class supports executing aggregations over multiple independent groups.
 * Each group maintains its own data buffer and window state, allowing for
 * independent processing of different data streams or partitions.
 *
 * Features:
 * - Multiple independent groups (each with separate data buffers)
 * - Per-group window management and emission
 * - Shared aggregation graph definition across all groups
 * - Efficient memory layout using arena allocation
 * - Support for custom allocators
 *
 * Usage:
 * 1. Create a graph_agg defining the aggregation pipeline
 * 2. Instantiate agg_exec with the graph and number of groups
 * 3. Feed data using on_data() with group index
 * 4. Extract results using value() with group index
 *
 * @tparam T The DAG node type (must satisfy dag_node concept)
 * @tparam Alloc Allocator type for memory management
 */
template <dag_node T, typename Alloc = std::allocator<T>>
class agg_exec {
public:
  using data_type = typename T::data_type;

  agg_exec(graph_agg<T> const &graph, size_t n_input, size_t n_groups, size_t pre_alloc_rows = 256,
           Alloc alloc = Alloc{})
      : n_grp(n_groups), aggr(graph, n_groups, alloc), history(aggr.record_size, n_grp, alloc), dataframes(),
        curr_args(0, n_groups, alloc), win_args(0, n_groups, alloc) {
    // Initialize data frames for each group
    dataframes.reserve(n_grp);
    for (size_t igrp = 0; igrp < n_grp; ++igrp) {
      dataframes.emplace_back(n_input, pre_alloc_rows);
    }

    size_t max_input = 0;
    for (size_t i = 0; i < aggr.size(); ++i) {
      max_input = std::max(max_input, aggr.input_column[i].size());
    }
    curr_args.ensure_group_capacity(max_input);

    win_args.ensure_group_capacity(aggr.win_column.size());
  }

  // Ingest a new row. Returns the timestamp of the emitted window, if any.
  std::optional<data_type> on_data(data_type timestamp, data_type const *in, size_t igrp) {
    assert(igrp < n_grp && "[BUG] Group index out of range.");

    dataframes[igrp].append(in);

    // Check if window should emit
    auto const &win = aggr.window(igrp);
    if (!win->on_data(timestamp, win_in_ptr(in, igrp))) {
      return std::nullopt;
    }

    return run_aggr(win->emit(), igrp);
  }

  void value(data_type *OPFLOW_RESTRICT out, size_t igrp) const noexcept {
    assert(igrp < n_grp && "[BUG] Group index out of range.");

    auto record = history[igrp];
    for (size_t i = 0; i < aggr.record_size; ++i) {
      out[i] = record[i];
    }
  }

  // Force emission. Returns the timestamp of the emitted window, if any.
  std::optional<data_type> flush(size_t igrp) {
    assert(igrp < n_grp && "[BUG] Group index out of range.");

    auto &win = aggr.window(igrp);
    if (!win->flush()) {
      return std::nullopt;
    }

    return run_aggr(win->emit(), igrp);
  }

  size_t num_inputs() const noexcept {
    auto const &df = dataframes[0];
    return df.ncol();
  }

  size_t num_outputs() const noexcept { return aggr.record_size; }

  size_t num_groups() const noexcept { return n_grp; }

private:
  using spec_type = typename window_base<data_type>::spec_type;

  void append_row(data_type const *in, size_t igrp) { dataframes[igrp].append(in); }

  data_type run_aggr(spec_type const &spec, size_t igrp) {
    auto nodes = aggr[igrp];

    for (size_t i = 0; i < nodes.size(); ++i) {
      nodes[i]->on_data(spec.size, in_ptr(i, spec.offset, igrp), out_ptr(i, igrp));
    }

    // evict data
    dataframes[igrp].evict(spec.evict);

    return spec.timestamp;
  }

  data_type *out_ptr(size_t node_id, size_t igrp) noexcept {
    auto record = history[igrp];
    return record.data() + aggr.record_offset[node_id];
  }

  data_type const *const *in_ptr(size_t node_id, size_t offset, size_t igrp) noexcept {
    auto args = curr_args[igrp];
    auto cols = aggr.input_column[node_id];
    auto const &df = dataframes[igrp];

    for (size_t i = 0; i < cols.size(); ++i) {
      args[i] = df[cols[i]].data() + offset;
    }
    return args.data();
  }

  data_type const *win_in_ptr(data_type const *in, size_t igrp) noexcept {
    auto args = win_args[igrp];
    for (size_t i = 0; i < aggr.win_column.size(); ++i) {
      args[i] = in[aggr.win_column[i]];
    }
    return args.data();
  }

  size_t n_grp;
  detail::agg_store<T, Alloc> aggr;

  detail::vector_store<data_type, Alloc> history;

  // TODO: currently df is using cacheline_aligned_alloc instead of rebind_alloc to avoid false sharing
  using df_type = detail::aligned_type<detail::column_store<data_type, detail::cacheline_aligned_alloc<data_type>>,
                                       detail::cacheline_size>;
  std::vector<df_type, detail::rebind_alloc<Alloc, df_type>> dataframes;

  detail::vector_store<data_type const *, Alloc> curr_args;
  detail::vector_store<data_type, Alloc> win_args; ///< Reused for window function arguments
};

} // namespace opflow
