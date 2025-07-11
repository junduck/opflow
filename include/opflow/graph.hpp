#pragma once

#include <algorithm>
#include <functional>
#include <queue>
#include <ranges>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace opflow {

/// Exception thrown when a cycle is detected in the graph
class CycleError : public std::runtime_error {
public:
  explicit CycleError(const std::string &message) : std::runtime_error(message) {}
};

/// A generic topological sorter for directed acyclic graphs (DAGs)
/// This is a C++ clone of Python's graphlib.TopologicalSorter
template <typename T, typename Hash = std::hash<T>, typename Equal = std::equal_to<T>>
class TopologicalSorter {
public:
  using NodeSet = std::unordered_set<T, Hash, Equal>;
  using NodeMap = std::unordered_map<T, NodeSet, Hash, Equal>;

private:
  NodeMap graph_;             // Adjacency list: node -> set of successors
  NodeMap incoming_edges_;    // node -> set of predecessors
  std::queue<T> ready_queue_; // Nodes ready for processing
  NodeSet processing_;        // Nodes currently being processed
  bool prepared_ = false;     // Whether prepare() has been called

  /// DFS-based cycle detection
  bool has_cycle_dfs(const T &node, NodeSet &visited, NodeSet &rec_stack) {
    visited.insert(node);
    rec_stack.insert(node);

    auto it = graph_.find(node);
    if (it != graph_.end()) {
      for (const auto &neighbor : it->second) {
        if (rec_stack.find(neighbor) != rec_stack.end()) {
          return true; // Back edge found - cycle detected
        }

        if (visited.find(neighbor) == visited.end() && has_cycle_dfs(neighbor, visited, rec_stack)) {
          return true;
        }
      }
    }

    rec_stack.erase(node);
    return false;
  }

  NodeSet const &empty_set() const {
    static NodeSet empty_set;
    return empty_set;
  }

public:
  /// Default constructor
  TopologicalSorter() = default;

  /// Constructor that takes an initial graph as adjacency map
  /// @param graph Map where keys are nodes and values are sets of their dependencies
  template <std::ranges::input_range Range>
  explicit TopologicalSorter(Range &&graph) {
    for (auto const &pair : graph) {
      auto const &node = std::get<0>(pair);
      auto const &deps = std::get<1>(pair);
      add(node, NodeSet(std::ranges::begin(deps), std::ranges::end(deps)));
    }
  }

  explicit TopologicalSorter(NodeMap const &graph) : TopologicalSorter(std::views::all(graph)) {}

  /// Add a new node with optional dependencies
  /// @param node The node to add
  /// @param predecessors Nodes that this node depends on
  template <std::ranges::input_range Range>
  void add(T const &node, Range &&predecessors) {
    // Add the node itself
    if (graph_.find(node) == graph_.end()) {
      graph_[node] = NodeSet{};
    }

    // Add dependencies
    for (auto const &pred : predecessors) {
      // Ensure predecessor exists in graph
      if (graph_.find(pred) == graph_.end()) {
        graph_[pred] = NodeSet{};
      }

      // Add edge from predecessor to node
      graph_[pred].insert(node);

      // Track incoming edges for node
      if (incoming_edges_.find(node) == incoming_edges_.end()) {
        incoming_edges_[node] = NodeSet{};
      }
      incoming_edges_[node].insert(pred);
    }

    // If node has no incoming edges recorded, initialize empty set
    if (incoming_edges_.find(node) == incoming_edges_.end()) {
      incoming_edges_[node] = NodeSet{};
    }
  }

  void add(T const &node, NodeSet const &predecessors = {}) { add(node, predecessors | std::views::all); }

  /// Prepare the graph for iteration by checking for cycles
  /// Must be called before calling get_ready() or done()
  void prepare() {
    if (prepared_) {
      throw std::runtime_error("TopologicalSorter.prepare() was called more than once");
    }

    // Check for cycles using DFS
    NodeSet visited;
    NodeSet rec_stack;

    for (auto const &[node, _] : graph_) {
      if (visited.find(node) == visited.end()) {
        if (has_cycle_dfs(node, visited, rec_stack)) {
          throw CycleError("Graph contains a cycle");
        }
      }
    }

    // Initialize ready queue with nodes that have no dependencies
    ready_queue_ = std::queue<T>{};
    processing_ = NodeSet{};

    for (auto const &[node, deps] : incoming_edges_) {
      if (deps.empty()) {
        ready_queue_.push(node);
      }
    }

    prepared_ = true;
  }

