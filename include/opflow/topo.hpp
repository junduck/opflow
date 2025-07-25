#pragma once

#include <algorithm>
#include <functional>
#include <queue>
#include <ranges>
#include <unordered_map>
#include <vector>

#include "graph.hpp"
#include "impl/iterator.hpp"
#include "opflow/dependency_map.hpp"

namespace opflow {
enum class colour {
  white, ///< Node has not been visited
  gray,  ///< Node is discovered but not visited yet
  black  ///< Node has been visited
};

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
class topological_sorter : public graph<T, Hash, Equal> {
public:
  using base = graph<T, Hash, Equal>;
  using typename base::NodeMap;
  using typename base::NodeSet;

  /// @brief Type alias for colour map used in BFS
  using colour_map_type = std::unordered_map<T, colour, Hash, Equal>;

  /// @brief Type alias for depth map used in BFS
  using depth_map_type = std::unordered_map<T, size_t, Hash, Equal>;

protected:
  using base::adj;
  using base::reverse_adj;

public:
  /**
   * @brief No-op visitor for BFS
   *
   * This is a default visitor that does nothing and always returns true.
   * It can be used when no specific action is needed during BFS traversal.
   */
  constexpr static auto noop_visitor = [](T const &, NodeMap const &, size_t) noexcept { return true; };

  /**
   * @brief Default constructor
   *
   * Creates an empty topological sorter with no nodes or edges.
   */
  topological_sorter() = default;

  /**
   * @brief Perform a breadth-first search (BFS)
   *
   * This method traverses the graph using BFS, allowing you to visit nodes
   * and their successors in a breadth-first manner. It provides hooks for
   * handling discovered nodes (gray) and visited nodes (black).
   *
   * @tparam Visitor A callable type that accepts a node, the graph, and optionally the depth
   * @param root The root node for the BFS traversal
   * @param visitor A callable that is invoked for each visited node
   * @param gray_handler Optional callable for handling gray nodes (discovered but not visited)
   * @param black_handler Optional callable for handling black nodes (visited)
   * @return A tuple containing:
   *         - colour_map: Maps each node to its colour state
   *         - depth_map: Maps each node to its relative depth (distance from the start node)
   */
  template <typename Visitor, typename GrayHandler, typename BlackHandler>
  auto bfs(T const &root, Visitor &&visitor, GrayHandler &&gray_handler, BlackHandler &&black_handler) const {
    colour_map_type colour_map{};
    depth_map_type depth_map{};
    std::queue<T> fifo{};

    // Check if start node exists in the graph
    if (reverse_adj.find(root) == reverse_adj.end()) {
      // Start node doesn't exist, return empty maps
      return std::make_tuple(std::move(colour_map), std::move(depth_map));
    }

    // Initialize all nodes as unvisited
    for (const auto &[node, _] : reverse_adj) {
      colour_map[node] = colour::white;
      // Only initialize depth for nodes we might visit - others remain uninitialized
    }

    colour_map[root] = colour::gray;
    depth_map[root] = 0; // root node has depth 0
    fifo.push(root);
    while (!fifo.empty()) {
      T current = std::move(fifo.front());
      fifo.pop();

      bool should_continue = true;

      // Discover successors (nodes that depend on current)
      if (auto it = reverse_adj.find(current); it != reverse_adj.end()) {
        for (auto const &successor : it->second) {
          switch (colour_map[successor]) {
          case colour::white:
            colour_map[successor] = colour::gray;          // Mark as discovered
            depth_map[successor] = depth_map[current] + 1; // Increment depth/distance
            fifo.push(successor);
            break;
          case colour::gray:
            // a gray node is discovered
            if constexpr (std::is_invocable_v<GrayHandler, T const &, NodeMap const &, size_t>) {
              should_continue = gray_handler(successor, std::cref(reverse_adj), depth_map[successor]);
            } else {
              should_continue = gray_handler(successor, std::cref(reverse_adj));
            }
            break;
          case colour::black:
            // a black node is discovered
            if constexpr (std::is_invocable_v<BlackHandler, T const &, NodeMap const &, size_t>) {
              should_continue = black_handler(successor, std::cref(reverse_adj), depth_map[successor]);
            } else {
              should_continue = black_handler(successor, std::cref(reverse_adj));
            }
            break;
          }
        }
      }

      if (!should_continue) {
        break; // Exit loop if handler returns false
      }

      // Visit the current node
      if constexpr (std::is_invocable_v<Visitor, T const &, NodeMap const &, size_t>) {
        should_continue = visitor(current, std::cref(reverse_adj), depth_map[current]);
      } else {
        should_continue = visitor(current, std::cref(reverse_adj));
      }
      colour_map[current] = colour::black; // Mark as visited

      if (!should_continue) {
        break; // if visitor returns false, stop the traversal
      }
    }

    return std::make_tuple(std::move(colour_map), std::move(depth_map));
  }

