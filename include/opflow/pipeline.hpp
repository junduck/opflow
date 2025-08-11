#pragma once

#include <memory>
#include <stdexcept>

#include "common.hpp"
#include "detail/history_ringbuf.hpp"
#include "graph.hpp"
#include "op_base.hpp"
#include "topo_graph.hpp"

namespace opflow {

// A pipeline handles a DAG of operators

/**
 * @brief Sliding window types for a pipeline.
 *
 */
enum class sliding {
  time, ///< sliding window based on time
  step, ///< sliding window based on step
};

/**
 * @brief Window descriptor a operator node.
 *
 * @tparam Time
 */
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
   * @details If window_period is zero, pipeline will consult op->window_period() on each step to determine
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

/**
 * @brief pipeline
 *
 * @details A pipeline maintains a sliding window of data for a DAG.
 *
 * @tparam Time
 * @tparam Data
 */
template <typename Time, typename Data>
class pipeline {
public:
  using time_type = Time;
  using duration_type = duration_t<time_type>;
  using data_type = Data;
  using op_type = op_base<time_type, data_type>;
  using node_type = std::shared_ptr<op_type>;

  pipeline(graph<node_type> const &g, sliding mode,
           associative<node_type, window_descriptor<time_type>> auto const &desc)
      : g(g), nodes(g), data_offset(), data_size(), history(), window_mode(mode), window_desc(), last_removed(),
        step_count(), all_cumulative() {

    validate_nodes_topo();
    validate_nodes_compat();

    init_io_index();

    init_history();

    init_window_desc(desc);

    init_window_meta();
  }

  void step(time_type timestamp, std::span<const data_type> input_data) {
    // 0. validate timestamp and input data
    if (!history.empty() && timestamp <= history.back().first) {
      throw std::runtime_error("Non-monotonic timestamp detected in pipeline step.");
    }
    if (input_data.size() != nodes[0]->num_outputs()) {
      throw std::runtime_error("Input data size does not match root input node requirement.");
    }

    // 1. insert new data into nodes

    auto [_, data] = history.push(timestamp);

    // 1.0. handle root input node
    std::vector<data_type const *> input_ptrs{input_data.data()};
    nodes[0]->step(input_ptrs.data());
    nodes[0]->value(data.data()); // Store root input node data directly

    for (size_t i = 1; i < nodes.size(); ++i) {
      // 1.1. prepare input pointers for current node
      input_ptrs.clear();
      for (auto dep : nodes.pred_of(i)) {
        input_ptrs.push_back(data.data() + data_offset[dep]);
      }
      // 1.2. call step on the operator node
      nodes[i]->step(input_ptrs.data());
      if (!window_desc[i].cumulative) {
        // only update step_count for non cumulative nodes
        ++step_count[i];
      }

      // 2. remove expired data if needed

      if (!window_desc[i].cumulative) {
        switch (window_mode) {
        case sliding::time:
          inverse_sliding_time(timestamp, i);
          break;
        case sliding::step:
          inverse_sliding_step(timestamp, i);
          break;
        }
      }

      // 3. collect operator node output

      double *output_ptr = data.data() + data_offset[i];
      nodes[i]->value(output_ptr);
    }

    // 4. flush unneeded history entries

    if (all_cumulative) {
      // If all nodes are cumulative, we leave newest entry for output, remove all others
      while (history.size() > 1) {
        history.pop();
      }
      return;
    }

    size_t max_window_period = std::ranges::max(step_count);
    while (history.size() > max_window_period) {
      history.pop();
    }
#if 0
    // TODO: we need history.flush() for side effects/observability
    switch (window_mode) {
    case sliding::time: {
      // we only need to keep data within the oldest removed timestamp across all nodes
      time_type oldest_removed = std::ranges::min(last_removed);
      while (!history.empty() && history.front().first < oldest_removed) {
        history.pop();
      }
      break;
    }
    case sliding::step:
      // we only need to keep max_window_period steps in history
      size_t max_window_period = std::ranges::max(step_count);
      while (history.size() > max_window_period) {
        history.pop();
      }
      break;
    }
#endif
  }

  // TODO: there is a disconnect between the meaningful node objects user creates and the opaque IDs the current
  // interface requires the user to use for output access

