#pragma once

#include <memory>

#include "def.hpp"
#include "fn_base.hpp"

#include "detail/graph_store.hpp"
#include "detail/vector_store.hpp"

namespace opflow {
template <typename T, typename Alloc = std::allocator<T>>
class fn_exec {
public:
  using data_type = T;
  using fn_type = fn_base<data_type>;
  using root_type = fn_root<data_type>;

  template <typename G>
  fn_exec(G const &g, size_t num_groups, Alloc const &alloc = Alloc{})
      : // DAG
        ngrp(num_groups), dag(g, ngrp, alloc),
        // data
        history(dag.record_size, ngrp, alloc),
        // tmp
        curr_args(0, ngrp, alloc) {
    size_t max_inputs = 0;
    for (size_t i = 0; i < dag.size(); ++i) {
      max_inputs = std::max(max_inputs, dag.input_offset.size(i));
    }
    curr_args.ensure_group_capacity(max_inputs);
  }

  void on_data(data_type const *in,            //
               data_type *OPFLOW_RESTRICT out, //
               size_t igrp) noexcept {
    auto record = history[igrp];
    auto nodes = dag[igrp];

    root_type *root = static_cast<root_type *>(nodes[0].get());
    root->on_data(in, out_ptr(record, 0));

    for (size_t i = 1; i < nodes.size(); ++i) {
      // call node
      nodes[i]->on_data(in_ptr(record, i, igrp), out_ptr(record, i));
    }

    size_t i = 0;
    for (auto idx : dag.output_offset) {
      out[i++] = record[idx];
    }
  }

  size_t num_inputs() const noexcept {
    auto nodes = dag[0];
    root_type *root = static_cast<root_type *>(nodes[0].get());
    return root->input_size;
  }

  size_t num_outputs() const noexcept { return dag.output_offset.size(); }

  size_t num_groups() const noexcept { return ngrp; }

private:
  data_type *out_ptr(auto record, size_t node_id) noexcept { return record.data() + dag.record_offset[node_id]; }

  data_type const *in_ptr(auto record, size_t node_id, size_t grp_id) noexcept {
    auto args = curr_args[grp_id];
    auto offsets = dag.input_offset[node_id];
    assert(args.size() >= offsets.size() && "[BUG] Argument size mismatch");
    for (size_t i = 0; i < offsets.size(); ++i) {
      args[i] = record[offsets[i]];
    }
    return args.data();
  }

  size_t const ngrp;                              ///< Number of groups
  detail::graph_store<fn_type, Alloc> const dag;  ///< DAG to execute
  detail::vector_store<data_type, Alloc> history; ///< Memory buffer for each node

  detail::vector_store<data_type, Alloc> curr_args; ///< Reused for current node arguments
};
} // namespace opflow
