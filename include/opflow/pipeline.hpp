#pragma once

#include <chrono>
#include <memory>
#include <stdexcept>

#include "graph.hpp"
#include "history_ringbuf.hpp"
#include "op_base.hpp"
#include "topo_graph.hpp"

namespace opflow {

template <typename Container, typename Key, typename Value>
concept associative = requires(Container const c) {
  // query using find
  { c.find(std::declval<Key>()) } -> std::same_as<typename Container::const_iterator>;
  { c.end() } -> std::same_as<typename Container::const_iterator>;

  // check iterator returns [key, value] pairs
  { std::declval<typename Container::const_iterator>()->first } -> std::convertible_to<Key const &>;
  { std::declval<typename Container::const_iterator>()->second } -> std::convertible_to<Value const &>;
};

// Dev only we specialise this to make clangd more efficient
using Time = int; // std::chrono::system_clock::time_point;
using Data = double;

// A pipeline handles a DAG of operators

enum class sliding {
  time, ///< sliding window based on time
  step, ///< sliding window based on step
};

template <typename Time>
struct window_descriptor {
  using time_type = Time;
  using duration_type = duration_t<time_type>;
  /**
   * @brief Window size if node is run in sliding pipeline
   *
   * @details If window_size is set to zero, pipeline will consult op->window_start() on each step to determine window
   * dynamically.
   *
   */
  duration_type window_size;

  /**
   * @brief Window period if node is run in tumbling pipeline
   *
   * @details If window_period is zero, pipeline will consult op->window_period() *ONCE* on intialisation to determine
   * window period.
   */
  size_t window_period;

  bool cumulative;

  window_descriptor() noexcept : window_size(), window_period(), cumulative(true) {}

  window_descriptor(bool cumulative, duration_type window_size)
    requires(!std::is_same_v<duration_type, size_t>)
      : window_size(window_size), window_period(), cumulative(cumulative) {}

  window_descriptor(bool cumulative, size_t window_period)
    requires(!std::is_same_v<duration_type, size_t>)
      : window_size(), window_period(window_period), cumulative(cumulative) {}
};

template <typename NodeType>
class node_error : public std::runtime_error {
public:
  using node_type = NodeType;

  node_error(auto &&msg, node_type const &node) : std::runtime_error(std::forward<decltype(msg)>(msg)), node(node) {}

  node_type const &get_node() const noexcept { return node; }

private:
  node_type node;
};

template <typename Time>
Time min_time() noexcept {
  return Time::min(); // Use min time for non-arithmetic types
}

template <typename Time>
Time min_time() noexcept
  requires(std::is_arithmetic_v<Time>)
{
  return std::numeric_limits<Time>::min();
}

// template <typename Time, typename Data>
struct pipeline {
  using time_type = Time;
  using duration_type = duration_t<time_type>;
  using data_type = Data;
  using op_type = op_base<time_type, data_type>;
  using node_type = std::shared_ptr<op_type>;

  topo_graph<node_type> nodes;
  // TODO: history provider should be a template parameter for future extensibility
  std::vector<size_t> data_offset;               ///< I/O data offsets for each node
  size_t data_size;                              ///< Total size of I/O data for all nodes
  history_ringbuf<time_type, data_type> history; ///< History buffer for node I/O data

  // window management
  sliding window_mode;                                   ///< Window sliding mode
  std::vector<window_descriptor<time_type>> window_desc; ///< Window descriptors for each node

  std::vector<time_type> last_removed; ///< Last window boundary time for each node, used in time sliding mode
  std::vector<size_t> step_count;      ///< Step count for each node, used in step sliding mode
  size_t max_window_period;            ///< Maximum window period across all nodes, used in step sliding mode

  bool all_cumulative;

