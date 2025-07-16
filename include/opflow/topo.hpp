#pragma once

#include <algorithm>
#include <functional>
#include <queue>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace opflow {

/**
 * @brief A generic topological sorter for directed acyclic graphs (DAGs)
 *
 * This class provides functionality to build a directed graph and perform
 * topological sorting using Kahn's algorithm. It maintains both forward
 * and reverse adjacency lists for efficient operations.
 *
 * @tparam T The type of nodes in the graph
 * @tparam Hash Hash function for type T (defaults to std::hash<T>)
 * @tparam Equal Equality comparison for type T (defaults to std::equal_to<T>)
 *
 * @note The graph must be acyclic for topological sorting to work correctly.
 *       If a cycle is detected, the sort() method returns an empty vector.
 *
 * Example usage:
 * @code
 * topological_sorter<std::string> sorter;
 * sorter.add_vertex("task1", std::vector<std::string>{"dependency1", "dependency2"});
 * sorter.add_vertex("task2", std::vector<std::string>{"task1"});
 * auto sorted = sorter.sort(); // Returns topologically sorted order
 * @endcode
 */
template <typename T, typename Hash = std::hash<T>, typename Equal = std::equal_to<T>>
class topological_sorter {
public:
  /// @brief Type alias for a set of nodes
  using NodeSet = std::unordered_set<T, Hash, Equal>;
  /// @brief Type alias for a map from nodes to sets of nodes
  using NodeMap = std::unordered_map<T, NodeSet, Hash, Equal>;

private:
  NodeMap graph;         ///< Adjacency list: node -> set of nodes it depends on (predecessors)
  NodeMap reverse_graph; ///< Reverse adjacency list: node -> set of nodes that depend on it (successors)

public:
  /**
   * @brief Default constructor
   *
   * Creates an empty topological sorter with no nodes or edges.
   */
  topological_sorter() = default;

  /**
   * @brief Add a vertex with specified dependencies
   *
   * Adds a node to the graph along with its dependencies. If the node already exists,
   * the new dependencies are added to its existing dependency set.
   *
   * @tparam R A forward range type containing elements of type T
   * @param node The node to add to the graph
   * @param deps A range of dependencies that this node depends on
   *
   * @note If any dependency doesn't exist in the graph, it will be created automatically
   *
   * Example:
   * @code
   * std::vector<std::string> deps = {"dep1", "dep2"};
   * sorter.add_vertex("mynode", deps);
   * @endcode
   */
  template <std::ranges::forward_range R>
  void add_vertex(T const &node, R &&deps) {
    // Ensure the node exists in both graphs
    if (graph.find(node) == graph.end()) {
      graph[node] = NodeSet{};
    }
    if (reverse_graph.find(node) == reverse_graph.end()) {
      reverse_graph[node] = NodeSet{};
    }

    // Add dependencies
    for (auto const &dep : deps) {
      // Ensure dependency exists in both graphs
      if (graph.find(dep) == graph.end()) {
        graph[dep] = NodeSet{};
      }
      if (reverse_graph.find(dep) == reverse_graph.end()) {
        reverse_graph[dep] = NodeSet{};
      }

      // Add the dependency relationship
      graph[node].insert(dep);
      reverse_graph[dep].insert(node);
    }
  }

  /**
   * @brief Add a vertex with no dependencies
   *
   * Convenience method to add a node that doesn't depend on any other nodes.
   * This node can serve as a starting point in the topological order.
   *
   * @param node The node to add to the graph
   */
  void add_vertex(T const &node) { add_vertex(node, std::vector<T>{}); }

  /**
   * @brief Remove a vertex and all its connections
   *
   * Removes the specified node from the graph along with all edges connecting
   * to or from this node. This includes both incoming and outgoing edges.
   *
   * @param node The node to remove from the graph
   *
   * @note If the node doesn't exist, this operation has no effect
   *
   * @warning Removing a node will affect the topological ordering of remaining nodes
   */
  void rm_vertex(T const &node) {
    auto graph_it = graph.find(node);
    if (graph_it == graph.end()) {
      return; // Node doesn't exist
    }

    // Remove this node from all nodes that depend on it
    for (auto const &dependent : reverse_graph[node]) {
      graph[dependent].erase(node);
    }

    // Remove this node from all nodes it depends on
    for (auto const &dependency : graph[node]) {
      reverse_graph[dependency].erase(node);
    }

    // Remove the node from both graphs
    graph.erase(node);
    reverse_graph.erase(node);
  }

  /**
   * @brief Add dependencies to an existing node
   *
   * Adds new dependency edges to an existing node. If the node doesn't exist,
   * it will be created. This is useful for incrementally building the dependency graph.
   *
   * @tparam R A forward range type containing elements of type T
   * @param node The node to add dependencies to
   * @param deps A range of dependencies to add
   *
   * @note If any dependency doesn't exist, it will be created automatically
   * @note Duplicate dependencies are automatically handled (no duplicate edges)
   */
  template <std::ranges::forward_range R>
  void add_edge(T const &node, R &&deps) {
    // Ensure the node exists
    if (graph.find(node) == graph.end()) {
      add_vertex(node);
    }

    // Add each dependency
    for (auto const &dep : deps) {
      // Ensure dependency exists
      if (graph.find(dep) == graph.end()) {
        add_vertex(dep);
      }

      // Add the dependency relationship
      graph[node].insert(dep);
      reverse_graph[dep].insert(node);
    }
  }

