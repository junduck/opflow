#pragma once

#include <cassert>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common.hpp"

namespace opflow {
namespace detail {
// transparent hashing for pointer pointing to T
template <typename T>
struct ptr_hash {
  using is_transparent = void;

  std::size_t operator()(T *ptr) const noexcept { return std::hash<T const *>()(ptr); }
  std::size_t operator()(T const *ptr) const noexcept { return std::hash<T const *>()(ptr); }
  std::size_t operator()(std::shared_ptr<T> const &ptr) const noexcept { return std::hash<T const *>()(ptr.get()); }
  std::size_t operator()(std::unique_ptr<T> const &ptr) const noexcept { return std::hash<T const *>()(ptr.get()); }
};

template <typename T>
struct graph_edge {
  T node;
  uint32_t port;

  graph_edge(T const &node, uint32_t port) : node(node), port(port) {}
  graph_edge(T &&node, uint32_t port) : node(std::move(node)), port(port) {}

  friend bool operator==(graph_edge const &lhs, graph_edge const &rhs) noexcept = default;
};
} // namespace detail

template <dag_node_ptr T>
auto operator|(T &&node, uint32_t pos) {
  using DT = std::decay_t<T>;
  return detail::graph_edge<DT>{std::forward<T>(node), pos};
}

template <dag_node_ptr T>
auto make_edge(T &&node, uint32_t pos = 0) {
  using DT = std::decay_t<T>;
  return detail::graph_edge<DT>{std::forward<T>(node), pos};
}

template <dag_node T>
class graph_node {
  using data_type = typename T::data_type; ///< underlying data type
  using Hash = detail::ptr_hash<T>;        ///< transparent hashing for pointer to T
  using Equal = std::equal_to<>;           ///< transparent equality for pointer to T

public:
  using node_type = std::shared_ptr<T>;                                ///< node type, shared_ptr to T
  using edge_type = detail::graph_edge<node_type>;                     ///< edge type, represents an arg passed to node
  using NodeSet = std::unordered_set<node_type, Hash, Equal>;          ///< set of node observer
  using NodeArgsSet = std::vector<edge_type>;                          ///< set of node arguments
  using NodeMap = std::unordered_map<node_type, NodeSet, Hash, Equal>; ///< node -> adjacent nodes
  using NodeArgsMap = std::unordered_map<node_type, NodeArgsSet, Hash, Equal>; ///< node -> call arguments
  using NodeStorage = std::vector<node_type>;                                  ///< storage for all nodes

  template <range_of<node_type> R>
  void add(node_type const &node, R &&preds) {
    ensure_node(node);

    for (auto pred : preds) {
      ensure_node(pred);
      add_edge_impl(node, pred, 0); // Add with default port 0
    };
  }

  template <range_of<edge_type> R>
  void add(node_type const &node, R &&preds) {
    ensure_node(node);

    for (auto const &[pred, port] : preds) {
      ensure_node(pred);
      add_edge_impl(node, pred, port);
    }
  }

  void add(node_type const &node, std::initializer_list<node_type> preds) {
    std::vector<node_type> preds_list(preds);
    add(node, preds_list);
  }

  void add(node_type const &node, std::initializer_list<edge_type> preds) {
    std::vector<edge_type> preds_list(preds);
    add(node, preds_list);
  }

  void add(node_type const &node, node_type const &pred) {
    ensure_node(node);

    ensure_node(pred);
    add_edge_impl(node, pred, 0);
  }

  void add(node_type const &node, edge_type const &pred) {
    ensure_node(node);

    ensure_node(pred.node);
    add_edge_impl(node, pred.node, pred.port);
  }

  void add(node_type const &node) { ensure_node(node); }

  // In-place construction

  template <typename U, typename R, typename... Args>
  node_type add(R &&preds, Args &&...args)
    requires(range_of<R, node_type> || range_of<R, edge_type>)
  {
    node_type node = std::make_shared<U>(std::forward<Args>(args)...);
    add(node, std::forward<R>(preds));
    return node;
  }

  template <typename U, typename... Args>
  node_type add(std::initializer_list<node_type> preds, Args &&...args) {
    node_type node = std::make_shared<U>(std::forward<Args>(args)...);
    add(node, preds);
    return node;
  }