  pipeline(graph<node_type> const &g, sliding mode,
           associative<node_type, window_descriptor<time_type>> auto const &desc)
      : nodes(g), data_offset(), data_size(), history(), window_mode(mode), window_desc(), last_removed(), step_count(),
        max_window_period(), all_cumulative() {
    // 0. check empty graph and any nullptr nodes
    if (nodes.empty()) {
      throw std::runtime_error("Pipeline cannot be constructed from an empty graph.");
    }
    if (std::ranges::any_of(nodes, [](auto const &node) { return node == nullptr; })) {
      throw std::runtime_error("Pipeline contains null operator node.");
    }

    // 1. check number of roots (input nodes)
    if (nodes.get_roots().size() != 1) {
      throw std::runtime_error("Pipeline must have exactly one root input node.");
    }

    // 2. check compatiblity of nodes, nodes are topo-sorted, we skip root input node
    for (size_t i = 1; i < nodes.size(); ++i) {
      auto const &node = nodes[i];
      // collect pointers to dependencies
      auto deps = std::views::all(nodes.preds(i)) |
                  std::views::transform([this](size_t id) { return static_cast<op_type const *>(nodes[id].get()); });
      if (!node->compatible_with(deps)) {
        throw node_error("Operator node is not compatible with its dependencies.", node);
      }
    }

    // 3. calculate data offsets and total size
    data_offset.reserve(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i) {
      data_offset.push_back(data_size);
      data_size += nodes[i]->num_outputs();
    }

    // 4. initialise history buffer
    history.init(data_size);

    // 5. initialise window descriptors
    window_desc.reserve(nodes.size());
    // 5.1. handle root input node
    window_desc.emplace_back();
    // 5.2. handle other nodes
    for (size_t i = 1; i < nodes.size(); ++i) {
      auto const &node = nodes[i];
      auto it = desc.find(node);
      if (it == desc.end()) {
        throw node_error("Operator node is not registered in the pipeline descriptor.", node);
      }
      auto d = it->second;
      if (window_mode == sliding::step && d.window_period == 0) {
        auto p = node->window_period();
        if (p == 0) {
          throw node_error("Operator node does not provide a valid window period.", node);
        }
        d.window_period = p;
      }
      window_desc.push_back(d);
      max_window_period = std::max(max_window_period, d.window_period);
    }

    // 6. initialise window boundaries
    switch (window_mode) {
    case sliding::time:
      last_removed.resize(nodes.size(), min_time<time_type>());
      all_cumulative = std::ranges::all_of(window_desc, [](auto const &d) { return d.cumulative; });
      break;
    case sliding::step:
      step_count.resize(nodes.size(), 0);
      break;
    default:
      // TODO: add time weighted window type (delta_t inserted, 1 step lagged data)
      break;
    }
  }

  auto step(time_type timestamp, std::span<data_type> input_data) {
    // 0. validate timestamp and input data
    if (!history.empty() && timestamp <= history.back().first) {
      throw std::runtime_error("Non-monotonic timestamp detected in pipeline step.");
    }
    if (input_data.size() != nodes[0]->num_outputs()) {
      throw std::runtime_error("Input data size does not match root input node requirement.");
    }

    // 1. prepare data buffer for current step, we use a mutable span provided by history to avoid copy
    auto [_unused, data] = history.push(timestamp);

    std::vector<data_type const *> input_ptrs;
    for (size_t i = 0; i < nodes.size(); ++i) {
      // 1.1. prepare input pointers for input
      input_ptrs.clear();
      if (i == 0) {
        // Root input node gets external input
        input_ptrs.push_back(input_data.data());
      } else {
        // Construct input from predecessors/dependencies in current step
        for (auto dep : nodes.preds(i)) {
          input_ptrs.push_back(data.data() + data_offset[dep]);
        }
      }

      // 1.2.1. call step on the operator node
      nodes[i]->step(timestamp, input_ptrs.data());
      // 1.2.1.0. for root input node we write data directly and skip to next node
      if (i == 0) {
        nodes[i]->value(data.data());
        continue;
      }
      // 1.2.2. update step count for tumbling pipelines
      if (window_mode == sliding::step) {
        ++step_count[i];
      }

      // 1.3. check retention policy and call inverse if needed
      switch (window_mode) {
      case sliding::time:
        if (!window_desc[i].cumulative) {
          inverse_streaming(timestamp, i);
        }
        break;
      case sliding::step:
        inverse_tumbling(timestamp, i);
        break;
      }

      // 1.4. collect operator node output
      double *output_ptr = data.data() + data_offset[i];
      nodes[i]->value(output_ptr);
    }

    // 2. flush unneeded history entries
    switch (window_mode) {
    case sliding::time: {
      // TODO: we need history.flush() for side effects/observability
      // providers
      if (all_cumulative) {
        // If all nodes are cumulative, we can clear history
        history.clear();
      } else {
        // we only need to keep data within the oldest removed timestamp across all nodes
        time_type oldest_removed = timestamp;
        for (auto t : last_removed) {
          oldest_removed = std::min(oldest_removed, t);
        }
        while (!history.empty() && history.front().first < oldest_removed) {
          history.pop();
        }
      }
      break;
    }
    case sliding::step:
      // we only need to keep max_window_period steps in history
      while (history.size() > max_window_period) {
        history.pop();
      }
      break;
    }
  }

