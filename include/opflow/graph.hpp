#pragma once

#include <cassert>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common.hpp"

namespace opflow {
namespace detail {
template <std::copy_constructible T>
struct node_arg_t {
  T node;        ///< Node data
  uint32_t port; ///< Port ID, used to connect to specific output of an operator node

  node_arg_t(T const &node, uint32_t port) : node(node), port(port) {}
  node_arg_t(T &&node, uint32_t port) : node(std::move(node)), port(port) {}

  friend bool operator==(node_arg_t const &lhs, node_arg_t const &rhs) noexcept = default;
};

struct node_port_t {
  uint32_t pos; ///< Port index, used to connect to specific output of an operator node
};
} // namespace detail

namespace literals {

constexpr auto operator"" _p(unsigned long long pos) { return detail::node_port_t{static_cast<uint32_t>(pos)}; }

/**
 * @brief Create an arg wrapper for a specified node and output port
 *
 * @note If node is a char literal, it will be converted to std::string
 *
 * @code
 * auto arg = "node" | 1_p; // creates an arg_wrapper<std::string>, not arg_wrapper<char const*>
 * auto arg2 = make_shared<my_node_t>("my_node") | 0_p;
 * @endcode
 */
template <typename T>
auto operator|(T &&node, detail::node_port_t pos) {
  using DT = std::decay_t<T>;
  return detail::node_arg_t<DT>{std::forward<T>(node), pos.pos};
}

/// @overload
inline auto operator|(char const *node, detail::node_port_t pos) {
  return detail::node_arg_t<std::string>{std::string(node), pos.pos};
}
} // namespace literals

/**
 * @brief Create an arg wrapper for a specified node and output port
 *
 * @note If node is a char literal, it will be converted to std::string
 *
 * @code
 * auto arg = make_node_arg("node", 1); // creates an arg_wrapper<std::string>, not arg_wrapper<char const*>
 * using namespace opflow::literals;
 * auto arg2 = make_node_arg("node" | 1_p);
 * @endcode
 */
template <typename T>
auto make_node_arg(T &&node, uint32_t pos = 0) {
  using DT = std::decay_t<T>;
  return opflow::detail::node_arg_t<DT>{std::forward<T>(node), pos};
}

/// @overload
template <typename T>
auto make_node_arg(T &&node, opflow::detail::node_port_t pos) {
  return make_node_arg(std::forward<T>(node), pos.pos);
}

/// @overload
inline auto make_node_arg(char const *node, uint32_t pos = 0) { return make_node_arg(std::string(node), pos); }

/// @overload
inline auto make_node_arg(char const *node, opflow::detail::node_port_t pos) {
  return make_node_arg(std::string(node), pos.pos);
}

/**
 * @brief A generic directed graph
 *
 * @tparam T node type
 * @tparam Hash Hash function for nodes
 * @tparam Equal Equality comparison function for nodes
 */
template <std::copy_constructible T, typename Hash = std::hash<T>, typename Equal = std::equal_to<T>>
class graph {
public:
  /// @brief Type alias for a node
  using node_type = T;

  /// @brief Type alias for a node with port
  using node_arg_type = detail::node_arg_t<T>;

  /// @brief Type alias for a set of nodes
  using NodeSet = std::unordered_set<node_type, Hash, Equal>;

  /// @brief Type alias for a list of nodes with port
  using NodeArgsSet = std::vector<node_arg_type>;

  /// @brief Type alias for a map of node -> adjacent nodes
  using NodeMap = std::unordered_map<node_type, NodeSet, Hash, Equal>;

  /// @brief Type alias for a map of node -> call arguments for this node
  using NodeArgsMap = std::unordered_map<node_type, NodeArgsSet, Hash, Equal>;

