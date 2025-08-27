#pragma once

#include <memory>

#include "detail/flat_multivect.hpp"
#include "detail/vector_store.hpp"
#include "fn_base.hpp"
#include "graph_topo.hpp"

namespace opflow {
template <typename T, typename Alloc = std::allocator<T>>
class fn_dag_exec {
public:
  using data_type = T;
  using fn_type = fn_base<data_type>;
  using graph_node_type = std::shared_ptr<fn_type>;

  fn_dag_exec(graph_node<fn_type> const &g, size_t num_groups, Alloc const &alloc = Alloc{})
      : // DAG
        ngrp(num_groups), graph(g, ngrp),
        // data
        history(0, ngrp, alloc), record_offset(alloc), args_offset(alloc),
        // tmp
        curr_args(0, ngrp, alloc) {
    validate();
    init_data();
  }

  void on_data(data_type const *in, size_t igrp) {
    auto record = history[igrp];

    auto nodes = graph[igrp];
    nodes[0]->on_data(in, out_ptr(record, 0));

    commit_input_buffer(igrp);
  }

  void value(data_type *OPFLOW_RESTRICT out, size_t igrp) const noexcept {
    auto record = history[igrp];
    size_t i = 0;
    for (auto [id, size] : graph.nodes_out()) {
      for (size_t port = 0; port < size; ++port) {
        out[i++] = record[record_offset[id] + port];
      }
    }
  }

  data_type *input_buffer(size_t igrp) noexcept {
    auto record = history[igrp];
    return record.data();
  }

  void commit_input_buffer(size_t igrp) noexcept {
    auto record = history[igrp];
    auto nodes = graph[igrp];

    for (size_t i = 1; i < nodes.size(); ++i) {
      // call node
      nodes[i]->on_data(in_ptr(record, i, igrp), out_ptr(record, i));
    }
  }

  size_t num_inputs() const noexcept {
    auto nodes = graph[0];
    return nodes[0]->num_inputs();
  }

  size_t num_outputs() const noexcept {
    size_t total = 0;
    for (auto const &out : graph.nodes_out()) {
      total += out.size;
    }
    return total;
  }

  size_t num_groups() const noexcept { return ngrp; }

private:
  data_type *out_ptr(auto record, size_t node_id) noexcept { return record.data() + record_offset[node_id]; }

  data_type const *in_ptr(auto record, size_t node_id, size_t grp_id) noexcept {
    auto args = curr_args[grp_id];
    auto offsets = args_offset[node_id];
    assert(args.size() >= offsets.size() && "[BUG] Argument size mismatch");
    for (size_t i = 0; i < offsets.size(); ++i) {
      args[i] = record[offsets[i]];
    }
    return args.data();
  }

  void validate() const {
    auto nodes = graph[0];
    // validate root
    if (!dynamic_cast<fn_root<data_type> *>(nodes[0].get())) {
      throw std::runtime_error("Wrong root node type in graph.");
    }
    for (size_t i = 1; i < graph.size(); ++i) {
      if (graph.pred_of(i).empty()) {
        throw std::runtime_error("Multiple root nodes detected in graph.");
      }
    }
    // validate connections
    for (size_t i = 1; i < nodes.size(); ++i) {
      for (auto [pred, port] : graph.args_of(i)) {
        if (port >= nodes[pred]->num_outputs()) {
          throw std::runtime_error("Incompatible node connections in graph.");
        }
      }
    }
  }

  void init_data() {
    auto nodes = graph[0];

    size_t max_inputs = 0;
    size_t total_size = 0;
    record_offset.reserve(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i) {
      record_offset.push_back(total_size);
      total_size += nodes[i]->num_outputs();
      max_inputs = std::max(max_inputs, nodes[i]->num_inputs());
    }
    history.ensure_group_capacity(total_size);
    curr_args.ensure_group_capacity(max_inputs);

    std::vector<size_t> args{};
    args_offset.reserve(graph.size(), graph.num_edges());
    for (size_t i = 0; i < graph.size(); ++i) {
      args.clear();
      for (auto [pred, port] : graph.args_of(i)) {
        args.push_back(record_offset[pred] + port);
      }
      args_offset.push_back(args);
    }
  }

  // allocator types
  using data_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<data_type>;
  using size_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<size_t>;

  size_t ngrp;               ///< Number of groups
  graph_topo<fn_type> graph; ///< DAG to execute, uses its own alloc

  detail::vector_store<data_type, data_alloc> history;    ///< Memory buffer for each node
  std::vector<size_t, size_alloc> record_offset;          ///< Memory offsets for each node
  detail::flat_multivect<size_t, size_alloc> args_offset; ///< Argument offsets for each node

  detail::vector_store<data_type, data_alloc> curr_args; ///< Reused for current node arguments
};
} // namespace opflow