  // Post: op only contains data in (window_start, timestamp]
  void inverse_streaming(time_type timestamp, size_t id) {
    // 1. Calculate window start
    auto w = window_desc[id].window_size;
    time_type window_start = (w == duration_type{}) ? nodes[id]->window_start() : timestamp - w;
    if (window_start <= last_removed[id]) {
      return; // No new data to remove
    }

    // 2. Process history entries that need to be removed
    std::vector<const data_type *> rm_ptrs;
    rm_ptrs.reserve(nodes.preds(id).size());

    time_type new_boundary = last_removed[id];

    for (auto const [time, data] : history) {
      if (time > window_start)
        break; // No more data to remove, we are done
      if (time <= last_removed[id])
        continue; // Skip data that is already removed before

      // This data point is in the range (last_removed[id], window_start]
      rm_ptrs.clear(); // Clear previous pointers
      for (auto dep : nodes.preds(id)) {
        rm_ptrs.push_back(data.data() + data_offset[dep]);
      }
      nodes[id]->inverse(time, rm_ptrs.data());

      // Update the new boundary to the last removed timestamp
      new_boundary = time;
    }

    // 3. Update boundary to the last removed timestamp
    last_removed[id] = new_boundary;
  }

  // Post: op only contains window_period data points
  void inverse_tumbling(time_type timestamp, size_t id) {
    std::ignore = timestamp; // Unused in tumbling pipelines
    // 1. get current window period
    auto window_period = window_desc[id].window_period;

    // 2. check if we need to remove data
    if (step_count[id] <= window_period)
      return; // Nothing to do, we are accumulating data

    // Since we call inverse_tumbling on every step, we should have exactly 1 expired data to remove
    assert(step_count[id] == window_period + 1 && "[BUG] Step count inconsistent with window period.");
    assert(!history.empty() && "[BUG] History is empty when calling inverse_tumbling.");

    // 3.1. prepare data for inverse
    const auto [time, data] = history[0];
    std::vector<const data_type *> rm_ptrs;
    rm_ptrs.reserve(nodes.preds(id).size());
    for (auto dep : nodes.preds(id)) {
      rm_ptrs.push_back(data.data() + data_offset[dep]);
    }
    // 2.2 call inverse on the operator node
    nodes[id]->inverse(time, rm_ptrs.data());
    // 2.3 decrement step count
    --step_count[id];
  }

  auto get_output(size_t node_id) const {
    if (node_id >= nodes.size()) {
      throw std::out_of_range("Node ID out of range in get_output.");
    }
    auto [latest_tick, latest_data] = history.back();
    size_t start_idx = data_offset[node_id];
    size_t num_outputs = nodes[node_id]->num_outputs();

    // 2. Return the output for the specified node
    return latest_data.subspan(start_idx, num_outputs);
  }
};

} // namespace opflow