  /**
   * @brief Add connections to the graph
   *
   * Add connections between node and its predecessors to the graph. If node and preds don't
   * exist in the graph, they will be created automatically.
   *
   * @param node The node to add to the graph
   * @param preds [OPTIONAL] A range of predecessors that this node has
   *
   * @note node is copy constructed into the graph, user should consider value semantics or
   * shared ownership if needed
   *
   * Example:
   * @code
   * g.add("mynode", {"dep1", "dep2"}); // by default connects to port 0
   * g.add("mynode", {{"dep1", 0}, {"dep2", 1}}); // specify ports (aka output index)
   * using namespace opflow::literals;
   * g.add("mynode", {"dep1" | 0_p, "dep2" | 1_p}); // use _p literal and operator| for expression
   * g.add("mynode"); // add node without predecessors
   * g.add("mynode", "dep1"); // add node with single predecessor
   * g.add("mynode", "dep1" | 0_p); // add node with single predecessor and port
   * @endcode
   */
  template <range_of<node_type> R>
  void add(node_type const &node, R &&preds) {
    ensure_node(node); // Ensure the node is added to the graph

    for (auto const &pred : preds) {
      ensure_node(pred);
      add_edge_impl(node, pred, 0); // Add with default port 0
    }
  }

  /// @overload
  template <range_of<node_arg_type> R>
  void add(node_type const &node, R &&preds) {
    ensure_node(node); // Ensure the node is added to the graph

    for (auto const &[pred, port] : preds) {
      ensure_node(pred);
      add_edge_impl(node, pred, port); // Add with specified port
    }
  }

  /// @overload
  void add(node_type const &node, std::initializer_list<node_type> preds) {
    std::vector<node_type> preds_list(preds);
    add(node, preds_list);
  }

  /// @overload
  void add(node_type const &node, std::initializer_list<node_arg_type> edges) {
    std::vector<node_arg_type> edges_list(edges);
    add(node, edges_list);
  }

  /// @overload
  void add(node_type const &node) { ensure_node(node); }

  /// @overload
  void add(node_type const &node, node_type const &pred) {
    ensure_node(node); // Ensure the node is added to the graph

    ensure_node(pred);
    add_edge_impl(node, pred, 0);
  }

  /// @overload
  void add(node_type const &node, node_arg_type const &edge) {
    ensure_node(node); // Ensure the node is added to the graph

    ensure_node(edge.node);
    add_edge_impl(node, edge.node, edge.port);
  }

  /**
   * @brief Add a new node to the graph, make shared in-place
   *
   * Example:
   * @code
   graph<ob_base<double>> g{};
   auto root = g.add<op::graph_root<double>>(2);
   auto ma = g.add<op::ema<double>>(root | 1_p, 0.5); // depend on root port 1
   auto sum = g.add<op::sum<double>>(root); // default port 0
   auto add = g.add<op::add2>({sum, ma});  // overload instantiates op::add2<double> properly
   * @endcode
   *
   * @tparam U The type of the new node
   * @param preds A range of predecessors for the new node
   * @param args args passed to the new node's constructor
   * @return node_type a shared pointer to the newly created node
   */
  template <typename U, range_of<node_type> R, typename... Args>
  node_type add(R &&preds, Args &&...args)
    requires(dag_node_base<node_type>)
  {
    node_type node = std::make_shared<U>(std::forward<Args>(args)...);
    add(node, std::forward<R>(preds));
    return node; // Return the created node
  }

  template <typename U, range_of<node_arg_type> R, typename... Args>
  node_type add(R &&preds, Args &&...args)
    requires(dag_node_base<node_type>)
  {
    node_type node = std::make_shared<U>(std::forward<Args>(args)...);
    add(node, std::forward<R>(preds));
    return node; // Return the created node
  }

  template <typename U, typename... Args>
  node_type add(std::initializer_list<node_type> preds, Args &&...args)
    requires(dag_node_base<node_type>)
  {
    node_type node = std::make_shared<U>(std::forward<Args>(args)...);
    add(node, preds);
    return node; // Return the created node
  }

  template <typename U, typename... Args>
  node_type add(std::initializer_list<node_arg_type> preds, Args &&...args)
    requires(dag_node_base<node_type>)
  {
    node_type node = std::make_shared<U>(std::forward<Args>(args)...);
    add(node, preds);
    return node; // Return the created node
  }