  template <typename U, typename... Args>
  node_type add(std::initializer_list<edge_type> preds, Args &&...args) {
    node_type node = std::make_shared<U>(std::forward<Args>(args)...);
    add(node, preds);
    return node;
  }

  template <typename U, typename... Args>
  node_type add(node_type const &pred, Args &&...args) {
    node_type node = std::make_shared<U>(std::forward<Args>(args)...);
    add(node, pred);
    return node;
  }

  template <typename U, typename... Args>
  node_type add(edge_type const &pred, Args &&...args) {
    node_type node = std::make_shared<U>(std::forward<Args>(args)...);
    add(node, pred);
    return node;
  }

  template <template <typename> typename U, typename R, typename... Args>
  node_type add(R &&preds, Args &&...args)
    requires(range_of<R, node_type> || range_of<R, edge_type>)
  {
    using UT = U<data_type>;
    node_type node = std::make_shared<UT>(std::forward<Args>(args)...);
    add(node, std::forward<R>(preds));
    return node;
  }

  template <template <typename> typename U, typename... Args>
  node_type add(std::initializer_list<node_type> preds, Args &&...args) {
    using UT = U<data_type>;
    node_type node = std::make_shared<UT>(std::forward<Args>(args)...);
    add(node, preds);
    return node;
  }

  template <template <typename> typename U, typename... Args>
  node_type add(std::initializer_list<edge_type> preds, Args &&...args) {
    using UT = U<data_type>;
    node_type node = std::make_shared<UT>(std::forward<Args>(args)...);
    add(node, preds);
    return node;
  }

  template <template <typename> typename U, typename... Args>
  node_type add(node_type const &pred, Args &&...args) {
    using UT = U<data_type>;
    node_type node = std::make_shared<UT>(std::forward<Args>(args)...);
    add(node, pred);
    return node;
  }

  template <template <typename> typename U, typename... Args>
  node_type add(edge_type const &pred, Args &&...args) {
    using UT = U<data_type>;
    node_type node = std::make_shared<UT>(std::forward<Args>(args)...);
    add(node, pred);
    return node;
  }

  template <typename U, typename... Args>
  node_type root(Args &&...args) {
    node_type node = std::make_shared<U>(std::forward<Args>(args)...);
    add(node);
    return node;
  }

  template <template <typename> typename U, typename... Args>
  node_type root(Args &&...args) {
    using UT = U<data_type>;
    node_type node = std::make_shared<UT>(std::forward<Args>(args)...);
    add(node);
    return node;
  }

  // Removal

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

  // Edge

  template <range_of<node_type> R>
  void add_edge(node_type const &node, R &&preds) {
    for (auto const &pred : preds) {
      add_edge_impl(node, pred, 0);
    }
  }

  template <range_of<edge_type> R>
  void add_edge(node_type const &node, R &&preds) {
    for (auto const &[pred, port] : preds) {
      add_edge_impl(node, pred, port);
    }
  }

  void add_edge(node_type const &node, node_type const &pred) { add_edge_impl(node, pred, 0); }

  void add_edge(node_type const &node, edge_type const &pred) { add_edge_impl(node, pred.node, pred.port); }

  template <range_of<node_type> R>
  void rm_edge(node_type const &node, R &&preds) {
    if (predecessor.find(node) == predecessor.end()) {
      return; // Node doesn't exist
    }
    for (auto const &pred : preds) {
      rm_edge_impl(node, pred, 0);
    }
  }

  template <range_of<edge_type> R>
  void rm_edge(node_type const &node, R &&preds) {
    if (predecessor.find(node) == predecessor.end()) {
      return; // Node doesn't exist
    }
    for (auto const &[pred, port] : preds) {
      rm_edge_impl(node, pred, port);
    }
  }

  void rm_edge(node_type const &node, node_type const &pred) {
    if (predecessor.find(node) == predecessor.end()) {
      return; // Node doesn't exist
    }
    rm_edge_impl(node, pred, 0);
  }