  /// Check if the sorting process is complete
  /// @return true if all nodes have been processed
  bool done() const {
    if (!prepared_) {
      throw std::runtime_error("TopologicalSorter.done() called before prepare()");
    }
    return ready_queue_.empty() && processing_.empty();
  }

  /// Get nodes that are ready to be processed
  /// @param n Maximum number of nodes to return (0 means all available)
  /// @return Vector of nodes ready for processing
  std::vector<T> get_ready(size_t n = 0) {
    if (!prepared_) {
      throw std::runtime_error("TopologicalSorter.get_ready() called before prepare()");
    }

    std::vector<T> result;
    size_t count = (n == 0) ? ready_queue_.size() : std::min(n, ready_queue_.size());

    for (size_t i = 0; i < count; ++i) {
      T node = ready_queue_.front();
      ready_queue_.pop();
      processing_.insert(node);
      result.push_back(node);
    }

    return result;
  }

  // TODO: target C++23 and co_yield to a generator?

  /// Mark nodes as completed and update ready queue
  /// @param nodes Vector of nodes that have been processed
  template <std::ranges::input_range Range>
  void mark_done(Range &&nodes) {
    if (!prepared_) {
      throw std::runtime_error("TopologicalSorter.mark_done() called before prepare()");
    }

    for (const auto &node : nodes) {
      if (processing_.find(node) == processing_.end()) {
        throw std::runtime_error("Node was not being processed");
      }

      processing_.erase(node);

      // Remove this node from incoming edges of its successors
      auto it = graph_.find(node);
      if (it != graph_.end()) {
        for (const auto &successor : it->second) {
          incoming_edges_[successor].erase(node);

          // If successor has no more dependencies, add to ready queue
          if (incoming_edges_[successor].empty() && processing_.find(successor) == processing_.end()) {
            ready_queue_.push(successor);
          }
        }
      }
    }
  }

  void mark_done(NodeSet const &nodes) { mark_done(nodes | std::views::all); }

  /// Get a complete topological ordering of the graph
  /// @return Vector containing all nodes in topological order
  std::vector<T> static_order() {
    if (prepared_) {
      throw std::runtime_error("TopologicalSorter.static_order() called after prepare()");
    }

    // Create a copy to work with
    TopologicalSorter copy = *this;
    copy.prepare();

    std::vector<T> result;

    while (!copy.done()) {
      auto ready = copy.get_ready();
      if (ready.empty()) {
        throw CycleError("Graph contains a cycle");
      }

      result.insert(result.end(), ready.begin(), ready.end());
      copy.mark_done(ready);
    }

    return result;
  }

  /// Get the number of nodes in the graph
  size_t size() const { return graph_.size(); }

  /// Check if the graph is empty
  bool empty() const { return graph_.empty(); }

  /// Clear the graph
  void clear() {
    graph_.clear();
    incoming_edges_.clear();
    ready_queue_ = std::queue<T>{};
    processing_.clear();
    prepared_ = false;
  }

  /// Get all nodes in the graph
  auto nodes() const { return std::views::keys(graph_); }

  /// Check if a node exists in the graph
  bool contains(const T &node) const { return graph_.find(node) != graph_.end(); }

  /// Get the dependencies of a node
  auto dependencies(const T &node) const {
    auto it = incoming_edges_.find(node);
    if (it != incoming_edges_.end()) {
      return it->second | std::views::all;
    } else {
      return empty_set() | std::views::all;
    }
  }

  /// Get the successors of a node
  auto successors(const T &node) const {
    auto it = graph_.find(node);
    if (it != graph_.end()) {
      return it->second | std::views::all;
    } else {
      return empty_set() | std::views::all;
    }
  }
};

/// Convenience function to perform topological sort on a graph
/// @param graph Map where keys are nodes and values are sets of their dependencies
/// @return Vector containing all nodes in topological order
template <typename T, typename Hash = std::hash<T>, typename Equal = std::equal_to<T>>
std::vector<T> topological_sort(const std::unordered_map<T, std::unordered_set<T, Hash, Equal>, Hash, Equal> &graph) {
  TopologicalSorter<T, Hash, Equal> sorter(graph);
  return sorter.static_order();
}

} // namespace opflow
