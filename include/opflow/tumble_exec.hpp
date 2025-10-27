#pragma once

#include "def.hpp"
#include "fn_base.hpp"
#include "tumble_base.hpp"

#include "detail/graph_store.hpp"
#include "detail/vector_store.hpp"

namespace opflow {
template <typename T, typename Alloc = std::allocator<T>>
class tumble_exec {
public:
  using data_type = T;
  using fn_type = fn_base<data_type>;
  using root_type = fn_root<data_type>;
  using tumble_type = tumble_base<data_type>;

  template <typename G>
  tumble_exec(G const &g, size_t num_groups, Alloc const &alloc = Alloc{})
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

  std::optional<data_type> on_data(data_type timestamp,            //
                                   data_type const *in,            //
                                   data_type *OPFLOW_RESTRICT out, //
                                   size_t igrp) noexcept {
    auto record = history[igrp];
    auto nodes = dag[igrp];
    auto const &win = dag.window(igrp);

    // call root node
    nodes[0]->on_data(in, out_ptr(record, 0));

    bool should_emit = win->on_data(timestamp, in_ptr(record, 0, igrp));
    if (!should_emit) {
      for (size_t i = 1; i < nodes.size(); ++i) {
        // call node
        nodes[i]->on_data(in_ptr(record, i, igrp), out_ptr(record, i));
      }
      return std::nullopt;
    }

    auto spec = win->emit();
    if (spec.include) {
      // update -> flush -> reset
      for (size_t i = 1; i < nodes.size(); ++i) {
        nodes[i]->on_data(in_ptr(record, i, igrp), out_ptr(record, i));
        nodes[i]->reset(); // equivalent to placing reset after flush
      }
      flush(out, igrp);
    } else {
      // flush -> reset -> update
      flush(out, igrp);
      for (size_t i = 1; i < nodes.size(); ++i) {
        nodes[i]->reset();
        nodes[i]->on_data(in_ptr(record, i, igrp), out_ptr(record, i));
      }
    }

    return spec.timestamp;
  }

  void op_param(data_type const *in, size_t igrp) noexcept {
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

  void flush(data_type *OPFLOW_RESTRICT out, size_t igrp) noexcept {
    auto record = history[igrp];
    size_t i = 0;
    for (auto idx : dag.output_offset) {
      out[i++] = record[idx];
    }
  }

  size_t num_inputs() const noexcept { return dag[0][0]->num_inputs(); }
  size_t num_outputs() const noexcept { return dag.output_offset.size(); }
  size_t num_groups() const noexcept { return ngrp; }

private:
  data_type *out_ptr(auto record, size_t node_id) noexcept { return record.data() + dag.record_offset[node_id]; }

  data_type const *in_ptr(auto record, size_t node_id, size_t grp_id) noexcept {
    auto tmp = tmp_args[grp_id];
    auto offsets = dag.input_offset[node_id];
    for (size_t i = 0; i < offsets.size(); ++i) {
      tmp[i] = record[offsets[i]];
    }
    return tmp.data();
  }

  size_t const ngrp;
  detail::graph_store<fn_type, tumble_type, Alloc> const dag;
  detail::vector_store<data_type, Alloc> history;
  detail::vector_store<data_type, Alloc> param_history;
  detail::vector_store<data_type, Alloc> tmp_args;
};
} // namespace opflow