  template <typename U, typename... Args>
  node_type add(node_type const &pred, Args &&...args)
    requires(dag_node_base<node_type>)
  {
    node_type node = std::make_shared<U>(std::forward<Args>(args)...);
    add(node, pred);
    return node; // Return the created node
  }

  template <typename U, typename... Args>
  node_type add(node_arg_type const &pred, Args &&...args)
    requires(dag_node_base<node_type>)
  {
    node_type node = std::make_shared<U>(std::forward<Args>(args)...);
    add(node, pred);
    return node; // Return the created node
  }

  template <template <typename> typename U, range_of<node_type> R, typename... Args>
  node_type add(R &&preds, Args &&...args)
    requires(dag_node_base<node_type>)
  {
    using data_type = typename node_type::element_type::data_type;
    node_type node = std::make_shared<U<data_type>>(std::forward<Args>(args)...);
    add(node, std::forward<R>(preds));
    return node; // Return the created node
  }

  template <template <typename> typename U, range_of<node_arg_type> R, typename... Args>
  node_type add(R &&preds, Args &&...args)
    requires(dag_node_base<node_type>)
  {
    using data_type = typename node_type::element_type::data_type;
    node_type node = std::make_shared<U<data_type>>(std::forward<Args>(args)...);
    add(node, std::forward<R>(preds));
    return node; // Return the created node
  }

  template <template <typename> typename U, typename... Args>
  node_type add(std::initializer_list<node_type> preds, Args &&...args)
    requires(dag_node_base<node_type>)
  {
    using data_type = typename node_type::element_type::data_type;
    node_type node = std::make_shared<U<data_type>>(std::forward<Args>(args)...);
    add(node, preds);
    return node; // Return the created node
  }

  template <template <typename> typename U, typename... Args>
  node_type add(std::initializer_list<node_arg_type> preds, Args &&...args)
    requires(dag_node_base<node_type>)
  {
    using data_type = typename node_type::element_type::data_type;
    node_type node = std::make_shared<U<data_type>>(std::forward<Args>(args)...);
    add(node, preds);
    return node; // Return the created node
  }

  template <template <typename> typename U, typename... Args>
  node_type add(node_type const &pred, Args &&...args)
    requires(dag_node_base<node_type>)
  {
    using data_type = typename node_type::element_type::data_type;
    node_type node = std::make_shared<U<data_type>>(std::forward<Args>(args)...);
    add(node, pred);
    return node; // Return the created node
  }

  template <template <typename> typename U, typename... Args>
  node_type add(node_arg_type const &pred, Args &&...args)
    requires(dag_node_base<node_type>)
  {
    using data_type = typename node_type::element_type::data_type;
    node_type node = std::make_shared<U<data_type>>(std::forward<Args>(args)...);
    add(node, pred);
    return node; // Return the created node
  }

  template <typename U, typename... Args>
  node_type root(Args &&...args)
    requires(dag_node_base<node_type>)
  {
    node_type node = std::make_shared<U>(std::forward<Args>(args)...);
    add(node);
    return node; // Return the created node
  }

  template <template <typename> typename U, typename... Args>
  node_type root(Args &&...args)
    requires(dag_node_base<node_type>)
  {
    using data_type = typename node_type::element_type::data_type;
    node_type node = std::make_shared<U<data_type>>(std::forward<Args>(args)...);
    add(node);
    return node; // Return the created node
  }

  /**
   * @brief Remove connections from the graph
   *
   * Remove connections between node and its predecessors from the graph. If only node is specified, the node
   * itself is removed from the graph.
   *
   * @param node The node to remove connections from
   * @param preds [OPTIONAL] A range of predecessors to remove connections. If not specified, node is removed.
   *
   * Example:
   * @code
   * g.rm("mynode", {"dep1", "dep2"}); // remove all edges to dep1:0 and dep2:0, default port is 0
   * g.rm("mynode", "dep1" | 42_p); // remove edge to dep1:42
   * g.rm("mynode", {{"dep1" | 0_p}, {"dep2" | 1_p}}); // remove specific edges with ports
   * g.rm("mynode"); // remove node itself.
   * @endcode
   */
  template <range_of<node_type> R>
  void rm(node_type const &node, R &&preds) {
    if (predecessor.find(node) == predecessor.end()) {
      return; // Node doesn't exist
    }
    for (auto const &pred : preds) {
      rm_edge_impl(node, pred, 0); // Remove with default port 0
    }
  }

