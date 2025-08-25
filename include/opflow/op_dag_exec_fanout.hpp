#pragma once

#include <atomic>
#include <memory>

#include "common.hpp"
#include "def.hpp"
#include "detail/flat_multivect.hpp"
#include "detail/history_buffer.hpp"
#include "detail/vector_store.hpp"
#include "graph_node.hpp"
#include "graph_topo_fanout.hpp"
#include "opflow/op_base.hpp"

namespace opflow {
using T = double;                // Temporary, to be templated later
using Alloc = std::allocator<T>; // Temporary, to be templated later
// template <typename T, typename Alloc = std::allocator<T>>

/**
 * @brief Fan-out multi-group DAG executor
 *
 * This is an extended version of op_dag_exec that supports multiple groups for fan-out execution.
 * Each group maintains its own execution state and history buffer, allowing for concurrent
 * processing of multiple data streams through the same DAG structure.
 *
 * Key differences from op_dag_exec:
 * - Multiple groups with independent state
 * - Separate history buffers per group
 * - Thread-safe synchronization per group
 * - Shared DAG structure with cloned operators
 */
class op_dag_exec_fanout {
public:
  using data_type = T;
  using op_type = op_base<data_type>;
  using graph_node_type = std::shared_ptr<op_type>;

  template <range_of<graph_node_type> R>
  op_dag_exec_fanout(graph_node<op_type> const &g, R &&output_nodes, size_t num_groups)
      : ngrp(num_groups), sync(ngrp),                                 // sync
        graph(g, output_nodes, ngrp),                                 // DAG
        history(), record_offset(), args_offset(),                    // data
        all_cumulative(), win_desc(), step_count(graph.size(), ngrp), // window
        curr_args(max_num_inputs(), ngrp)                             // args
  {
    init_data();
    init_window();
  }

  op_dag_exec_fanout(graph_node<op_type> const &g, std::initializer_list<graph_node_type> output_nodes,
                     size_t num_groups)
      : op_dag_exec_fanout(g, std::vector<graph_node_type>(output_nodes), num_groups) {}

  void on_data(data_type timestamp, data_type const *input_data, size_t igrp) {
    sync[igrp].enter();
    auto [_, mem] = history[igrp].push(timestamp);

    auto grp = graph.nodes_of(igrp);
    grp[0]->on_data(input_data);
    grp[0]->value(out_ptr(mem, 0));

    commit_input_buffer(igrp);
  }

  void value(data_type *OPFLOW_RESTRICT out, size_t igrp) const noexcept {
    auto [_, mem] = history[igrp].back();
    size_t i = 0;
    for (auto [id, size] : graph.nodes_out()) {
      for (size_t port = 0; port < size; ++port) {
        out[i++] = mem[record_offset[id] + port];
      }
    }
    sync[igrp].exit();
  }

  size_t num_inputs() const noexcept {
    auto grp = graph.nodes_of(0);
    return grp[0]->num_inputs();
  }

  size_t num_outputs() const noexcept {
    auto outputs = graph.nodes_out();
    size_t total = 0;
    for (auto const &out : outputs) {
      total += out.size;
    }
    return total;
  }

  data_type *input_buffer(data_type timestamp, size_t igrp) {
    sync[igrp].enter();
    auto [_, mem] = history[igrp].push(timestamp);
    return mem.data();
  }

