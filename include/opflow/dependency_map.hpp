#pragma once

#include <algorithm>
#include <cassert>
#include <ranges>
#include <span>
#include <vector>

namespace opflow {
/**
 * @brief A compact, topologically-sorted dependency map
 *
 * This data structure maintains dependencies between nodes in a directed acyclic graph (DAG).
 * Nodes are assigned sequential IDs starting from 0, and dependencies must reference previously
 * added nodes, ensuring the graph remains acyclic and topologically sorted.
 *
 */
class dependency_map {
  struct meta_type {
    size_t degree; ///< Number of dependencies for this node
    size_t offset; ///< Starting index in dependencies for this node

    constexpr meta_type() noexcept = default;
    constexpr meta_type(size_t d, size_t o) noexcept : degree(d), offset(o) {}
  };

private:
  std::vector<size_t> dependencies; ///< Flattened storage of all dependency IDs
  std::vector<meta_type> meta;      ///< Metadata for each node (degree and offset)

public:
  constexpr static size_t invalid_id = static_cast<size_t>(-1);

  /**
   * @brief Default constructor creates an empty dependency map
   */
  dependency_map() = default;

  /**
   * @brief Reserve capacity for nodes and dependencies
   * @param node_capacity Expected number of nodes
   * @param dependency_capacity Expected total number of dependencies
   */
  void reserve(size_t node_capacity, size_t dependency_capacity = 0) {
    meta.reserve(node_capacity);
    if (dependency_capacity > 0) {
      dependencies.reserve(dependency_capacity);
    }
  }

  /**
   * @brief Get the number of nodes in the dependency map
   * @return Number of nodes currently in the map
   */
  size_t size() const noexcept { return meta.size(); }

  /**
   * @brief Check if the dependency map is empty
   * @return true if no nodes have been added, false otherwise
   */
  bool empty() const noexcept { return meta.empty(); }

  /**
   * @brief Get the total number of stored dependencies across all nodes
   * @return Total number of dependencies
   */
  size_t total_dependencies() const noexcept { return dependencies.size(); }

  /**
   * @brief Check if a node ID exists in the map
   * @param node_id ID to check
   * @return true if the node exists, false otherwise
   */
  bool contains(size_t node_id) const noexcept { return node_id < size(); }

  /**
   * @brief Validate that a range of dependency IDs are valid for the next node
   *
   * Dependencies are valid if all IDs refer to previously added nodes (ID < current size).
   * This ensures the dependency graph remains acyclic and topologically sorted.
   *
   * @tparam R Range type that must satisfy std::ranges::forward_range
   * @param deps Range of dependency node IDs to validate
   * @return true if all dependencies are valid, false otherwise
   */
  template <std::ranges::forward_range R>
  bool validate(R &&deps) const noexcept {
    size_t const next_id = meta.size();
    return std::ranges::all_of(deps, [next_id](size_t dep_id) { return dep_id < next_id; });
  }

  /**
   * @brief Add a new node with its dependencies
   *
   * The new node will be assigned the next sequential ID (equal to current size()).
   * Dependencies must reference previously added nodes to maintain topological order.
   *
   * @tparam R Range type that must satisfy std::ranges::forward_range
   * @param deps Range of dependency node IDs
   * @return ID of the newly added node, or size_t(-1) if dependencies are invalid
   *
   * @note This function validates dependencies before adding. Use validate() first
   *       if you want to check validity without potentially adding the node.
   */
  template <std::ranges::forward_range R>
  size_t add(R &&deps) {
    if (!validate(deps)) {
      return invalid_id; // Invalid dependencies
    }

    const size_t node_id = meta.size();
    const size_t deps_count = static_cast<size_t>(std::ranges::size(deps));
    const size_t deps_offset = dependencies.size(); // Store current size as offset

    // Add dependencies to flattened storage
    dependencies.insert(dependencies.end(), std::ranges::begin(deps), std::ranges::end(deps));

    // Update metadata - offset should be the START position, not end
    meta.emplace_back(deps_count, deps_offset);

    return node_id;
  }

  /**
   * @brief Get the dependencies for a specific node
   *
   * @param node_id ID of the node whose dependencies to retrieve
   * @return A span of the node's dependencies
   *
   * @pre node_id < size()
   */
  std::span<const size_t> get_dependencies(size_t node_id) const noexcept {
    assert(node_id < size() && "Node ID out of bounds");
    auto [degree, offset] = meta[node_id];
    return std::span<const size_t>(dependencies).subspan(offset, degree);
  }

  /**
   * @brief Get the number of dependencies for a specific node
   * @param node_id ID of the node
   * @return Number of dependencies for the node
   * @pre node_id < size()
   */
  size_t get_degree(size_t node_id) const noexcept {
    assert(node_id < size() && "Node ID out of bounds");
    return meta[node_id].degree;
  }

  /**
   * @brief Check if a node has any dependencies
   * @param node_id ID of the node to check
   * @return true if the node has no dependencies, false otherwise
   * @pre node_id < size()
   */
  bool is_root(size_t node_id) const noexcept {
    assert(node_id < size() && "Node ID out of bounds");
    return meta[node_id].degree == 0;
  }

  /**
   * @brief Get all nodes that have no dependencies (root nodes)
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
   * @brief Get all leaf nodes (nodes with no incoming dependencies from other nodes)
   * @return Vector of leaf node IDs
   */
  std::vector<size_t> get_leaves() const {
    std::vector<bool> has_dependents(meta.size(), false);

    // Mark nodes that have dependents
    for (size_t dep : dependencies) {
      has_dependents[dep] = true;
    }

    std::vector<size_t> leafs;
    leafs.reserve(size());
    for (size_t i = 0; i < has_dependents.size(); ++i) {
      if (!has_dependents[i]) {
        leafs.push_back(i);
      }
    }
    return leafs;
  }

  /**
   * @brief Get all dependents of a node (nodes that depend on this node)
   * @param node_id ID of the node
   * @return Vector of node IDs that depend on the given node
   * @note This is O(n) operation where n is the total number of dependencies
   */
  std::vector<size_t> get_dependents(size_t node_id) const {
    assert(node_id < size() && "Node ID out of bounds");
    std::vector<size_t> dependents;

    for (size_t i = 0; i < size(); ++i) {
      auto deps = get_dependencies(i);
      if (std::ranges::find(deps, node_id) != deps.end()) {
        dependents.push_back(i);
      }
    }
    return dependents;
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
      for (size_t dep : get_dependencies(current)) {
        if (lambda_self(dep, lambda_self))
          return true;
      }
      return false;
    };

    return dfs(node_a, dfs);
  }

  /**
   * @brief Clear all data from the dependency map
   */
  void clear() noexcept {
    dependencies.clear();
    meta.clear();
  }

  /**
   * @brief Get statistics about the dependency map
   */
  struct statistics {
    size_t node_count;
    size_t total_dependencies;
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
      size_t deg = get_degree(i);
      max_deg = std::max(max_deg, deg);
      if (deg == 0)
        ++root_cnt;
    }

    return {.node_count = size(),
            .total_dependencies = total_dependencies(),
            .max_degree = max_deg,
            .avg_degree = static_cast<double>(total_dependencies()) / size(),
            .root_count = root_cnt,
            .leaf_count = get_leaves().size()};
  }
};

} // namespace opflow
