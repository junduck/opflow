#pragma once

#include "common.hpp"
#include "def.hpp"
#include "detail/flat_multivect.hpp"
#include "detail/history_buffer.hpp"
#include "detail/vector_store.hpp"
#include "graph_node.hpp"
#include "graph_topo_fanout.hpp"
#include "opflow/op_base.hpp"

namespace opflow {
/**
 * @brief Multi-group DAG operator executor
 *
 */
template <typename T, typename Alloc = std::allocator<T>>
class op_dag_exec {
public:
  using data_type = T;
  using op_type = op_base<data_type>;
  using graph_node_type = std::shared_ptr<op_type>;

  op_dag_exec(graph_node<op_type> const &g, size_t num_groups, size_t history_size_hint = 256,
              Alloc const &alloc = Alloc{})
      : // DAG
        ngrp(num_groups), graph(g, ngrp),
        // data
        history(alloc), record_offset(alloc), args_offset(alloc),
        // window
        all_cumulative(), win_desc(alloc), step_count(graph.size(), ngrp, alloc),
        // tmp
        curr_args(0, ngrp, alloc) {
    validate();
    std::vector<size_t> size_hints(ngrp, history_size_hint);
    init_data(size_hints);
    init_window();
  }

  template <sized_range_of<size_t> S>
  op_dag_exec(graph_node<op_type> const &g, size_t num_groups, S &&hints_by_grp, Alloc const &alloc = Alloc{})
      : // DAG
        ngrp(num_groups), graph(g, ngrp),
        // data
        history(alloc), record_offset(alloc), args_offset(alloc),
        // window
        all_cumulative(), win_desc(alloc), step_count(graph.size(), ngrp),
        // tmp
        curr_args(0, ngrp, alloc) {
    if (std::ranges::size(hints_by_grp) != ngrp) {
      throw std::runtime_error("History size hints must match number of groups.");
    }
    validate();
    init_data(std::forward<S>(hints_by_grp));
    init_window();
  }

  void on_data(data_type timestamp, data_type const *input_data, size_t igrp) {
    auto [_, record] = history[igrp].push(timestamp);

    auto nodes = graph.nodes_of(igrp);
    nodes[0]->on_data(input_data);
    nodes[0]->value(out_ptr(record, 0));

    commit_input_buffer(igrp);
  }

  void value(data_type *OPFLOW_RESTRICT out, size_t igrp) const noexcept {
    auto [_, record] = history[igrp].back();
    size_t i = 0;
    for (auto [id, size] : graph.nodes_out()) {
      for (size_t port = 0; port < size; ++port) {
        out[i++] = record[record_offset[id] + port];
      }
    }
  }

  data_type *input_buffer(data_type timestamp, size_t igrp) {
    auto [_, record] = history[igrp].push(timestamp);
    return record.data();
  }