  template <std::ranges::forward_range R>
  auto get_id(R &&node_range) const {
    std::vector<size_t> ids;
    ids.reserve(std::ranges::distance(node_range));
    for (auto const &node : node_range) {
      ids.push_back(get_id(node));
    }
    return ids;
  }

  auto get_id(node_type const &node) const {
    auto it = std::ranges::find(nodes, node);
    if (it == nodes.end()) {
      throw node_error("Node not found in pipeline.", node);
    }
    return std::distance(nodes.begin(), it);
  }

  auto leaf_ids() const { return nodes.leaf_ids(); }

  template <std::ranges::forward_range R>
  auto get_output(R &&id_range) const {
    std::vector<std::span<const data_type>> outputs;
    outputs.reserve(std::ranges::distance(id_range));
    for (auto id : id_range) {
      outputs.push_back(get_output(id));
    }
    return outputs;
  }

  auto get_output(size_t node_id) const {
    if (node_id >= nodes.size()) {
      throw std::out_of_range("Node ID out of range in get_output.");
    }
    auto [_, latest_data] = history.back();
    size_t pos = data_offset[node_id];
    size_t size = nodes[node_id]->num_outputs();
    return latest_data.subspan(pos, size);
  }

private:
  void validate_nodes_topo() const {
    // check empty graph
    if (nodes.empty()) {
      throw std::runtime_error("Pipeline cannot be constructed from an empty graph.");
    }
    // check any null operator nodes
    if (std::ranges::any_of(nodes, [](auto const &node) { return node == nullptr; })) {
      throw std::runtime_error("Pipeline contains null operator node.");
    }
    // check number of roots (input nodes)
    if (nodes.get_roots().size() != 1) {
      throw std::runtime_error("Pipeline must have exactly one root input node.");
    }
  }

  void validate_nodes_compat() const {
    for (size_t i = 1; i < nodes.size(); ++i) {
      auto const &node = nodes[i];
      // collect pointers to dependencies
      auto deps = std::views::all(nodes.pred_of(i)) |
                  std::views::transform([this](size_t id) { return static_cast<op_type const *>(nodes[id].get()); });
      if (!node->compatible_with(deps)) {
        throw node_error("Operator node is not compatible with its dependencies.", node);
      }
    }
  }

  void init_io_index() {
    data_offset.reserve(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i) {
      data_offset.push_back(data_size);
      data_size += nodes[i]->num_outputs();
    }
  }

  void init_history() { history.init(data_size); }

  void init_window_desc(auto const &desc) {
    window_desc.reserve(nodes.size());
    // handle root input node
    window_desc.emplace_back();
    // handle other nodes
    for (size_t i = 1; i < nodes.size(); ++i) {
      auto const &node = nodes[i];
      auto it = desc.find(node);
      if (it == desc.end()) {
        throw node_error("Operator node is not registered in the pipeline descriptor.", node);
      }
      window_desc.push_back(it->second);
    }
  }

  void init_window_meta() {
    // collect cumulative nodes
    all_cumulative = true;
    for (size_t i = 0; i < nodes.size(); ++i) {
      if (!window_desc[i].cumulative) {
        all_cumulative = false;
        break;
      }
    }
    // initialise sliding window metadata
    switch (window_mode) {
    case sliding::time:
      last_removed.resize(nodes.size(), min_time<time_type>());
      step_count.resize(nodes.size(), 0);
      for (size_t i = 0; i < nodes.size(); ++i) {
        if (window_desc[i].cumulative) {
          last_removed[i] = max_time<time_type>();
          step_count[i] = 1; // Cumulative nodes always keep last step
        }
      }
      break;
    case sliding::step:
      step_count.resize(nodes.size(), 0);
      for (size_t i = 0; i < nodes.size(); ++i) {
        if (window_desc[i].cumulative) {
          step_count[i] = 1; // Cumulative nodes always keep last step
        }
      }
      break;
    default:
      // TODO: add time weighted window type (delta_t inserted, 1 step lagged data)
      break;
    }
  }