  void commit_input_buffer(size_t igrp) {
    auto [timestamp, mem] = history[igrp].back();
    auto grp = graph.nodes_of(igrp);

    for (size_t i = 1; i < graph.size(); ++i) {
      // call node
      grp[i]->on_data(in_ptr(mem, i, igrp));
      // handle eviction for non-cumulative nodes
      if (!win_desc[i].cumulative) {
        // update step_count
        ++step_count.get(igrp)[i];
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
      grp[i]->value(out_ptr(mem, i));
    }

    cleanup_history(igrp);
  }

  size_t num_groups() const noexcept { return ngrp; }

  bool empty() const noexcept { return graph.empty(); }

  explicit operator bool() const noexcept { return !empty(); }

private:
  data_type *out_ptr(auto record, size_t node_id) noexcept { return record.data() + record_offset[node_id]; }
  data_type const *in_ptr(auto record, size_t node_id, size_t grp_id) {
    auto args = curr_args.get(grp_id);
    auto offsets = args_offset[node_id];
    assert(args.size() == offsets.size() && "[BUG] Argument size mismatch");
    for (size_t i = 0; i < args.size(); ++i) {
      args[i] = record[offsets[i]];
    }
    return args.data();
  }

  void init_data() {
    // we only need to test group 0
    auto grp = graph.nodes_of(0);

    size_t total_size = 0;
    record_offset.reserve(graph.size());
    for (size_t i = 0; i < graph.size(); ++i) {
      record_offset.push_back(total_size);
      total_size += grp[i]->num_outputs();
    }
    history.reserve(ngrp);
    for (size_t i = 0; i < ngrp; ++i) {
      history.emplace_back(total_size, 2);
    }

    std::vector<size_t> args{};
    args_offset.reserve(graph.size(), graph.size_edge());
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
    auto grp = graph.nodes_of(0);

    win_desc.reserve(graph.size());
    size_t n_cumulative = 0;
    for (size_t i = 0; i < graph.size(); ++i) {
      win_desc_t desc{};
      if (grp[i]->is_cumulative()) {
        // cumulative op
        desc.cumulative = true;
        for (size_t j = 0; j < ngrp; ++j) {
          step_count.get(j)[i] = 1;
        }
        ++n_cumulative;
      } else {
        // windowed op
        desc.dynamic = grp[i]->is_dynamic();
        desc.type = grp[i]->window_type();
        switch (desc.type) {
        case win_type::event:
          desc.win_event = grp[i]->window_size(event_window);
          break;
        case win_type::time:
          desc.win_time = grp[i]->window_size(time_window);
          break;
        }
      }
      win_desc.push_back(desc);
    }
    all_cumulative = n_cumulative == graph.size();
  }

  size_t max_num_inputs() const noexcept {
    // we only need to test group 0
    auto grp = graph.nodes_of(0);

    size_t max_inputs = 0;
    for (size_t i = 1; i < graph.size(); ++i) {
      max_inputs = std::max(max_inputs, grp[i]->num_inputs());
    }
    return max_inputs;
  }

  void evict_event(data_type timestamp, size_t id, size_t igrp) {
    // Pre: op contains data at history idx  [k,  N - 1], where k = N - step_count[id];
    // Post: op contains data at history idx [k', N - 1], where k' = N - win_size
    // Obj: remove data at history idx       [k,  k')

    std::ignore = timestamp;
    auto &step_cnt = step_count.get(igrp)[id];
    assert(history[igrp].size() >= step_cnt && "[BUG] History is smaller than step count for node.");

    auto grp = graph.nodes_of(igrp);
    auto win_size = win_desc[id].dynamic ? grp[id]->window_size(event_window) : win_desc[id].win_event;
    if (step_cnt <= win_size)
      return; // No data to remove

    auto k = history[igrp].size() - step_cnt;
    auto kp = history[igrp].size() - win_size;

    for (size_t i = k; i < kp; ++i) {
      auto [_, mem] = history[igrp][i];
      grp[id]->on_evict(in_ptr(mem, id, igrp));
      --step_cnt;
    }
  }

  void evict_time(data_type timestamp, size_t id, size_t igrp) {
    // Pre: op contains data at history idx  [k,  N - 1], where k = N - step_count[id]
    // Post: op contains data at history idx [k', N - 1], where k' = argmin(time[i] > win_start)
    // Obj: remove data at history idx       [k,  k')

    auto &step_cnt = step_count.get(igrp)[id];
    assert(history[igrp].size() >= step_cnt && "[BUG] History is smaller than step count for node.");

    auto grp = graph.nodes_of(igrp);
    auto win_size = win_desc[id].dynamic ? grp[id]->window_size(time_window) : win_desc[id].win_time;
    auto win_start = timestamp - win_size;
    auto k = history[igrp].size() - step_cnt;

    for (size_t i = k; i < history[igrp].size(); ++i) {
      auto [time, mem] = history[igrp][i];
      if (time > win_start) // k'
        break;              // No more data to remove
      grp[id]->on_evict(in_ptr(mem, id, igrp));
      --step_cnt;
    }
  }

  void cleanup_history(size_t igrp) {
    if (all_cumulative) {
      // All nodes are cumulative, we only need to store latest output
      while (history[igrp].size() > 1) {
        history[igrp].pop();
      }
    } else {
      // Pop entries that no longer need
      auto steps = step_count.get(igrp);
      size_t max_count = *std::ranges::max_element(steps);
      while (history[igrp].size() > max_count) {
        history[igrp].pop();
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

  /**
   * @brief Synchronisation point for each group
   *
   * We don't have concurrent access to a single group, so we only need a simple sync barrier to make side effect
   * visible for subsequent access to same group.
   *
   */
  struct alignas(cacheline_size) sync_point {
    std::atomic<size_t> seq;

    void enter() noexcept { seq.load(std::memory_order::acquire); }
    void exit() noexcept { seq.fetch_add(1, std::memory_order::release); }
  };

  size_t ngrp;                          ///< Number of groups for the fan-out execution
  mutable std::vector<sync_point> sync; ///< Synchronisation points for each group

  graph_topo_fanout<op_type> graph; ///< DAG to execute, fan-out

  std::vector<detail::history_buffer<data_type>> history; ///< History buffer for node I/O data, fan-out
  std::vector<size_t> record_offset;                      ///< History record offsets for each node, shared
  detail::flat_multivect<size_t> args_offset;             ///< Argument offsets for each node, shared

  bool all_cumulative;                     ///< True if all nodes are in cumulative mode, shared
  std::vector<win_desc_t> win_desc;        ///< Window descriptors for each node, shared
  detail::vector_store<size_t> step_count; ///< Step count for each node, used in step sliding mode, fan-out

  detail::vector_store<data_type> curr_args; ///< Reused for current node arguments
};

} // namespace opflow
