#pragma once

#include "impl/flat_multivect.hpp"

namespace opflow {

/// @brief Invalid node ID
constexpr inline size_t invalid_id = static_cast<size_t>(-1);

/**
 * @brief A compact, flattened topologically-sorted dependency map
 *
 * This data structure maintains dependencies between nodes in a directed acyclic graph (DAG).
 * Nodes are assigned sequential IDs starting from 0, and dependencies must reference previously
 * added nodes, ensuring the graph remains acyclic and topologically sorted.
 *
 */
class flat_graph {
  impl::flat_multivect<size_t> graph; ///< Flattened storage of dependencies

public:
  /**
   * @brief Default constructor creates an empty dependency map
   */
  flat_graph() = default;

  /**
   * @brief Get the number of nodes in the dependency map
   * @return Number of nodes currently in the map
   */
  size_t size() const noexcept { return graph.size(); }

  /**
   * @brief Check if the dependency map is empty
   * @return true if no nodes have been added, false otherwise
   */
  bool empty() const noexcept { return graph.empty(); }

  /**
   * @brief Check if a node ID exists in the map
   * @param node_id ID to check
   * @return true if the node exists, false otherwise
   */
  bool contains(size_t node_id) const noexcept { return node_id < size(); }

  /**
   * @brief Validate that a range of predecessor IDs are valid for the next node
   *
   * Predecessors are valid if all IDs refer to previously added nodes (ID < current size).
   * This ensures the dependency graph remains acyclic and topologically sorted.
   *
   * @tparam R Range type that must satisfy std::ranges::forward_range
   * @param preds Range of predecessor node IDs to validate
   * @return true if all predecessors are valid, false otherwise
   */
  template <std::ranges::forward_range R>
  bool validate(R &&preds) const noexcept {
    size_t const next_id = size();
    return std::ranges::all_of(preds, [next_id](size_t pred_id) { return pred_id < next_id; });
  }

  /**
   * @brief Add a new node with its predecessors
   *
   * The new node will be assigned the next sequential ID (equal to current size()).
   * Predecessors must reference previously added nodes to maintain topological order.
   *
   * @tparam R Range type that must satisfy std::ranges::forward_range
   * @param preds Range of predecessor node IDs
   * @return ID of the newly added node, or size_t(-1) if predecessors are invalid
   *
   * @note This function validates predecessors before adding. Use validate() first
   *       if you want to check validity without potentially adding the node.
   */
  template <std::ranges::forward_range R>
  size_t add(R &&preds) {
    if (!validate(preds)) {
      return invalid_id; // Invalid predecessors
    }
    return graph.push_back(std::forward<R>(preds));
  }

  /**
   * @brief Get the predecessors for a specific node
   *
   * @param node_id ID of the node whose predecessors to retrieve
   * @return A span of the node's predecessors
   *
   * @pre node_id < size()
   */
  auto get_predecessors(size_t node_id) const noexcept {
    assert(node_id < size() && "Node ID out of bounds");
    return graph[node_id];
  }

  /**
   * @brief Get the number of predecessors for a specific node
   * @param node_id ID of the node
   * @return Number of predecessors for the node
   * @pre node_id < size()
   */
  size_t num_predecessors(size_t node_id) const noexcept {
    assert(node_id < size() && "Node ID out of bounds");
    return graph.size(node_id);
  }

  /**
   * @brief Get the total number of stored predecessors across all nodes
   * @return Total number of predecessors
   */
  size_t total_predecessors() const noexcept { return graph.total_size(); }

  /**
   * @brief Check if a node has any predecessors
   * @param node_id ID of the node to check
   * @return true if the node has no predecessors, false otherwise
   * @pre node_id < size()
   */
  bool is_root(size_t node_id) const noexcept {
    assert(node_id < size() && "Node ID out of bounds");
    return graph.size(node_id) == 0;
  }

  /**
   * @brief Get all nodes that have no predecessors (root nodes)
   * @return Vector of node IDs that are roots
   */
  std::vector<size_t> get_roots() const {
    std::vector<size_t> roots;
    roots.reserve(size());
    for (size_t i = 0; i < size(); ++i) {
      if (is_root(i)) {
        roots.push_back(i);
      }
    }
    return roots;
  }

  /**
   * @brief Get all leaf nodes (nodes with no successor nodes)
   * @return Vector of leaf node IDs
   */
  std::vector<size_t> get_leaves() const {
    std::vector<bool> has_succ(size(), false);

    // Mark nodes that have dependents
    for (size_t dep : graph.flat()) {
      has_succ[dep] = true;
    }

    std::vector<size_t> leaves;
    leaves.reserve(size());
    for (size_t i = 0; i < has_succ.size(); ++i) {
      if (!has_succ[i]) {
        leaves.push_back(i);
      }
    }
    return leaves;
  }

  /**
   * @brief Get all successors of a node
   * @param node_id ID of the node
   * @return Vector of node IDs that depend on the given node
   * @note This is O(n) operation where n is the total number of predecessors
   */
  std::vector<size_t> get_successors(size_t node_id) const {
    assert(node_id < size() && "Node ID out of bounds");
    std::vector<size_t> successors;

    for (size_t i = 0; i < size(); ++i) {
      auto preds = get_predecessors(i);
      if (std::ranges::find(preds, node_id) != preds.end()) {
        successors.push_back(i);
      }
    }
    return successors;
  }

  /**
   * @brief Check if node A depends on node B (directly or indirectly)
   * @param node_a ID of the potentially dependent node
   * @param node_b ID of the potentially depended-upon node
   * @return true if node_a depends on node_b, false otherwise
   * @note This performs a depth-first search and may be expensive for large graphs
   */
  bool depends_on(size_t node_a, size_t node_b) const {
    assert(node_a < size() && "Node A ID out of bounds");
    assert(node_b < size() && "Node B ID out of bounds");

    if (node_a == node_b)
      return false;

    std::vector<bool> visited(size(), false);

    auto dfs = [&](size_t current, auto &lambda_self) -> bool {
      if (current == node_b)
        return true;
      if (visited[current])
        return false;

      visited[current] = true;
      for (size_t pred : get_predecessors(current)) {
        if (lambda_self(pred, lambda_self))
          return true;
      }
      return false;
    };

    return dfs(node_a, dfs);
  }

  /**
   * @brief Clear all data from the dependency map
   */
  void clear() noexcept { graph.clear(); }

  void reserve(size_t n_nodes, size_t n_preds) { graph.reserve(n_nodes, n_preds); }

  /**
   * @brief Get statistics about the dependency map
   */
  struct statistics {
    size_t node_count;
    size_t total_predecessors;
    size_t max_degree;
    double avg_degree;
    size_t root_count;
    size_t leaf_count;
  };

  statistics get_statistics() const noexcept {
    if (empty()) {
      return {0, 0, 0, 0.0, 0, 0};
    }

    size_t max_deg = 0;
    size_t root_cnt = 0;

    for (size_t i = 0; i < size(); ++i) {
      size_t deg = num_predecessors(i);
      max_deg = std::max(max_deg, deg);
      if (deg == 0)
        ++root_cnt;
    }

    return {.node_count = size(),
            .total_predecessors = total_predecessors(),
            .max_degree = max_deg,
            .avg_degree = static_cast<double>(total_predecessors()) / size(),
            .root_count = root_cnt,
            .leaf_count = get_leaves().size()};
  }
};

} // namespace opflow
