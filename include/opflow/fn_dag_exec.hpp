#pragma once

#include <memory>

#include "fn_base.hpp"

#include "detail/dag_store.hpp"
#include "detail/vector_store.hpp"

namespace opflow {
template <typename T, typename Alloc = std::allocator<T>>
class fn_dag_exec {
public:
  using data_type = T;
  using fn_type = fn_base<data_type>;
  using graph_node_type = std::shared_ptr<fn_type>;

  fn_dag_exec(graph_node<fn_type> const &g, size_t num_groups, Alloc const &alloc = Alloc{})
      : // DAG
        ngrp(num_groups), dag(g, ngrp),
        // data
        history(0, ngrp, alloc),
        // tmp
        curr_args(0, ngrp, alloc) {
    validate();
    init_data();
  }

  void on_data(data_type const *in, size_t igrp) noexcept {
    auto record = history[igrp];

    auto nodes = dag[igrp];
    nodes[0]->on_data(in, out_ptr(record, 0));

    commit_input_buffer(igrp);
  }

  void value(data_type *OPFLOW_RESTRICT out, size_t igrp) const noexcept {
    auto record = history[igrp];
    size_t i = 0;
    for (auto [offset, size] : dag.output_offset) {
      for (size_t port = 0; port < size; ++port) {
        out[i++] = record[offset + port];
      }
    }
  }

  data_type *input_buffer(size_t igrp) noexcept {
    auto record = history[igrp];
    return record.data();
  }

  void commit_input_buffer(size_t igrp) noexcept {
    auto record = history[igrp];
    auto nodes = dag[igrp];

    for (size_t i = 1; i < nodes.size(); ++i) {
      // call node
      nodes[i]->on_data(in_ptr(record, i, igrp), out_ptr(record, i));
    }
  }

  size_t num_inputs() const noexcept {
    auto nodes = dag[0];
    return nodes[0]->num_inputs();
  }

  size_t num_outputs() const noexcept {
    size_t total = 0;
    for (auto [_, size] : dag.output_offset) {
      total += size;
    }
    return total;
  }

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

  void validate() const {
    auto nodes = dag[0];
    // validate root
    if (!dynamic_cast<fn_root<data_type> *>(nodes[0].get())) {
      throw std::runtime_error("Wrong root node type in graph.");
    }
    for (size_t i = 1; i < dag.size(); ++i) {
      if (dag.input_offset[i].empty()) {
        throw std::runtime_error("Multiple root nodes detected in graph.");
      }
    }
  }

  void init_data() {
    auto nodes = dag[0];

    size_t max_inputs = 0;
    for (size_t i = 0; i < nodes.size(); ++i) {
      max_inputs = std::max(max_inputs, nodes[i]->num_inputs());
    }
    curr_args.ensure_group_capacity(max_inputs);

    history.ensure_group_capacity(dag.record_size);
  }

  size_t ngrp;                                    ///< Number of groups
  detail::dag_store<fn_type, Alloc> dag;          ///< DAG to execute
  detail::vector_store<data_type, Alloc> history; ///< Memory buffer for each node

  detail::vector_store<data_type, Alloc> curr_args; ///< Reused for current node arguments
};
} // namespace opflow