  /**
   * @brief Remove specific dependencies from a node
   *
   * Removes the specified dependency edges from a node. The node itself
   * and the dependency nodes remain in the graph.
   *
   * @tparam R A forward range type containing elements of type T
   * @param node The node to remove dependencies from
   * @param deps A range of dependencies to remove
   *
   * @note If the node or any dependency doesn't exist, those operations are ignored
   * @note Only the specific dependency edges are removed, not the nodes themselves
   */
  template <std::ranges::forward_range R>
  void rm_edge(T const &node, R &&deps) {
    auto graph_it = graph.find(node);
    if (graph_it == graph.end()) {
      return; // Node doesn't exist
    }

    for (auto const &dep : deps) {
      // Remove the dependency relationship
      graph[node].erase(dep);

      auto reverse_it = reverse_graph.find(dep);
      if (reverse_it != reverse_graph.end()) {
        reverse_it->second.erase(node);
      }
    }
  }

  /**
   * @brief Perform topological sort using Kahn's algorithm
   *
   * Computes a topological ordering of the nodes in the graph. A topological
   * ordering is a linear ordering of nodes such that for every directed edge
   * from node A to node B, A appears before B in the ordering.
   *
   * @return A vector containing nodes in topological order, or an empty vector if a cycle is detected
   *
   * @note This method uses Kahn's algorithm which has O(V + E) time complexity
   * @note The original graph structure is not modified (const method)
   * @note If the graph contains cycles, an empty vector is returned instead of throwing an exception
   *
   * @warning If a cycle exists in the graph, the returned vector will be empty.
   *          Check the size of the result against the graph size to detect cycles.
   *
   * Example:
   * @code
   * auto result = sorter.sort();
   * if (result.empty() && !sorter.empty()) {
   *     // Cycle detected
   * }
   * @endcode
   */
  std::vector<T> sort() const {
    if (graph.empty()) {
      return {};
    }

    // Create working copies of the graph structures
    std::unordered_map<T, size_t, Hash, Equal> in_degree_map;
    in_degree_map.reserve(graph.size());

    // Initialize in-degree count for all nodes
    for (auto const &[node, dependencies] : graph) {
      in_degree_map[node] = dependencies.size();
    }

    // Find all nodes with no incoming edges
    std::queue<T> zero_in_degree;
    for (auto const &[node, in_degree] : in_degree_map) {
      if (in_degree == 0) {
        zero_in_degree.push(node);
      }
    }

    std::vector<T> result;
    result.reserve(graph.size());

    // Process nodes with zero in-degree
    while (!zero_in_degree.empty()) {
      T current = std::move(zero_in_degree.front());
      zero_in_degree.pop();
      result.push_back(current);

      // For each node that depends on current
      auto reverse_it = reverse_graph.find(current);
      if (reverse_it != reverse_graph.end()) {
        for (auto const &dependent : reverse_it->second) {
          if (--in_degree_map[dependent] == 0) {
            zero_in_degree.push(dependent);
          }
        }
      }
    }

    if (result.size() != graph.size()) {
      // throw cycle_error("Graph contains a cycle");
      return {}; // Return empty vector if cycle detected
    }

    return result;
  }

  /**
   * @brief Get the number of nodes in the graph
   *
   * @return The total number of nodes currently in the graph
   */
  size_t size() const { return graph.size(); }

  /**
   * @brief Check if the graph is empty
   *
   * @return true if the graph contains no nodes, false otherwise
   */
  bool empty() const { return graph.empty(); }

  /**
   * @brief Clear the graph
   *
   * Removes all nodes and edges from the graph, leaving it in an empty state.
   * After calling this method, size() will return 0.
   */
  void clear() {
    graph.clear();
    reverse_graph.clear();
  }

  /**
   * @brief Check if a node exists in the graph
   *
   * @param node The node to search for
   * @return true if the node exists in the graph, false otherwise
   */
  bool contains(T const &node) const { return graph.find(node) != graph.end(); }

  /**
   * @brief Get the dependencies of a node
   *
   * Returns the set of nodes that the specified node depends on (its predecessors).
   * These are the nodes that must be processed before the specified node in a
   * topological ordering.
   *
   * @param node The node to get dependencies for
   * @return A const reference to the set of dependencies, or an empty set if the node doesn't exist
   *
   * @note The returned reference remains valid until the graph is modified
   */
  NodeSet const &dependencies(T const &node) const {
    static NodeSet empty_set;
    auto it = graph.find(node);
    return (it != graph.end()) ? it->second : empty_set;
  }

  /**
   * @brief Get the dependents of a node
   *
   * Returns the set of nodes that depend on the specified node (its successors).
   * These are the nodes that must be processed after the specified node in a
   * topological ordering.
   *
   * @param node The node to get dependents for
   * @return A const reference to the set of dependents, or an empty set if the node doesn't exist
   *
   * @note The returned reference remains valid until the graph is modified
   */
  NodeSet const &dependents(T const &node) const {
    static NodeSet empty_set;
    auto it = reverse_graph.find(node);
    return (it != reverse_graph.end()) ? it->second : empty_set;
  }

  /**
   * @brief Get all nodes in the graph
   *
   * Returns a view of all nodes currently in the graph. This is useful for
   * iterating over all nodes without caring about their dependencies.
   *
   * @return A view object that can be used to iterate over all nodes
   *
   * @note The returned view is invalidated when the graph is modified
   * @note This uses C++20 ranges and returns std::views::keys of the internal graph
   *
   * Example:
   * @code
   * for (const auto& node : sorter.nodes()) {
   *     std::cout << node << std::endl;
   * }
   * @endcode
   */
  auto nodes() const { return std::views::keys(graph); }
};

} // namespace opflow