  void rm_edge(node_type const &node, edge_type const &pred) {
    if (predecessor.find(node) == predecessor.end()) {
      return; // Node doesn't exist
    }
    rm_edge_impl(node, pred.node, pred.port);
  }

  // Replacement

  void replace(node_type const &new_node, node_type const &old_node) {
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

  void replace(node_type const &node, edge_type const &old_pred, edge_type const &new_pred) {
    if (predecessor.find(node) == predecessor.end()) {
      return; // Node doesn't exist
    }
    if (old_pred == new_pred) {
      return; // No change needed
    }

    auto &args = argmap[node];

    if (std::find(args.begin(), args.end(), old_pred) == args.end()) {
      return; // Old edge doesn't exist, nothing to replace
    }

    // Ensure new predecessor node exists
    ensure_node(new_pred.node);

    // Update adjacency maps
    predecessor[node].emplace(new_pred.node);
    successor[new_pred.node].emplace(node);

    // Replace old_pred with new_pred in argmap
    for (auto &arg : args) {
      if (arg == old_pred) {
        arg = new_pred;
      }
    }

    cleanup_adj(node, old_pred.node); // Check if old_pred is still needed
  }

  // Utilities

  size_t size() const { return predecessor.size(); }

  bool empty() const { return predecessor.empty(); }

  void clear() {
    predecessor.clear();
    argmap.clear();
    successor.clear();
  }

  bool contains(node_type const &node) const { return predecessor.find(node) != predecessor.end(); }

  NodeSet const &pred_of(node_type const &node) const {
    static NodeSet empty_set{};
    auto it = predecessor.find(node);
    return (it != predecessor.end()) ? it->second : empty_set;
  }

  NodeMap const &get_pred() const { return predecessor; }

  NodeArgsSet const &args_of(node_type const &node) const {
    static NodeArgsSet empty_set{};
    auto it = argmap.find(node);
    return (it != argmap.end()) ? it->second : empty_set;
  }

  NodeArgsMap const &get_args() const { return argmap; }

  NodeSet const &succ_of(node_type const &node) const {
    static NodeSet empty_set{};
    auto it = successor.find(node);
    return (it != successor.end()) ? it->second : empty_set;
  }

  NodeMap const &get_succ() const { return successor; }

  bool is_root(node_type const &node) const {
    auto it = predecessor.find(node);
    return (it != predecessor.end() && it->second.empty());
  }

  bool is_leaf(node_type const &node) const {
    auto it = successor.find(node);
    return (it != successor.end() && it->second.empty());
  }

  auto get_roots() const {
    std::vector<node_type> roots;
    for (auto [node, preds] : predecessor) {
      if (preds.empty()) {
        roots.push_back(node);
      }
    }
    return roots;
  }

  auto get_leaves() const {
    std::vector<node_type> leaves;
    for (auto [node, succs] : successor) {
      if (succs.empty()) {
        leaves.push_back(node);
      }
    }
    return leaves;
  }

  void merge(graph_node const &other) {
    // Collect new nodes to add
    NodeSet nodes_to_add{};
    for (auto const &[other_node, _] : other.predecessor) {
      if (!contains(other_node)) {
        nodes_to_add.emplace(other_node);
      }
    }

    // Add all new nodes to the graph
    for (auto const &new_node : nodes_to_add) {
      add(new_node, other.args_of(new_node));
    }
  }

  graph_node &operator+=(graph_node const &rhs) {
    merge(rhs);
    return *this;
  }

  friend graph_node operator+(graph_node const &lhs, graph_node const &rhs) {
    graph_node result(lhs);
    result.merge(rhs);
    return result;
  }

private:
  // add slot for node
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
    argmap[node].emplace_back(pred, port);
    successor[pred].emplace(node);
  }

  void rm_edge_impl(node_type const &node, node_type const &pred, uint32_t port) {
    auto &args = argmap[node];
    auto rm = make_edge(pred, port);

    if (std::find(args.begin(), args.end(), rm) == args.end()) {
      return; // Edge doesn't exist, nothing to remove
    }

    // Remove all edges [pred:port]
    std::erase(args, rm);

    // Cleanup adjacency maps
    cleanup_adj(node, rm.node);
  }

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