  void inverse_sliding_time(time_type timestamp, size_t id) {
    // Pre: op contains data in  (last_removed[id],    timestamp]
    // Post: op contains data in (window_start,        timestamp]
    // Obj: remove data in       (last_removed[id], window_start], update last_removed[id]

#if 0
    // 1. Calculate removal range
    auto w = window_desc[id].window_size != duration_type{} ? window_desc[id].window_size : nodes[id]->window_size();
    auto rm_start = last_removed[id];
    auto rm_end = timestamp - w;
    auto rm_last = last_removed[id];
    // In case of an enlarged dynamic window, window_start could be older than last_removed
    if (rm_start >= rm_end) {
      return; // No data to remove
    }
#endif
    auto w = window_desc[id].window_size != duration_type{} ? window_desc[id].window_size : nodes[id]->window_size();
    auto window_start = timestamp - w;

    // 2. Process history entries that need to be removed
    std::vector<data_type const *> rm_ptrs;
    rm_ptrs.reserve(nodes.pred_of(id).size());

    size_t start_idx = history.size() - step_count[id];

    for (size_t i = start_idx; i < history.size(); ++i) {
      auto [time, data] = history[i];
      if (time > window_start)
        break;
      // This data point is in the range (rm_start, rm_end]
      rm_ptrs.clear();
      for (auto dep : nodes.pred_of(id)) {
        rm_ptrs.push_back(data.data() + data_offset[dep]);
      }
      nodes[id]->inverse(rm_ptrs.data());
      --step_count[id];
    }
#if 0
    for (auto const [time, data] : history) {
      if (time <= rm_start)
        continue;
      if (time > rm_end)
        break;
      // This data point is in the range (rm_start, rm_end]
      rm_ptrs.clear();
      for (auto dep : nodes.pred_of(id)) {
        rm_ptrs.push_back(data.data() + data_offset[dep]);
      }
      nodes[id]->inverse(rm_ptrs.data());
      --step_count[id];
      // record last removed timestamp
      rm_last = time;
    }
    // 3. Update last removed timestamp
    last_removed[id] = rm_last;
#endif
  }

  void inverse_sliding_step(time_type timestamp, size_t id) {
    // Pre: op contains data at history idx  [size() - step_count[id], size() - 1]
    // Post: op contains data at history idx [size() - window_period,  size() - 1]
    // Obj: remove data at history idx       [size() - step_count[id], size() - window_period) <- right open

    std::ignore = timestamp; // Unused in tumbling pipelines
    // 1. get current window period
    auto window_period = window_desc[id].window_period ? window_desc[id].window_period : nodes[id]->window_period();
    if (window_period == 0) {
      throw node_error("Operator node does not provide a valid window period.", nodes[id]);
    }

    // 2. check if we need to remove data
    if (step_count[id] <= window_period)
      return; // Nothing to do, we are accumulating data

    assert(history.size() >= step_count[id] && "[BUG] History is smaller than step count for node.");

    size_t rm_start = history.size() - step_count[id];
    size_t rm_end = history.size() - window_period;

    // 3.1. prepare data for inverse
    std::vector<const data_type *> rm_ptrs;
    rm_ptrs.reserve(nodes.pred_of(id).size());

    for (size_t i = rm_start; i < rm_end; ++i) {
      auto [time, data] = history[i];
      rm_ptrs.clear();
      for (auto dep : nodes.pred_of(id)) {
        size_t data_start = data_offset[dep];
        rm_ptrs.push_back(data.data() + data_start);
      }
      nodes[id]->inverse(rm_ptrs.data());
      // 3.2 decrement step count
      --step_count[id];
    }
    assert(step_count[id] == window_period &&
           "[BUG] Step count is not equal to window period after inverse sliding step.");
  }

private:
  graph<node_type> g;
  topo_graph<node_type> nodes;
  // TODO: history provider should be a template parameter for future extensibility
  std::vector<size_t> data_offset;                       ///< I/O data offsets for each node
  size_t data_size;                                      ///< Total size of I/O data for all nodes
  detail::history_ringbuf<time_type, data_type> history; ///< History buffer for node I/O data

  // window management
  sliding window_mode;                                   ///< Window sliding mode
  std::vector<window_descriptor<time_type>> window_desc; ///< Window descriptors for each node

  std::vector<time_type> last_removed; ///< Last window boundary time for each node, used in time sliding mode
  std::vector<size_t> step_count;      ///< Step count for each node, used in step sliding mode

  bool all_cumulative;
};

} // namespace opflow