  /// @overload
  template <range_of<node_arg_type> R>
  void rm(node_type const &node, R &&edges) {
    if (predecessor.find(node) == predecessor.end()) {
      return; // Node doesn't exist
    }
    for (auto const &[pred, port] : edges) {
      rm_edge_impl(node, pred, port); // Remove with specified port
    }
  }

  /// @overload
  void rm(node_type const &node, node_type const &pred) {
    if (predecessor.find(node) == predecessor.end()) {
      return; // Node doesn't exist
    }
    rm_edge_impl(node, pred, 0); // Remove with default port 0
  }

  /// @overload
  void rm(node_type const &node, node_arg_type const &edge) {
    if (predecessor.find(node) == predecessor.end()) {
      return; // Node doesn't exist
    }
    auto const &[pred_node, pred_port] = edge;
    rm_edge_impl(node, pred_node, pred_port); // Remove with specified port
  }

  /// @overload
  void rm(node_type const &node) {
    if (predecessor.find(node) == predecessor.end()) {
      return; // Node doesn't exist
    }

    // Remove this node from pred map of all successors
    for (auto const &succ : successor[node]) {
      predecessor[succ].erase(node);
      auto n = std::erase_if(argmap[succ], [&](auto const &np) { return np.node == node; });
      assert(n && "[BUG] Inconsistent graph state.");
    }

    // Remove this node from succ map of all predecessors
    for (auto const &pred : predecessor[node]) {
      successor[pred].erase(node);
    }

    // Remove the node from adj maps
    predecessor.erase(node);
    argmap.erase(node);
    successor.erase(node);
  }

  /**
   * @brief Replace a vertex in the graph
   *
   * Replaces an existing node with a new node in the graph. The new node takes over all adjacency information
   * (predecessors, successors, and arguments) of the old node. After replacement old_node is removed from graph.
   * This operation is useful for updating nodes without changing their connections.
   *
   * @note If old_node doesn't exist, this operation has no effect.
   * @note If new_node already exists, this operation has no effect.
   *
   * @param old_node The node to replace
   * @param new_node The new node to replace the old one
   */
  void replace(node_type const &old_node, node_type const &new_node) {
    if (predecessor.find(old_node) == predecessor.end()) {
      return; // Old node doesn't exist
    }
    if (predecessor.find(new_node) != predecessor.end()) {
      return; // Can't replace with an existing node
    }
    if (old_node == new_node) {
      return; // No change needed
    }

    // Copy all adjacency information from old_node to new_node
    predecessor.emplace(new_node, std::move(predecessor[old_node]));
    argmap.emplace(new_node, std::move(argmap[old_node]));
    successor.emplace(new_node, std::move(successor[old_node]));

    // Update all predecessors to point to new_node instead of old_node
    for (auto const &pred : predecessor[new_node]) {
      successor[pred].erase(old_node);
      successor[pred].insert(new_node);
    }

    // Update all successors to depend on new_node instead of old_node
    for (auto const &succ : successor[new_node]) {
      predecessor[succ].erase(old_node);
      predecessor[succ].insert(new_node);

      // Update args to replace old_node with new_node
      for (auto &arg : argmap[succ]) {
        if (arg.node == old_node) {
          arg.node = new_node;
        }
      }
    }

    // Remove old_node from all maps
    predecessor.erase(old_node);
    argmap.erase(old_node);
    successor.erase(old_node);
  }

