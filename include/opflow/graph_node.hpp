#pragma once

#include <cassert>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common.hpp"
#include "detail/utils.hpp"

namespace opflow {
namespace detail {
template <typename T>
struct graph_node_edge {
  T node;
  uint32_t port;

  graph_node_edge(T const &node, uint32_t port) : node(node), port(port) {}
  graph_node_edge(T &&node, uint32_t port) : node(std::move(node)), port(port) {}

  explicit graph_node_edge(T const &node) : node(node), port(0) {}
  explicit graph_node_edge(T &&node) : node(std::move(node)), port(0) {}

  friend bool operator==(graph_node_edge const &lhs, graph_node_edge const &rhs) noexcept = default;
};
} // namespace detail

template <typename T>
auto operator|(std::shared_ptr<T> const &node, uint32_t pos) {
  return detail::graph_node_edge<std::shared_ptr<T>>{node, pos};
}

template <typename T>
auto make_edge(std::shared_ptr<T> const &node, uint32_t pos = 0) {
  return detail::graph_node_edge<std::shared_ptr<T>>{node, pos};
}

template <typename T>
class graph_node {
  using Equal = std::equal_to<>; ///< transparent equality for pointer to T

public:
  using key_type = std::shared_ptr<T>;                            ///< key type, shared_ptr to T
  using key_hash = detail::ptr_hash<T>;                           ///< transparent hashing for pointer to T
  using node_type = std::shared_ptr<T>;                           ///< node type, shared_ptr to T
  using edge_type = detail::graph_node_edge<node_type>;           ///< edge type, represents an arg passed to node
  using NodeSet = std::unordered_set<node_type, key_hash, Equal>; ///< set of nodes
  using NodeArgsSet = std::vector<edge_type>;                     ///< set of node arguments
  using NodeMap = std::unordered_map<node_type, NodeSet, key_hash, Equal>;         ///< node -> adjacent nodes
  using NodeArgsMap = std::unordered_map<node_type, NodeArgsSet, key_hash, Equal>; ///< node -> call arguments

  // Add

  template <typename... Ts>
  void add(node_type const &node, Ts &&...preds) {
    if (!node)
      return;

    std::vector<edge_type> preds_list{};
    preds_list.reserve(sizeof...(preds));
    add_impl(preds_list, node, std::forward<Ts>(preds)...);
  }

  // In-place construction

  template <typename U, typename... Args>
  node_type add(Args &&...preds_and_args) {
    std::vector<edge_type> preds_list{};
    preds_list.reserve(sizeof...(preds_and_args));
    return add_inplace_impl<U>(preds_list, std::forward<Args>(preds_and_args)...);
  }

  template <template <typename> typename U, typename... Args>
  node_type add(Args &&...preds_and_args)
    requires(detail::has_data_type<T>)
  {
    using UT = U<typename T::data_type>;
    std::vector<edge_type> preds_list{};
    preds_list.reserve(sizeof...(preds_and_args));
    return add_inplace_impl<UT>(preds_list, std::forward<Args>(preds_and_args)...);
  }

  node_type root(size_t root_input_size)
    requires(dag_node_base<T>)
  {
    node_type node = std::make_shared<dag_root_type<T>>(root_input_size);
    add(node);
    return node;
  }

  template <typename U, typename... Args>
  node_type root(Args &&...args) {
    node_type node = std::make_shared<U>(std::forward<Args>(args)...);
    add(node);
    return node;
  }

  template <template <typename> typename U, typename... Args>
  node_type root(Args &&...args)
    requires(detail::has_data_type<T>)
  {
    using UT = U<typename T::data_type>;
    node_type node = std::make_shared<UT>(std::forward<Args>(args)...);
    add(node);
    return node;
  }

  // Output

  template <range_of<node_type> R>
  void add_output(R &&outputs) {
    for (auto const &node : outputs) {
      output.push_back(node);
    }
  }

  template <typename... Ts>
  void add_output(Ts &&...outputs) {
    (output.emplace_back(std::forward<Ts>(outputs)), ...);
  }

