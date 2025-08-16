#pragma once

#include <memory>

#include "common.hpp"
#include "detail/history_ringbuf.hpp"
#include "graph.hpp"
#include "graph_topo.hpp"
#include "op_base.hpp"

namespace opflow {
template <typename Data>
class graph_exec {
public:
  using data_type = Data;
  using op_type = op_base<data_type>;
  using node_type = std::shared_ptr<op_type>;

  explicit graph_exec(graph<node_type> const &g)
      : nodes(g),                                   // topo
        all_cumulative(), win_desc(), step_count(), // window state
        data_offset(), data_size(), history(),      // data
        curr_args()                                 // temp
  {
    validate_nodes();
    validate_nodes_compat();
    init_win_desc();
    init_data();
  }

  void on_data(data_type timestamp, data_type const *input_data) {
    auto [_, mem] = history.push(timestamp);

    // Handle root input node
    nodes[0]->on_data(input_data);
    nodes[0]->value(value_ptr(mem, 0));

    for (size_t i = 1; i < nodes.size(); ++i) {

      // call node
      auto const *ptr = args_ptr(mem, i);
      nodes[i]->on_data(ptr);

      if (!win_desc[i].cumulative) {
        // only update step_count for non cumulative nodes
        ++step_count[i];
        // remove expired data if needed
        switch (win_desc[i].domain) {
        case window_domain::event:
          evict_event(timestamp, i);
          break;
        case window_domain::time:
          evict_time(timestamp, i);
          break;
        }
      }

      // store output data
      auto *val_ptr = value_ptr(mem, i);
      nodes[i]->value(val_ptr);
    }

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

private:
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
    for (size_t i = 1; i < nodes.size(); ++i) {
      for (auto [pred, port] : nodes.args_of(i)) {
        if (port >= nodes[pred]->num_outputs()) {
          throw node_error("Incompatible node connections", nodes[i]);
        }
      }
    }
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
        desc.domain = nodes[i]->domain();
        switch (desc.domain) {
        case window_domain::event:
          desc.win_event = nodes[i]->window_size(event_domain);
          break;
        case window_domain::time:
          desc.win_time = nodes[i]->window_size(time_domain);
          break;
        }
      }
      win_desc.push_back(desc);
    }
    all_cumulative = n_cumulative == nodes.size();
  }

  data_type *value_ptr(std::span<data_type> base, size_t node_id) noexcept {
    return base.data() + data_offset[node_id];
  }

  data_type const *args_ptr(std::span<data_type const> base, size_t node_id) noexcept {
    curr_args.clear();
    for (auto [pred, port] : nodes.args_of(node_id)) {
      auto val = base[data_offset[node_id] + port];
      curr_args.push_back(val);
    }
    return curr_args.data();
  }

  void evict_event(data_type timestamp, size_t id) {
    // Pre: op contains data at history idx  [k,  N - 1], where k = N - step_count[id];
    // Post: op contains data at history idx [k', N - 1], where k' = N - win_size
    // Obj: remove data at history idx       [k,  k')

    std::ignore = timestamp;
    assert(history.size() >= step_count[id] && "[BUG] History is smaller than step count for node.");

    auto win_size = win_desc[id].dynamic ? nodes[id]->window_size(event_domain) : win_desc[id].win_event;
    if (step_count[id] <= win_size)
      return; // No data to remove

    auto k = history.size() - step_count[id];
    auto kp = history.size() - win_size;

    for (size_t i = k; i < kp; ++i) {
      auto [_, mem] = history[i];
      auto const *ptr = args_ptr(mem, id);
      nodes[id]->on_evict(ptr);
      --step_count[id];
    }
  }

  void evict_time(data_type timestamp, size_t id) {
    // Pre: op contains data at history idx  [k,  N - 1], where k = N - step_count[id]
    // Post: op contains data at history idx [k', N - 1], where k' = argmin(time[i] > win_start)
    // Obj: remove data at history idx       [k,  k')

    assert(history.size() >= step_count[id] && "[BUG] History is smaller than step count for node.");

    auto win_size = win_desc[id].dynamic ? nodes[id]->window_size(time_domain) : win_desc[id].win_time;
    auto win_start = timestamp - win_size;
    auto k = history.size() - step_count[id];

    for (size_t i = k; i < history.size(); ++i) {
      auto [time, mem] = history[i];
      if (time > win_start) // k'
        break;              // No more data to remove
      auto const *ptr = args_ptr(mem, id);
      nodes[id]->on_evict(ptr);
      --step_count[id];
    }
  }

  struct win_desc_t {
    size_t win_event;
    data_type win_time;
    bool cumulative;
    bool dynamic;
    window_domain domain;
  };

  graph_topo<node_type> nodes; ///< Topologically sorted DAG

  bool all_cumulative;              ///< True if all nodes are in cumulative mode
  std::vector<win_desc_t> win_desc; ///< Window descriptors for each node
  std::vector<size_t> step_count;   ///< Step count for each node, used in step sliding mode

  std::vector<size_t> data_offset;                       ///< I/O data offsets for each node
  size_t data_size;                                      ///< Total size of I/O data for all nodes
  detail::history_ringbuf<data_type, data_type> history; ///< History buffer for node I/O data

  std::vector<data_type> curr_args; ///< Reused for current node arguments
};
} // namespace opflow
