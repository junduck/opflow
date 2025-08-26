#pragma once

#include <queue>
#include <unordered_map>
#include <vector>

#include "detail/flat_multivect.hpp"
#include "detail/iterator.hpp"
#include "graph.hpp"

namespace opflow {
template <typename T>
class graph_topo {
public:
  using node_type = T;

  struct arg_type {
    uint32_t node;
    uint32_t port;
  };

  /**
   * @brief Construct a topological sorted graph from a directed graph
   *
   * @param g a directed graph
   */
  template <typename H, typename E>
  graph_topo(graph<T, H, E> const &g) : pred_map(), sorted() {
    // Perform a topological sort on the input graph
    // and populate the sorted vector and preds structure

    std::unordered_map<T, size_t, H, E> in_degree, sorted_id;
    std::queue<T> ready;

    in_degree.reserve(g.size());
    sorted.reserve(g.size());
    sorted_id.reserve(g.size());

    for (auto const &[node, pred] : g.get_pred()) {
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
      auto succ_it = g.get_succ().find(current);
      assert(succ_it != g.get_succ().end() && "[BUG] Node not found in successors map.");
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
    std::vector<arg_type> tmp_args;
    for (size_t i = 0; i < sorted.size(); ++i) {
      tmp.clear();
      tmp_args.clear();
      for (auto const &pred : g.pred_of(sorted[i])) {
        tmp.push_back(sorted_id[pred]);
      }
      for (auto const &arg : g.args_of(sorted[i])) {
        tmp_args.emplace_back(sorted_id[arg.node], arg.port);
      }
      auto test_id = pred_map.push_back(tmp);
      auto test_args_id = arg_map.push_back(tmp_args);
      assert(test_id == i && "[BUG] Preds ID mismatch when constructing preds map.");
      assert(test_args_id == i && "[BUG] Args ID mismatch when constructing args map.");
    }
  }

  /**
   * @brief Get the number of nodes in the graph
   *
   * @return size_t
   */
  size_t size() const noexcept { return sorted.size(); }

  size_t num_edges() const noexcept { return arg_map.total_size(); }

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
   * @brief Get the ID of a node by value
   *
   * @param node The node to look up
   * @return size_t The ID of the node, of size() if not found
   */
  size_t id_of(T const &node) const {
    auto const it = std::ranges::find(sorted, node);
    return static_cast<size_t>(std::distance(sorted.begin(), it));
  }

  /**
   * @brief Get the predecessors of a node by index
   *
   * @param id The index of the node whose predecessors to retrieve
   * @return std::span<const size_t> A span of the predecessor id for the node at the given index
   */
  auto pred_of(size_t id) const noexcept { return pred_map[id]; }

  /**
   * @brief Get the arguments of a node by index
   *
   * @param id The index of the node whose arguments to retrieve
   * @return std::span<const arg_type> A span of the arguments for the node at the given index
   */
  auto args_of(size_t id) const noexcept { return arg_map[id]; }

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

  using const_iterator = detail::iterator_t<graph_topo, true>;
  const_iterator begin() const noexcept { return const_iterator{this, 0}; }
  const_iterator end() const noexcept { return const_iterator{this, size()}; }

private:
  detail::flat_multivect<size_t> pred_map;  ///< Flattened storage of predecessors: id -> [pred ids]
  detail::flat_multivect<arg_type> arg_map; ///< Flattened storage of arguments: id -> [pred:port]
  std::vector<T> sorted;                    ///< Sorted nodes in topological order: id -> node
};

// Deduction guide
template <typename T, typename Hash, typename Equal>
graph_topo(graph<T, Hash, Equal> const &g) -> graph_topo<T>;

} // namespace opflow
