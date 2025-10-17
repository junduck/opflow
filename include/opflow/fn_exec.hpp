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
        history(dag.record_size, ngrp, alloc), param_history(dag.param_size, ngrp, alloc),
        // tmp
        tmp_args(0, ngrp, alloc) {
    size_t tmp_size_required = 0;
    for (size_t i = 0; i < dag.size(); ++i) {
      tmp_size_required = std::max(tmp_size_required, dag.input_offset.size(i));
    }
    for (size_t i = 0; i < dag.param_node.size(); ++i) {
      tmp_size_required = std::max(tmp_size_required, dag.param_port.size(i));
    }
    tmp_args.ensure_group_capacity(tmp_size_required);
  }

  void on_data(data_type const *in,            //
               data_type *OPFLOW_RESTRICT out, //
               size_t igrp) noexcept {
    auto record = history[igrp];
    auto nodes = dag[igrp];

    // call root node
    nodes[0]->on_data(in, out_ptr(record, 0));

    for (size_t i = 1; i < nodes.size(); ++i) {
      // call node
      nodes[i]->on_data(in_ptr(record, i, igrp), out_ptr(record, i));
    }

    size_t i = 0;
    for (auto idx : dag.output_offset) {
      out[i++] = record[idx];
    }
  }

  void on_param(data_type const *in, size_t igrp) noexcept {
    auto record = param_history[igrp];
    auto nodes = dag[igrp];

    // call param root node
    dag.param(igrp)->on_data(in, record.data());

    auto tmp = tmp_args[igrp];
    for (auto node_id : dag.param_node) {
      auto offsets = dag.param_port[node_id];
      for (size_t i = 0; i < dag.param_port.size(node_id); ++i) {
        tmp[i] = record[offsets[i]];
      }
      nodes[node_id]->on_param(tmp.data());
    }
  }

  size_t num_inputs() const noexcept { return dag[0][0]->num_inputs(); }
  size_t num_outputs() const noexcept { return dag.output_offset.size(); }
  size_t num_groups() const noexcept { return ngrp; }

private:
  data_type *out_ptr(auto record, size_t node_id) noexcept { return record.data() + dag.record_offset[node_id]; }

  data_type const *in_ptr(auto record, size_t node_id, size_t grp_id) noexcept {
    auto args = tmp_args[grp_id];
    auto offsets = dag.input_offset[node_id];
    assert(args.size() >= offsets.size() && "[BUG] Argument size mismatch");
    for (size_t i = 0; i < offsets.size(); ++i) {
      args[i] = record[offsets[i]];
    }
    return args.data();
  }

  size_t const ngrp;
  detail::graph_store<fn_type, Alloc> const dag;
  detail::vector_store<data_type, Alloc> history;
  detail::vector_store<data_type, Alloc> param_history;
  detail::vector_store<data_type, Alloc> tmp_args;
};
} // namespace opflow
