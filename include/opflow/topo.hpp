#pragma once

#include <algorithm>
#include <functional>
#include <queue>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace opflow {

// Forward declaration
template <typename T, typename Hash, typename Equal>
class sorted_graph;

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
      graph.emplace(node, NodeSet{});
    }
    if (reverse_graph.find(node) == reverse_graph.end()) {
      reverse_graph.emplace(node, NodeSet{});
    }

    // Add dependencies
    for (auto const &dep : deps) {
      // Ensure dependency exists in both graphs
      if (graph.find(dep) == graph.end()) {
        graph.emplace(dep, NodeSet{});
      }
      if (reverse_graph.find(dep) == reverse_graph.end()) {
        reverse_graph.emplace(dep, NodeSet{});
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
   * @brief Convert the topological sorter to a sorted graph
   *
   * This method performs the topological sort and returns a sorted_graph object
   * that provides a container-compatible interface for accessing nodes in topological order.
   *
   * @return A sorted_graph containing nodes in topological order
   */
  auto make_sorted_graph() const -> sorted_graph<T, Hash, Equal> {
    auto sorted_nodes = sort();
    if (sorted_nodes.empty()) {
      return sorted_graph<T, Hash, Equal>{};
    }
    return sorted_graph<T, Hash, Equal>(*this, std::move(sorted_nodes));
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

  /**
   * @brief Get all root nodes in the graph
   *
   * Returns a vector of nodes that have no dependencies (i.e., they are not
   * dependent on any other nodes). These nodes can be considered as starting
   * points in the topological order.
   *
   * @return A vector containing all root nodes in the graph
   *
   * @note The returned vector is a copy and remains valid until the graph is modified
   */
  auto get_roots() const {
    std::vector<T> roots;
    for (auto const &[node, dependencies] : graph) {
      if (dependencies.empty()) {
        roots.push_back(node);
      }
    }
    return roots;
  }

  /**
   * @brief Get all leaf nodes in the graph
   *
   * Returns a vector of nodes that have no dependents (i.e., no other nodes
   * depend on them). These nodes can be considered as end points in the
   * topological order.
   *
   * @return A vector containing all leaf nodes in the graph
   *
   * @note The returned vector is a copy and remains valid until the graph is modified
   */
  auto get_leaves() const {
    std::vector<T> leaves;
    for (auto const &[node, dependents] : reverse_graph) {
      if (dependents.empty()) {
        leaves.push_back(node);
      }
    }
    return leaves;
  }
};

/**
 * @brief An immutable sorted graph with container-compatible interface
 *
 * This class represents a topologically sorted graph that provides
 * container-like access to nodes in topological order. It inherits from
 * topological_sorter but hides all mutating operations to ensure immutability.
 *
 * @tparam T The type of nodes in the graph
 * @tparam Hash Hash function for type T
 * @tparam Equal Equality comparison for type T
 */
template <typename T, typename Hash = std::hash<T>, typename Equal = std::equal_to<T>>
class sorted_graph : public topological_sorter<T, Hash, Equal> {
public:
  using base_type = topological_sorter<T, Hash, Equal>;
  using typename base_type::NodeMap;
  using typename base_type::NodeSet;

  /// @brief Value type for iterator - pair of node and its dependencies
  using value_type = std::pair<T const &, NodeSet const &>;
  /// @brief Size type for container interface
  using size_type = std::size_t;
  /// @brief Difference type for iterator arithmetic
  using difference_type = std::ptrdiff_t;

private:
  std::vector<T> sorted; ///< Nodes in topological order

  /// Hide all mutating methods from base class
  using base_type::add_edge;
  using base_type::add_vertex;
  using base_type::clear;
  using base_type::rm_edge;
  using base_type::rm_vertex;
  /// Already sorted, don't allow re-sorting
  using base_type::sort;

public:
  /**
   * @brief Iterator for sorted graph
   *
   * Random access const iterator that provides access to nodes in topological order.
   * Dereferences to a pair of (node, dependencies).
   */
  class const_iterator {
  private:
    sorted_graph const *graph;
    size_type index;

  public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = sorted_graph::value_type;
    using difference_type = sorted_graph::difference_type;
    using pointer = value_type *;
    using reference = value_type;

    const_iterator() : graph(nullptr), index(0) {}
    const_iterator(sorted_graph const *graph, size_type index) : graph(graph), index(index) {}

    reference operator*() const {
      T const &node = graph->sorted[index];
      return std::make_pair(std::cref(node), std::cref(graph->dependencies(node)));
    }

    const_iterator &operator++() {
      ++index;
      return *this;
    }
    const_iterator operator++(int) {
      auto tmp = *this;
      ++index;
      return tmp;
    }
    const_iterator &operator--() {
      --index;
      return *this;
    }
    const_iterator operator--(int) {
      auto tmp = *this;
      --index;
      return tmp;
    }

    const_iterator &operator+=(difference_type n) {
      index = static_cast<size_type>(static_cast<difference_type>(index) + n);
      return *this;
    }
    const_iterator &operator-=(difference_type n) {
      index = static_cast<size_type>(static_cast<difference_type>(index) - n);
      return *this;
    }
    const_iterator operator+(difference_type n) const {
      const_iterator tmp = *this;
      tmp += n;
      return tmp;
    }
    const_iterator operator-(difference_type n) const {
      const_iterator tmp = *this;
      tmp -= n;
      return tmp;
    }

    difference_type operator-(const const_iterator &other) const {
      return static_cast<difference_type>(index) - static_cast<difference_type>(other.index);
    }

    reference operator[](difference_type n) const {
      const_iterator tmp = *this;
      tmp += n;
      return *tmp;
    }

    auto operator<=>(const const_iterator &other) const = default;
  };

  using iterator = const_iterator; ///< Only const iteration is allowed

  /**
   * @brief Default constructor creates empty sorted graph
   */
  sorted_graph() = default;

  /**
   * @brief Construct sorted graph from base class and sorted nodes
   *
   * @param base The base topological_sorter to copy from
   * @param nodes Vector of nodes in topological order
   */
  sorted_graph(const base_type &base, std::vector<T> nodes) : base_type(base), sorted(std::move(nodes)) {}

  /**
   * @brief Get iterator to beginning of sorted nodes
   */
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator cbegin() const { return begin(); }

  /**
   * @brief Get iterator to end of sorted nodes
   */
  const_iterator end() const { return const_iterator(this, sorted.size()); }
  const_iterator cend() const { return end(); }

  /**
   * @brief Access node and dependencies by sorted index
   *
   * @param index The index in topological order (0-based)
   * @return Pair of (node, dependencies) at the given index
   */
  value_type operator[](size_type index) const {
    const T &node = sorted[index];
    return std::make_pair(std::cref(node), std::cref(this->dependencies(node)));
  }

  /**
   * @brief Access node and dependencies by sorted index with bounds checking
   *
   * @param index The index in topological order (0-based)
   * @return Pair of (node, dependencies) at the given index
   * @throws std::out_of_range if index is out of bounds
   */
  value_type at(size_type index) const {
    if (index >= sorted.size()) {
      throw std::out_of_range("Index out of range");
    }
    return (*this)[index];
  }

  /**
   * @brief Get the node at a specific sorted index
   *
   * @param index The index in topological order (0-based)
   * @return The node at the given index
   */
  T const &node_at(size_type index) const { return sorted[index]; }

  /**
   * @brief Get access to the sorted nodes vector
   *
   * @return Const reference to the vector of sorted nodes
   */
  std::vector<T> const &sorted_nodes() const { return sorted; }

  /**
   * @brief Get the first node and its dependencies
   * @throws std::runtime_error if the graph is empty
   */
  value_type front() const { return (*this)[0]; }

  /**
   * @brief Get the last node and its dependencies
   * @throws std::runtime_error if the graph is empty
   */
  value_type back() const { return (*this)[sorted.size() - 1]; }
};

} // namespace opflow
