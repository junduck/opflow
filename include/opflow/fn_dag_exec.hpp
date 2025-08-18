#pragma once

#include <memory>
#include <numeric>

#include "common.hpp"
#include "def.hpp"
#include "fn/root.hpp"
#include "fn_base.hpp"
#include "graph.hpp"
#include "graph_topo.hpp"

namespace opflow {
template <typename T>
class fn_dag_exec {
public:
  using data_type = T;
  using fn_type = fn_base<data_type>;
  using node_type = std::shared_ptr<fn_type>;

  template <range_of<node_type> R>
  fn_dag_exec(graph<node_type> const &g, R &&outputs)
      : nodes(g),                                  // topo
        data_offset(), data_size(), mem(), time(), // data
        output_nodes(), output_sizes(),            // output
        curr_args()                                // temp
  {
    validate_nodes();
    validate_nodes_compat();
    init_data();

    for (auto const &node : outputs) {
      auto id = nodes.id_of(node);
      if (id == nodes.size()) {
        throw node_error("Output node not found in graph", node);
      }
      output_nodes.push_back(id);
      output_sizes.push_back(node->num_outputs());
    }
  }

  void on_data(data_type timestamp, data_type const *in) {
    // Handle root
    time = timestamp;
    nodes[0]->on_data(in, out_ptr(0));
    // Handle subsequent nodes
    commit_input_buffer();
  }

  data_type value(data_type *OPFLOW_RESTRICT out) const noexcept {
    size_t i = 0;
    for (auto id : output_nodes) {
      for (size_t port = 0; port < output_sizes[id]; ++port) {
        out[i++] = mem[data_offset[id] + port];
      }
    }
    return time;
  }
  size_t num_inputs() const noexcept { return nodes[0]->num_inputs(); }
  size_t num_outputs() const noexcept { return std::accumulate(output_sizes.begin(), output_sizes.end(), size_t(0)); }

  // LEAKY interface to avoid extra copy on_data at input node

  // upstream:
  // prev.value(curr.input_buffer());
  // curr.commit_input_buffer(timestamp);
  // curr.value(next.input_buffer());
  // ...

  data_type *input_buffer(data_type timestamp) noexcept {
    time = timestamp;
    return mem.data();
  }

  void commit_input_buffer() {
    // Root input is already written
    for (size_t i = 1; i < nodes.size(); ++i) {
      nodes[i]->on_data(in_ptr(i), out_ptr(i));
    }
  }

private:
  data_type *out_ptr(size_t node_id) noexcept { return mem.data() + data_offset[node_id]; }

  data_type const *in_ptr(size_t node_id) const {
    curr_args.clear();
    for (auto [pred, port] : nodes.args_of(node_id)) {
      auto val = mem[data_offset[pred] + port];
      curr_args.push_back(val);
    }
    return curr_args.data();
  }

  void validate_nodes() const {
    if (nodes.empty()) {
      throw std::runtime_error("Graph is empty.");
    }
    for (size_t i = 0; i < nodes.size(); ++i) {
      if (!nodes[i]) {
        throw std::runtime_error("Graph contains null node.");
      }
    }
    if (nodes.root_ids().size() != 1) {
      throw std::runtime_error("Graph must have exactly one root input.");
    }
  }

  void validate_nodes_compat() const {
    if (!dynamic_cast<fn::graph_root<data_type> *>(nodes[0].get())) {
      throw node_error("Root node must be of type fn::graph_root", nodes[0]);
    }
    size_t max_port = 0;
    for (size_t i = 1; i < nodes.size(); ++i) {
      for (auto [pred, port] : nodes.args_of(i)) {
        max_port = std::max(max_port, port);
        if (port >= nodes[pred]->num_outputs()) {
          throw node_error("Incompatible node connections", nodes[i]);
        }
      }
    }
    curr_args.resize(max_port + 1);
  }

  void init_data() {
    data_offset.reserve(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i) {
      data_offset.push_back(data_size);
      data_size += nodes[i]->num_outputs();
    }
    mem.resize(data_size);
  }

  graph_topo<node_type> nodes; ///< Topologically sorted DAG

  std::vector<size_t> data_offset; ///< Data offsets for each node
  size_t data_size;                ///< Total size
  std::vector<data_type> mem;      ///< Data buffer
  data_type time;                  ///< Timestamp for the current execution

  std::vector<size_t> output_nodes; ///< Output node indices
  std::vector<size_t> output_sizes; ///< Output sizes for each output node

  mutable std::vector<data_type> curr_args; ///< Reused for current node arguments
};
} // namespace opflow