  template <range_of<node_type> R>
  void set_output(R &&outputs) {
    output.clear();
    add_output(std::forward<R>(outputs));
  }

  template <typename... Ts>
  void set_output(Ts &&...outputs) {
    output.clear();
    add_output(std::forward<Ts>(outputs)...);
  }

  // Removal

  bool rm(node_type const &node) {
    if (predecessor.find(node) == predecessor.end()) {
      return false; // Node doesn't exist
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

    return true;
  }

  // Edge manipulation

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
  bool rm_edge(node_type const &node, R &&preds) {
    if (predecessor.find(node) == predecessor.end()) {
      return false; // Node doesn't exist
    }
    bool removed = false;
    for (auto const &[pred, port] : preds) {
      removed |= rm_edge_impl(node, pred, port);
    }
    return removed;
  }

  bool rm_edge(node_type const &node, node_type const &pred) {
    if (predecessor.find(node) == predecessor.end()) {
      return false; // Node doesn't exist
    }
    return rm_edge_impl(node, pred, 0);
  }

  bool rm_edge(node_type const &node, edge_type const &pred) {
    if (predecessor.find(node) == predecessor.end()) {
      return false; // Node doesn't exist
    }
    return rm_edge_impl(node, pred.node, pred.port);
  }

  // Replacement

  bool replace(node_type const &new_node, node_type const &old_node) {
    if (predecessor.find(old_node) == predecessor.end()) {
      return false; // Old node doesn't exist
    }
    if (predecessor.find(new_node) != predecessor.end()) {
      return false; // Can't replace with an existing node
    }
    if (old_node == new_node) {
      return true; // No change needed
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

    return true;
  }

  bool replace(node_type const &node, edge_type const &old_pred, edge_type const &new_pred) {
    if (predecessor.find(node) == predecessor.end()) {
      return false; // Node doesn't exist
    }
    if (old_pred == new_pred) {
      return true; // No change needed
    }

    auto &args = argmap[node];

    if (std::find(args.begin(), args.end(), old_pred) == args.end()) {
      return false; // Old edge doesn't exist, nothing to replace
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

    // Check if old_pred is still needed
    cleanup_adj(node, old_pred.node);

    return true;
  }

  // Utilities

  size_t size() const noexcept { return predecessor.size(); }

  bool empty() const noexcept { return predecessor.empty(); }

  void clear() noexcept {
    predecessor.clear();
    argmap.clear();
    successor.clear();
    output.clear();
  }

  bool contains(node_type const &node) const noexcept { return predecessor.find(node) != predecessor.end(); }

  NodeSet const &pred_of(node_type const &node) const {
    static NodeSet empty_set{};
    auto it = predecessor.find(node);
    return (it != predecessor.end()) ? it->second : empty_set;
  }

  NodeMap const &get_pred() const noexcept { return predecessor; }

  NodeArgsSet const &args_of(node_type const &node) const {
    static NodeArgsSet empty_set{};
    auto it = argmap.find(node);
    return (it != argmap.end()) ? it->second : empty_set;
  }

  NodeArgsMap const &get_args() const noexcept { return argmap; }

  NodeSet const &succ_of(node_type const &node) const {
    static NodeSet empty_set{};
    auto it = successor.find(node);
    return (it != successor.end()) ? it->second : empty_set;
  }

  NodeMap const &get_succ() const noexcept { return successor; }

  auto const &get_output() const noexcept { return output; }

  node_type get_node(key_type const &node) const {
    if (predecessor.find(node) == predecessor.end()) {
      return nullptr; // Node doesn't exist
    }
    return node;
  }

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

  // add

  template <typename... Ts, range_of<edge_type> R>
  void add_impl(std::vector<edge_type> &preds_list, node_type const &node, R &&preds, Ts &&...args) {
    preds_list.insert(preds_list.end(), std::ranges::begin(preds), std::ranges::end(preds));
    add_impl(preds_list, node, std::forward<Ts>(args)...);
  }

  template <typename... Ts, range_of<node_type> R>
  void add_impl(std::vector<edge_type> &preds_list, node_type const &node, R &&preds, Ts &&...args) {
    for (auto const &pred : preds) {
      preds_list.emplace_back(pred, 0); // Add with default port 0
    }
    add_impl(preds_list, node, std::forward<Ts>(args)...);
  }

  template <typename... Ts>
  void add_impl(std::vector<edge_type> &preds_list, node_type const &node, Ts &&...preds) {
    (preds_list.emplace_back(std::forward<Ts>(preds)), ...);

    ensure_node(node);
    for (auto const &pred : preds_list) {
      ensure_node(pred.node);
      add_edge_impl(node, pred);
    }
  }

  // add inplace - edge

  template <typename U, typename... Ts>
  node_type add_inplace_impl(std::vector<edge_type> &preds_list, edge_type pred, Ts &&...args) {
    preds_list.emplace_back(pred);
    return add_inplace_impl<U>(preds_list, std::forward<Ts>(args)...);
  }

  template <typename U, typename... Ts>
  node_type add_inplace_impl(std::vector<edge_type> &preds_list, node_type pred, Ts &&...args) {
    preds_list.emplace_back(pred, 0); // Add with default port 0
    return add_inplace_impl<U>(preds_list, std::forward<Ts>(args)...);
  }

  template <typename U, range_of<edge_type> R, typename... Ts>
  node_type add_inplace_impl(std::vector<edge_type> &preds_list, R &&preds, Ts &&...args) {
    preds_list.insert(preds_list.end(), std::ranges::begin(preds), std::ranges::end(preds));
    return add_inplace_impl<U>(preds_list, std::forward<Ts>(args)...);
  }

  template <typename U, range_of<node_type> R, typename... Ts>
  node_type add_inplace_impl(std::vector<edge_type> &preds_list, R &&preds, Ts &&...args) {
    for (auto const &pred : preds) {
      preds_list.emplace_back(pred, 0); // Add with default port 0
    }
    return add_inplace_impl<U>(preds_list, std::forward<Ts>(args)...);
  }

  // add inplace - ctor

  template <typename U, typename... Ts>
  node_type add_inplace_impl(std::vector<edge_type> &preds_list, Ts &&...args) {
    auto node = std::make_shared<U>(std::forward<Ts>(args)...);
    add(node, preds_list);
    return node;
  }

  template <typename U, typename... Ts>
  node_type add_inplace_impl(std::vector<edge_type> &preds_list, ctor_args_tag, Ts &&...args) {
    auto node = std::make_shared<U>(std::forward<Ts>(args)...);
    add(node, preds_list);
    return node;
  }

  // add edge node -> [pred:port]
  void add_edge_impl(node_type const &node, edge_type const &pred) {
    predecessor[node].emplace(pred.node);
    argmap[node].emplace_back(pred);
    successor[pred.node].emplace(node);
  }

  // remove all edges node -> [pred:port]
  bool rm_edge_impl(node_type const &node, node_type const &pred, uint32_t port) {
    auto &args = argmap[node];
    auto rm = make_edge(pred, port);

    if (std::find(args.begin(), args.end(), rm) == args.end()) {
      return false; // Edge doesn't exist, nothing to remove
    }

    // Remove all edges [pred:port]
    std::erase(args, rm);

    // Cleanup adjacency maps
    cleanup_adj(node, pred);

    return true;
  }

  void cleanup_adj(node_type const &node, node_type const &pred) {
    auto const &args = argmap[node];
    bool has_conn = std::any_of(args.begin(), args.end(), [&](auto const &arg) { return arg.node == pred; });
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
  NodeMap predecessor;           ///< Adjacency list: node -> [pred] i.e. set of nodes that it depends on
  NodeArgsMap argmap;            ///< node -> [pred:port] i.e. args for calling this op node, order preserved
  NodeMap successor;             ///< Reverse adjacency list: node -> [succ] i.e. set of nodes that depend on it
  std::vector<node_type> output; ///< Output nodes
};
} // namespace opflow