  template <typename Visitor>
  auto bfs(T const &root, Visitor &&visitor) const {
    return bfs(std::move(root), std::forward<Visitor>(visitor), noop_visitor, noop_visitor);
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
    if (adj.empty()) {
      return {};
    }

    // Create working copies of the graph structures
    std::unordered_map<T, size_t, Hash, Equal> in_degree_map;
    in_degree_map.reserve(adj.size());

    // Initialize in-degree count for all nodes
    for (auto const &[node, dependencies] : adj) {
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
    result.reserve(adj.size());

    // Process nodes with zero in-degree
    while (!zero_in_degree.empty()) {
      T current = std::move(zero_in_degree.front());
      zero_in_degree.pop();
      result.push_back(current);

      // For each node that depends on current
      auto reverse_it = reverse_adj.find(current);
      if (reverse_it != reverse_adj.end()) {
        for (auto const &dependent : reverse_it->second) {
          if (--in_degree_map[dependent] == 0) {
            zero_in_degree.push(dependent);
          }
        }
      }
    }

    if (result.size() != adj.size()) {
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
  auto nodes() const { return std::views::keys(adj); }

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
    for (auto const &[node, dependencies] : adj) {
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
    for (auto const &[node, dependents] : reverse_adj) {
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
  std::vector<T> sorted;                            ///< Nodes in topological order
  std::unordered_map<T, size_t, Hash, Equal> index; ///< Maps nodes to their sorted index

  /// Hide all mutating methods from base class
  using base_type::add_vertex;
  using base_type::clear;
  using base_type::rm_edge;
  using base_type::rm_vertex;
  /// Already sorted, don't allow re-sorting
  using base_type::sort;

public:
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
  sorted_graph(base_type const &base, std::vector<T> nodes) : base_type(base), sorted(std::move(nodes)) {
    index.reserve(sorted.size());
    for (size_t i = 0; i < sorted.size(); ++i) {
      index.emplace(sorted[i], i);
    }
  }

  using iterator = impl::iterator_t<sorted_graph, true>; ///< Only const iteration is allowed
  using const_iterator = iterator;

  /**
   * @brief Get iterator to beginning of sorted nodes
   */
  iterator begin() const { return const_iterator(this, 0); }
  const_iterator cbegin() const { return begin(); }

  /**
   * @brief Get iterator to end of sorted nodes
   */
  iterator end() const { return const_iterator(this, sorted.size()); }
  const_iterator cend() const { return end(); }

  /**
   * @brief Access node and predecessors by sorted index
   *
   * @param id The index in topological order (0-based)
   * @return Pair of (node, predecessors) at the given index
   */
  value_type operator[](size_type id) const {
    T const &node = sorted[id];
    return std::make_pair(std::cref(node), std::cref(this->pred_of(node)));
  }

  /**
   * @brief Access node and predecessors by sorted index with bounds checking
   *
   * @param id The index in topological order (0-based)
   * @return Pair of (node, predecessors) at the given index
   * @throws std::out_of_range if index is out of bounds
   */
  value_type at(size_type id) const {
    if (id >= sorted.size()) {
      throw std::out_of_range("Index out of range");
    }
    return (*this)[id];
  }

  /**
   * @brief Query id of a node in the sorted graph
   *
   * @param node The node to find the ID for
   * @return The index of the node in the sorted order, or invalid_id if not found
   */
  size_type id(T const &node) const {
    auto it = index.find(node);
    if (it == index.end()) {
      return invalid_id;
    }
    return it->second;
  }

  /**
   * @brief Get the node at a specific sorted index
   *
   * @param id The index in topological order (0-based)
   * @return The node at the given index
   */
  T const &node_at(size_type id) const { return sorted[id]; }

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
