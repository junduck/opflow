#pragma once

#include <memory>
#include <numeric>

#include "common.hpp"
#include "def.hpp"
#include "detail/history_ringbuf.hpp"
#include "graph.hpp"
#include "graph_topo.hpp"
#include "op/root.hpp"
#include "op_base.hpp"

namespace opflow {
template <typename Data>
class op_dag_exec {
public:
  using data_type = Data;
  using op_type = op_base<data_type>;
  using node_type = std::shared_ptr<op_type>;

  template <range_of<node_type> R>
  explicit op_dag_exec(graph<node_type> const &g, R &&outputs)
      : nodes(g),                                   // topo
        all_cumulative(), win_desc(), step_count(), // window state
        data_offset(), data_size(), history(),      // data
        output_nodes(), output_sizes(),             // output
        curr_args()                                 // temp
  {
    validate_nodes();
    validate_nodes_compat();
    init_win_desc();
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

  void on_data(data_type timestamp, data_type const *input_data) {
    auto [_, mem] = history.push(timestamp);

    // Handle root input node
    nodes[0]->on_data(input_data);
    nodes[0]->value(out_ptr(mem, 0));
    // Handle subsequent nodes
    commit_input_buffer();
  }

  void value(data_type *OPFLOW_RESTRICT out) const noexcept {
    auto [_, mem] = history.back();
    size_t i = 0;
    for (auto id : output_nodes) {
      for (size_t port = 0; port < output_sizes[id]; ++port) {
        out[i++] = mem[data_offset[id] + port];
      }
    }
  }
  size_t num_inputs() const noexcept { return nodes[0]->num_inputs(); }
  size_t num_outputs() const noexcept { return std::accumulate(output_sizes.begin(), output_sizes.end(), size_t(0)); }

  // LEAKY interface to avoid extra copy on_data at input node

  // upstream:
  // prev.value(curr.input_buffer(timestamp));
  // curr.commit_input_buffer();
  // curr.value(next.input_buffer(timestamp));
  // ...

  data_type *input_buffer(data_type timestamp) noexcept {
    auto [_, mem] = history.push(timestamp);
    return mem.data();
  }

  void commit_input_buffer() {
    auto [timestamp, mem] = history.back();

    for (size_t i = 1; i < nodes.size(); ++i) {
      // call node
      nodes[i]->on_data(in_ptr(mem, i));
      // handle eviction for non-cumulative nodes
      if (!win_desc[i].cumulative) {
        // update step_count
        ++step_count[i];
        // remove expired data
        switch (win_desc[i].type) {
        case win_type::event:
          evict_event(timestamp, i);
          break;
        case win_type::time:
          evict_time(timestamp, i);
          break;
        }
      }
      // store output data
      nodes[i]->value(out_ptr(mem, i));
    }

    cleanup_history();
  }

private:
  data_type *out_ptr(auto base, size_t node_id) noexcept { return base.data() + data_offset[node_id]; }

  data_type const *in_ptr(auto base, size_t node_id) const {
    curr_args.clear();
    for (auto [pred, port] : nodes.args_of(node_id)) {
      auto val = base[data_offset[pred] + port];
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
    if (!dynamic_cast<op::graph_root<data_type> *>(nodes[0].get())) {
      throw node_error("Root node must be of type fn::graph_root", nodes[0]);
    }
    size_t max_port = 0;
    for (size_t i = 1; i < nodes.size(); ++i) {
      for (auto [pred, port] : nodes.args_of(i)) {
        max_port = std::max<size_t>(max_port, port);
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
    history.init(data_size);
  }

  void init_win_desc() {
    win_desc.reserve(nodes.size());
    step_count.resize(nodes.size(), 0);
    size_t n_cumulative = 0;
    for (size_t i = 0; i < nodes.size(); ++i) {
      win_desc_t desc{};
      if (nodes[i]->is_cumulative()) {
        // cumulative op
        desc.cumulative = true;
        step_count[i] = 1;
        ++n_cumulative;
      } else {
        // windowed op
        desc.dynamic = nodes[i]->is_dynamic();
        desc.type = nodes[i]->window_type();
        switch (desc.type) {
        case win_type::event:
          desc.win_event = nodes[i]->window_size(event_window);
          break;
        case win_type::time:
          desc.win_time = nodes[i]->window_size(time_window);
          break;
        }
      }
      win_desc.push_back(desc);
    }
    all_cumulative = n_cumulative == nodes.size();
  }

  void evict_event(data_type timestamp, size_t id) {
    // Pre: op contains data at history idx  [k,  N - 1], where k = N - step_count[id];
    // Post: op contains data at history idx [k', N - 1], where k' = N - win_size
    // Obj: remove data at history idx       [k,  k')

    std::ignore = timestamp;
    assert(history.size() >= step_count[id] && "[BUG] History is smaller than step count for node.");

    auto win_size = win_desc[id].dynamic ? nodes[id]->window_size(event_window) : win_desc[id].win_event;
    if (step_count[id] <= win_size)
      return; // No data to remove

    auto k = history.size() - step_count[id];
    auto kp = history.size() - win_size;

    for (size_t i = k; i < kp; ++i) {
      auto [_, mem] = history[i];
      nodes[id]->on_evict(in_ptr(mem, id));
      --step_count[id];
    }
  }

  void evict_time(data_type timestamp, size_t id) {
    // Pre: op contains data at history idx  [k,  N - 1], where k = N - step_count[id]
    // Post: op contains data at history idx [k', N - 1], where k' = argmin(time[i] > win_start)
    // Obj: remove data at history idx       [k,  k')

    assert(history.size() >= step_count[id] && "[BUG] History is smaller than step count for node.");

    auto win_size = win_desc[id].dynamic ? nodes[id]->window_size(time_window) : win_desc[id].win_time;
    auto win_start = timestamp - win_size;
    auto k = history.size() - step_count[id];

    for (size_t i = k; i < history.size(); ++i) {
      auto [time, mem] = history[i];
      if (time > win_start) // k'
        break;              // No more data to remove
      nodes[id]->on_evict(in_ptr(mem, id));
      --step_count[id];
    }
  }

  void cleanup_history() {
    if (all_cumulative) {
      // All nodes are cumulative, we only need to store latest output
      while (history.size() > 1) {
        history.pop();
      }
    } else {
      // Pop entries that no longer need
      size_t max_count = std::ranges::max(step_count);
      while (history.size() > max_count) {
        history.pop();
      }
    }
  }

  struct win_desc_t {
    size_t win_event;
    data_type win_time;
    bool cumulative;
    bool dynamic;
    win_type type;
  };

  graph_topo<node_type> nodes; ///< Topologically sorted DAG

  bool all_cumulative;              ///< True if all nodes are in cumulative mode
  std::vector<win_desc_t> win_desc; ///< Window descriptors for each node
  std::vector<size_t> step_count;   ///< Step count for each node, used in step sliding mode

  std::vector<size_t> data_offset;                       ///< I/O data offsets for each node
  size_t data_size;                                      ///< Total size of I/O data for all nodes
  detail::history_ringbuf<data_type, data_type> history; ///< History buffer for node I/O data

  std::vector<size_t> output_nodes; ///< Output node IDs for each node
  std::vector<size_t> output_sizes; ///< Output sizes for each node

  mutable std::vector<data_type> curr_args; ///< Reused for current node arguments
};
} // namespace opflow