  /**
   * @brief Replace an edge in the graph
   *
   * @warning Since duplicate edges are allowed in node arguments, this method will replace all [pred:port] edges
   * in the node's argument list.
   *
   * @param node The node to replace the edge for
   * @param old_edge The edge to replace
   * @param new_edge The new edge to replace the old_edge with
   *
   * Example:
   * @code
   * g.add("mynode", {{"dep1" | 0_p}, {"another", 2}); // post: mynode -> [dep1:0, another:2]
   * g.replace_edge("mynode", "dep1" | 0_p, "dep2" | 1_p); // post: mynode -> [dep2:1, another:2] (order is preserved)
   *
   * g.add("mynode", {{"dep1" | 0_p}, {"another", 2}); // post: mynode -> [dep1:0, another:2]
   * g.rm("mynode", "dep1" | 0_p); // post: mynode -> [another:2]
   * g.add("mynode", "dep2" | 1_p); // post: mynode -> [another:2, dep2:1] (notice dep2:1 is added at the end)
   * @endcode
   */
  void replace(node_type const &node, node_arg_type const &old_edge, node_arg_type const &new_edge) {
    if (predecessor.find(node) == predecessor.end()) {
      return; // Node doesn't exist
    }
    if (old_edge == new_edge) {
      return; // No change needed
    }

    auto &args = argmap[node];

    if (std::find(args.begin(), args.end(), old_edge) == args.end()) {
      return; // Old edge doesn't exist, nothing to replace
    }

    // Ensure new predecessor node exists
    ensure_node(new_edge.node);

    // Update adjacency maps
    predecessor[node].emplace(new_edge.node);
    successor[new_edge.node].emplace(node);

    // Replace old_pred with new_pred in argmap
    for (auto &arg : args) {
      if (arg == old_edge) {
        arg = new_edge;
      }
    }

    cleanup_adj(node, old_edge.node); // Check if old_pred is still needed
  }

  /**
   * @brief Get the number of nodes in the graph
   *
   * @return The total number of nodes currently in the graph
   */
  size_t size() const { return predecessor.size(); }

  /**
   * @brief Check if the graph is empty
   *
   * @return true if the graph contains no nodes, false otherwise
   */
  bool empty() const { return predecessor.empty(); }

  /**
   * @brief Clear the graph
   *
   * Removes all nodes and edges from the graph, leaving it in an empty state.
   * After calling this method, size() will return 0.
   */
  void clear() {
    predecessor.clear();
    argmap.clear();
    successor.clear();
  }

  /**
   * @brief Check if a node exists in the graph
   *
   * @param node The node to search for
   * @return true if the node exists in the graph, false otherwise
   */
  bool contains(node_type const &node) const { return predecessor.find(node) != predecessor.end(); }

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
  NodeSet const &pred_of(node_type const &node) const {
    static NodeSet empty_set{};
    auto it = predecessor.find(node);
    return (it != predecessor.end()) ? it->second : empty_set;
  }

  auto const &get_pred() const { return predecessor; }

  /**
   * @brief Get the arguments list of a node
   *
   * @param node The node to get arguments for
   * @return A vector of [node, port] pairs representing the arguments for this node
   */
  NodeArgsSet const &args_of(node_type const &node) const {
    static NodeArgsSet empty_set{};
    auto it = argmap.find(node);
    return (it != argmap.end()) ? it->second : empty_set;
  }

  auto const &get_args() const { return argmap; }

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
  NodeSet const &succ_of(node_type const &node) const {
    static NodeSet empty_set{};
    auto it = successor.find(node);
    return (it != successor.end()) ? it->second : empty_set;
  }

  auto const &get_succ() const { return successor; }

  /**
   * @brief Check if a node is a root node
   *
   * @param node The node to check
   * @return true if the node is a root node, false otherwise
   */
  bool is_root(T const &node) const {
    // A root node has no predecessors
    auto it = predecessor.find(node);
    return (it != predecessor.end() && it->second.empty());
  }

  /**
   * @brief Check if a node is a leaf node
   *
   * @param node The node to check
   * @return true if the node is a leaf node, false otherwise
   */
  bool is_leaf(T const &node) const {
    // A leaf node has no successors
    auto it = successor.find(node);
    return (it != successor.end() && it->second.empty());
  }

  /**
   * @brief Get all root nodes in the graph
   *
   * @return a vector containing all nodes that have no predecessors (root nodes).
   */
  auto get_roots() const {
    std::vector<T> roots;
    for (auto const &[node, preds] : predecessor) {
      if (preds.empty()) {
        roots.push_back(node);
      }
    }
    return roots;
  }