  void commit_input_buffer(size_t igrp) noexcept {
    auto [timestamp, record] = history[igrp].back();
    auto nodes = graph[igrp];

    for (size_t i = 1; i < nodes.size(); ++i) {
      // call node
      nodes[i]->on_data(in_ptr(record, i, igrp));
      // handle eviction for non-cumulative nodes
      if (!win_desc[i].cumulative) {
        // update step_count
        ++step_count[igrp][i];
        // remove expired data
        switch (win_desc[i].type) {
        case win_type::event:
          evict_event(timestamp, i, igrp);
          break;
        case win_type::time:
          evict_time(timestamp, i, igrp);
          break;
        }
      }
      // store output data
      nodes[i]->value(out_ptr(record, i));
    }

    cleanup_history(igrp);
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

  void validate() {
    auto nodes = graph[0];
    // validate root
    if (!dynamic_cast<op_root<data_type> *>(nodes[0].get())) {
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

  template <sized_range_of<size_t> R>
  void init_data(R &&history_size_hints) {
    // we only need to test group 0
    auto nodes = graph[0];

    size_t max_inputs = 0;
    size_t total_size = 0;
    record_offset.reserve(graph.size());
    for (size_t i = 0; i < graph.size(); ++i) {
      record_offset.push_back(total_size);
      total_size += nodes[i]->num_outputs();
      max_inputs = std::max(max_inputs, graph.args_of(i).size());
    }
    history.reserve(ngrp);
    for (size_t hint : history_size_hints) {
      history.emplace_back(total_size, hint, history.get_allocator());
    }
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

  void init_window() {
    // we only need to test group 0
    auto nodes = graph[0];

    win_desc.reserve(graph.size());
    size_t n_cumulative = 0;
    for (size_t i = 0; i < graph.size(); ++i) {
      win_desc_type desc{};
      if (nodes[i]->is_cumulative()) {
        // cumulative op
        desc.cumulative = true;
        for (size_t igrp = 0; igrp < ngrp; ++igrp) {
          step_count[igrp][i] = 1;
        }
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
    all_cumulative = n_cumulative == graph.size();
  }

  void evict_event(data_type timestamp, size_t id, size_t igrp) noexcept {
    // Pre: op contains data at history idx  [k,  N - 1], where k = N - step_count[id];
    // Post: op contains data at history idx [k', N - 1], where k' = N - win_size
    // Obj: remove data at history idx       [k,  k')

    std::ignore = timestamp;
    auto &step_cnt = step_count[igrp][id];
    assert(history[igrp].size() >= step_cnt && "[BUG] History is smaller than step count for node.");

    auto nodes = graph[igrp];
    auto win_size = win_desc[id].dynamic ? nodes[id]->window_size(event_window) : win_desc[id].win_event;
    if (step_cnt <= win_size)
      return; // No data to remove

    auto k = history[igrp].size() - step_cnt;
    auto kp = history[igrp].size() - win_size;

    for (size_t i = k; i < kp; ++i) {
      auto [_, mem] = history[igrp][i];
      nodes[id]->on_evict(in_ptr(mem, id, igrp));
      --step_cnt;
    }
  }

  void evict_time(data_type timestamp, size_t id, size_t igrp) {
    // Pre: op contains data at history idx  [k,  N - 1], where k = N - step_count[id]
    // Post: op contains data at history idx [k', N - 1], where k' = argmin(time[i] > win_start)
    // Obj: remove data at history idx       [k,  k')

    auto &step_cnt = step_count[igrp][id];
    assert(history[igrp].size() >= step_cnt && "[BUG] History is smaller than step count for node.");

    auto nodes = graph[igrp];
    auto win_size = win_desc[id].dynamic ? nodes[id]->window_size(time_window) : win_desc[id].win_time;
    auto win_start = timestamp - win_size;
    auto k = history[igrp].size() - step_cnt;

    for (size_t i = k; i < history[igrp].size(); ++i) {
      auto [time, mem] = history[igrp][i];
      if (time > win_start) // k'
        break;              // No more data to remove
      nodes[id]->on_evict(in_ptr(mem, id, igrp));
      --step_cnt;
    }
  }

  void cleanup_history(size_t igrp) noexcept {
    if (all_cumulative) {
      // All nodes are cumulative, we only need to store latest output
      while (history[igrp].size() > 1) {
        history[igrp].pop();
      }
    } else {
      // Pop entries that no longer need
      auto steps = step_count[igrp];
      size_t max_count = *std::ranges::max_element(steps);
      while (history[igrp].size() > max_count) {
        history[igrp].pop();
      }
    }
  }

  struct win_desc_type {
    size_t win_event;   // used if win_type::event
    data_type win_time; // used if win_type::time
    bool cumulative;    // no eviction if cumulative
    bool dynamic;       // query window_size() on every step
    win_type type;      // window type
  };

  // allocator types
  using data_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<data_type>;
  using hbuf_type = detail::history_buffer<data_type, data_alloc>;
  using hbuf_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<hbuf_type>;
  using size_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<size_t>;
  using win_desc_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<win_desc_type>;

  size_t ngrp;                      ///< Number of groups
  graph_topo_fanout<op_type> graph; ///< DAG to execute, uses its own alloc

  std::vector<hbuf_type, hbuf_alloc> history;             ///< History buffer for node I/O data
  std::vector<size_t, size_alloc> record_offset;          ///< History record offsets for each node, shared
  detail::flat_multivect<size_t, size_alloc> args_offset; ///< Argument offsets for each node, shared

  bool all_cumulative;                                 ///< True if all nodes are in cumulative mode, shared
  std::vector<win_desc_type, win_desc_alloc> win_desc; ///< Window descriptors for each node, shared
  detail::vector_store<size_t, size_alloc> step_count; ///< Step count for each node, used in step sliding mode

  detail::vector_store<data_type, data_alloc> curr_args; ///< Reused for current node arguments
};
} // namespace opflow
