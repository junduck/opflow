#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "impl/flat_set.hpp"

namespace opflow {

template <std::copy_constructible T, typename Hash = std::hash<T>, typename Equal = std::equal_to<T>>
class graph {
public:
  /// @brief Type alias for a set of nodes
  using NodeSet = opflow::impl::flat_set<T>;

  /// @brief Type alias for a map from nodes to sets of nodes
  using NodeMap = std::unordered_map<T, NodeSet, Hash, Equal>;

  /**
   * @brief Add a vertex with specified predecessors
   *
   * Adds a node to the graph along with its predecessors. If the node already exists,
   * the new predecessors are added to its existing predecessor set.
   *
   * @tparam R A forward range type containing elements of type T
   * @param node The node to add to the graph
   * @param preds A range of predecessors that this node has
   *
   * @note If any predecessor doesn't exist in the graph, it will be created automatically
   * @note node is copy constructed into the graph, user should consider value semantics or
   * shared ownership if needed
   *
   * Example:
   * @code
   * std::vector<std::string> preds = {"dep1", "dep2"};
   * sorter.add_vertex("mynode", preds);
   * @endcode
   */
  template <std::ranges::forward_range R>
  void add_vertex(T const &node, R &&preds) {
    // Ensure the node exists in both graphs
    if (adj.find(node) == adj.end()) {
      adj.emplace(node, NodeSet{});
    }
    if (reverse_adj.find(node) == reverse_adj.end()) {
      reverse_adj.emplace(node, NodeSet{});
    }

    // Add predecessors
    for (auto const &pred : preds) {
      // Ensure predecessor exists in both graphs
      if (adj.find(pred) == adj.end()) {
        adj.emplace(pred, NodeSet{});
      }
      if (reverse_adj.find(pred) == reverse_adj.end()) {
        reverse_adj.emplace(pred, NodeSet{});
      }
      adj[node].insert(pred);
      reverse_adj[pred].insert(node);
    }
  }

  /**
   * @brief Add a vertex with no predecessors
   *
   * Convenience method to add a node that doesn't depend on any other nodes.
   * This node can serve as a starting point in the topological order.
   *
   * @param node The node to add to the graph
   */
  void add_vertex(T const &node) { add_vertex(node, std::vector<T>{}); }

  /**
   * @brief Remove a vertex from the graph
   *
   * Removes the specified node from the graph along with all edges connecting
   * to or from this node. This includes both incoming and outgoing edges.
   *
   * @param node The node to remove from the graph
   *
   * @note If the node doesn't exist, this operation has no effect
   *
   */
  void rm_vertex(T const &node) {
    auto graph_it = adj.find(node);
    if (graph_it == adj.end()) {
      return; // Node doesn't exist
    }

    // Remove this node from all successors
    for (auto const &succ : reverse_adj[node]) {
      adj[succ].erase(node);
    }

    // Remove this node from all predecessors
    for (auto const &pred : adj[node]) {
      reverse_adj[pred].erase(node);
    }

    // Remove the node from both graphs
    adj.erase(node);
    reverse_adj.erase(node);
  }

  /**
   * @brief Remove specific predecessors from a node
   *
   * Removes the specified predecessor edges from a node. The node itself
   * and the dependency nodes remain in the graph.
   *
   * @tparam R A forward range type containing elements of type T
   * @param node The node to remove predecessors from
   * @param preds A range of predecessors to remove
   *
   * @note If the node or any predecessor doesn't exist, those operations are ignored
   * @note Only the specific predecessor edges are removed, not the nodes themselves
   */
  template <std::ranges::forward_range R>
  void rm_edge(T const &node, R &&preds) {
    auto graph_it = adj.find(node);
    if (graph_it == adj.end()) {
      return; // Node doesn't exist
    }

    for (auto const &pred : preds) {
      // Remove the predecessor relationship
      adj[node].erase(pred);

      if (auto reverse_it = reverse_adj.find(pred); reverse_it != reverse_adj.end()) {
        reverse_it->second.erase(node);
      }
    }
  }

  /**
   * @brief Get the number of nodes in the graph
   *
   * @return The total number of nodes currently in the graph
   */
  size_t size() const { return adj.size(); }

  /**
   * @brief Check if the graph is empty
   *
   * @return true if the graph contains no nodes, false otherwise
   */
  bool empty() const { return adj.empty(); }

  /**
   * @brief Clear the graph
   *
   * Removes all nodes and edges from the graph, leaving it in an empty state.
   * After calling this method, size() will return 0.
   */
  void clear() {
    adj.clear();
    reverse_adj.clear();
  }

  /**
   * @brief Check if a node exists in the graph
   *
   * @param node The node to search for
   * @return true if the node exists in the graph, false otherwise
   */
  bool contains(T const &node) const { return adj.find(node) != adj.end(); }

  /**
   * @brief Get the predecessors of a node
   *
   * Returns the set of nodes that the specified node depends on (its predecessors).
   * These are the nodes that must be processed before the specified node in a
   * topological ordering.
   *
   * @param node The node to get predecessors for
   * @return A const reference to the set of predecessors, or an empty set if the node doesn't exist
   *
   * @note The returned reference remains valid until the graph is modified
   */
  NodeSet const &pred_of(T const &node) const {
    static NodeSet empty_set;
    auto it = adj.find(node);
    return (it != adj.end()) ? it->second : empty_set;
  }

  auto const &predecessors() const { return adj; }

  /**
   * @brief Get the successors of a node
   *
   * Returns the set of nodes that depend on the specified node (its successors).
   * These are the nodes that must be processed after the specified node in a
   * topological ordering.
   *
   * @param node The node to get successors for
   * @return A const reference to the set of successors, or an empty set if the node doesn't exist
   *
   * @note The returned reference remains valid until the graph is modified
   */
  NodeSet const &succ_of(T const &node) const {
    static NodeSet empty_set;
    auto it = reverse_adj.find(node);
    return (it != reverse_adj.end()) ? it->second : empty_set;
  }

  auto const &successors() const { return reverse_adj; }

  bool is_root(T const &node) const {
    // A root node has no predecessors
    auto it = adj.find(node);
    return (it != adj.end() && it->second.empty());
  }

  bool is_leaf(T const &node) const {
    // A leaf node has no successors
    auto it = reverse_adj.find(node);
    return (it != reverse_adj.end() && it->second.empty());
  }

  auto get_roots() const {
    std::vector<T> roots;
    for (auto const &[node, preds] : adj) {
      if (preds.empty()) {
        roots.push_back(node);
      }
    }
    return roots;
  }

  auto get_leaves() const {
    std::vector<T> leaves;
    for (auto const &[node, succs] : reverse_adj) {
      if (succs.empty()) {
        leaves.push_back(node);
      }
    }
    return leaves;
  }

protected:
  NodeMap adj;         ///< Adjacency list: node -> [predecessors] i.e. set of nodes that it depends on
  NodeMap reverse_adj; ///< Reverse adjacency list: node -> [successors] i.e. set of nodes that depend on it
};

} // namespace opflow
