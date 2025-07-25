#pragma once

#include <queue>
#include <unordered_map>
#include <vector>

#include "graph.hpp"
#include "impl/flat_multivect.hpp"
#include "impl/iterator.hpp"

namespace opflow {
template <typename T>
class topo_graph {
  impl::flat_multivect<size_t> pred_map; ///< Flattened storage of predecessors: id -> [pred ids]
  std::vector<T> sorted;                 ///< Sorted nodes in topological order: id -> node

public:
  /**
   * @brief Construct a topological sorted graph from a directed graph
   *
   * @param g a directed graph
   */
  template <typename H, typename E>
  topo_graph(graph<T, H, E> const &g) : pred_map(), sorted() {
    // Perform a topological sort on the input graph
    // and populate the sorted vector and preds structure

    std::unordered_map<T, size_t, H, E> in_degree, sorted_id;
    std::queue<T> ready;

    in_degree.reserve(g.size());
    sorted.reserve(g.size());
    sorted_id.reserve(g.size());

    for (auto const &[node, pred] : g.predecessors()) {
      auto n_pred = pred.size();
      in_degree.emplace(node, n_pred);
      if (n_pred == 0) {
        ready.push(node);
      }
    }

    while (!ready.empty()) {
      T current = std::move(ready.front());
      ready.pop();
      sorted.push_back(current);

      // update successors
      auto succ_it = g.successors().find(current);
      assert(succ_it != g.successors().end() && "[BUG] Node not found in successors map.");
      for (auto const &succ : succ_it->second) {
        if (--in_degree[succ] == 0) {
          ready.push(succ);
        }
      }
    }

    if (sorted.size() != g.size()) {
      sorted.clear();
      throw std::runtime_error("Graph contains a cycle");
    }

    // Build the sorted_id mapping first
    for (size_t i = 0; i < sorted.size(); ++i) {
      sorted_id[sorted[i]] = i;
    }

    // Then build the predecessors map
    std::vector<size_t> tmp;
    for (size_t i = 0; i < sorted.size(); ++i) {
      tmp.clear();
      for (auto const &pred : g.pred_of(sorted[i])) {
        tmp.push_back(sorted_id[pred]);
      }
      auto test_id = pred_map.push_back(tmp);
      assert(test_id == i && "[BUG] Preds ID mismatch when constructing preds map.");
    }
  }

  /**
   * @brief Get the number of nodes in the graph
   *
   * @return size_t
   */
  size_t size() const noexcept { return sorted.size(); }

  /**
   * @brief Check if the graph is empty
   *
   * @return true if the graph contains no nodes, false otherwise
   */
  bool empty() const noexcept { return sorted.empty(); }

  /**
   * @brief Check if a node exists in the graph
   *
   * @param node_id ID of the node to check
   * @return true if the node exists, false otherwise
   */
  bool contains_id(size_t node_id) const noexcept { return node_id < size(); }

  /**
   * @brief Check if a node exists in the graph by value
   *
   * @param node The node to check
   * @return true if the node exists, false otherwise
   */
  bool contains_node(T const &node) const { return std::ranges::find(sorted, node) != sorted.end(); }

  /**
   * @brief Get the node at a specific index
   *
   * @param id The index in topological order (0-based)
   * @return T const& The node at the given index
   */
  T const &operator[](size_t id) const noexcept { return sorted[id]; }

  /**
   * @brief Get the predecessors of a node by index
   *
   * @param id The index of the node whose predecessors to retrieve
   * @return std::span<const size_t> A span of the predecessor id for the node at the given index
   */
  auto preds(size_t id) const noexcept { return pred_map[id]; }

  /**
   * @brief Check if a node is a root node
   *
   * @param id The index of the node to check
   * @return true if the node is a root node, false otherwise
   */
  bool is_root(size_t id) const noexcept {
    // A root node has no predecessors
    return pred_map[id].empty();
  }

  /**
   * @brief Get the root IDs in the graph
   *
   * @note O(V) operation
   *
   * @return a vector of root node indices
   */
  auto root_ids() const noexcept {
    std::vector<size_t> ids;
    for (size_t i = 0; i < sorted.size(); ++i) {
      if (is_root(i)) {
        ids.push_back(i);
      }
    }
    return ids;
  }

  /**
   * @brief Get all root nodes in the graph
   *
   * @note O(V) operation and copies nodes
   *
   * @return a vector of root nodes
   */
  auto get_roots() const {
    std::vector<T> roots;
    for (size_t i = 0; i < sorted.size(); ++i) {
      if (is_root(i)) {
        roots.push_back(sorted[i]);
      }
    }
    return roots;
  }

  /**
   * @brief Check if a node is a leaf node
   *
   * @note O(V+E) operation
   *
   * @param id The index of the node to check
   * @return true if the node is a leaf node, false otherwise
   */
  bool is_leaf(size_t id) const noexcept {
    auto flat = pred_map.flat();
    return std::ranges::find(flat, id) == flat.end();
  }

  /**
   * @brief Get the leaf IDs in the graph
   *
   * @note O(V+E) operation
   *
   * @return a vector of leaf node indices
   */
  auto leaf_ids() const noexcept {
    std::vector<size_t> ids;
    std::vector<bool> is_leaf(sorted.size(), true);
    for (auto id : pred_map.flat()) {
      is_leaf[id] = false; // Mark all nodes that have successors as not leaf
    }
    for (size_t i = 0; i < sorted.size(); ++i) {
      if (is_leaf[i]) {
        ids.push_back(i);
      }
    }
    return ids;
  }

  /**
   * @brief Get all leaf nodes in the graph
   *
   * @note O(V+E) operation and copies nodes
   *
   * @return a vector of leaf nodes
   */
  auto get_leaves() const {
    std::vector<T> leaves;
    std::vector<bool> is_leaf(sorted.size(), true);
    for (auto id : pred_map.flat()) {
      is_leaf[id] = false; // Mark all nodes that have successors as not leaf
    }
    for (size_t i = 0; i < sorted.size(); ++i) {
      if (is_leaf[i]) {
        leaves.push_back(sorted[i]);
      }
    }
    return leaves;
  }

  // iterators

  using const_iterator = impl::iterator_t<topo_graph, true>;
  const_iterator begin() const noexcept { return const_iterator{this, 0}; }
  const_iterator end() const noexcept { return const_iterator{this, size()}; }
};

// Deduction guide
template <typename T, typename Hash, typename Equal>
topo_graph(graph<T, Hash, Equal> const &g) -> topo_graph<T>;

} // namespace opflow