  /**
   * @brief Get the leaves of the graph
   *
   * @return a vector containing all nodes that have no successors (leaf nodes).
   */
  auto get_leaves() const {
    std::vector<T> leaves;
    for (auto const &[node, succs] : successor) {
      if (succs.empty()) {
        leaves.push_back(node);
      }
    }
    return leaves;
  }

  /**
   * @brief Merge another graph into this
   *
   * Merges the specified graph into this graph. Only *NEW* nodes are added to the graph.
   * Example:
   * this:  A -> [B:0, C:1]
   * other: A -> [D:0, E:0]
   *
   * after merging:
   *
   * pred_of(A) = {B, C}     (no effect)
   * args_of(A) = [B:0, C:1] (A is still called by A(B:0, C:1))
   *
   * @param other The graph to merge
   *
   * @code
   * graph g1, g2;
   * g1.merge(g2); // Merges g2 into g1, adding only new nodes
   * auto g3 = g1 + g2; // Merges g2 into g1 and returns a new graph
   * auto g4 = g2 + g1; // Conflicting predecessors are resolved by keeping existing edges in g2
   */
  void merge(graph const &other) {
    NodeSet nodes_to_add{};
    for (auto const &[other_node, _] : other.predecessor) {
      if (!contains(other_node)) {
        nodes_to_add.emplace(other_node); // Collect new nodes to add
      }
    }

    // Add all new nodes to the graph
    for (auto const &new_node : nodes_to_add) {
      add(new_node, other.args_of(new_node));
    }
  }

  graph &operator+=(graph const &rhs) {
    merge(rhs);
    return *this;
  }

  friend graph operator+(graph const &lhs, graph const &rhs) {
    graph result(lhs);
    result.merge(rhs);
    return result;
  }

private:
  void ensure_node(node_type const &node) {
    if (predecessor.find(node) == predecessor.end()) {
      predecessor.emplace(node, NodeSet{});
    }
    if (argmap.find(node) == argmap.end()) {
      argmap.emplace(node, NodeArgsSet{});
    }
    if (successor.find(node) == successor.end()) {
      successor.emplace(node, NodeSet{});
    }
  }

  // add edge node -> [pred:port]
  void add_edge_impl(node_type const &node, node_type const &pred, uint32_t port) {
    predecessor[node].emplace(pred);
    argmap[node].emplace_back(pred, port); // Add with port information
    successor[pred].emplace(node);
  }

  // remove edge node -> [pred:port]
  void rm_edge_impl(node_type const &node, node_type const &pred, uint32_t port) {
    auto &args = argmap[node];
    auto rm = make_node_arg(pred, port);

    if (std::find(args.begin(), args.end(), rm) == args.end()) {
      return; // Edge doesn't exist, nothing to remove
    }

    // Remove all edges [pred:port]
    std::erase(args, rm);

    // Cleanup adjacency maps
    cleanup_adj(node, rm.node);
  }

  // Check if node's call args still require pred, if not, remove it from adjacency maps
  void cleanup_adj(node_type const &node, node_type const &pred) {
    auto const &args = argmap[node];
    bool has_conn = std::any_of(args.begin(), args.end(), [&](auto const &np) { return np.node == pred; });
    if (!has_conn) {
      assert(predecessor[node].find(pred) != predecessor[node].end() &&
             "[BUG] Inconsistent graph state: predecessor not found in adj map.");
      assert(successor[pred].find(node) != successor[pred].end() &&
             "[BUG] Inconsistent graph state: successor not found in reverse_adj map.");
      // If no other connections exist, remove old predecessor from adjacency maps
      predecessor[node].erase(pred);
      successor[pred].erase(node);
    }
  }

protected:
  NodeMap predecessor; ///< Adjacency list: node -> [pred] i.e. set of nodes that it depends on
  NodeArgsMap argmap;  ///< node -> [pred:port] i.e. args for calling this op node, order preserved
  NodeMap successor;   ///< Reverse adjacency list: node -> [succ] i.e. set of nodes that depend on it
};
} // namespace opflow
