#pragma once

#include <memory>
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

template <typename T, typename AUX = void>
class graph_node {
  using Equal = std::equal_to<>; ///< transparent equality for pointer to T

public:
  using key_type = std::shared_ptr<T>;  ///< key type, shared_ptr to T
  using key_hash = detail::ptr_hash<T>; ///< transparent hashing for pointer to T

  using node_type = T;
  using shared_node_ptr = std::shared_ptr<T>; ///< node type, shared_ptr to T

  using aux_type = AUX;
  using shared_aux_ptr = std::shared_ptr<AUX>;

  using edge_type = detail::graph_node_edge<shared_node_ptr>;           ///< edge type, represents an arg passed to node
  using NodeSet = std::unordered_set<shared_node_ptr, key_hash, Equal>; ///< set of nodes
  using NodeArgsSet = std::vector<edge_type>;                           ///< set of node arguments
  using NodeMap = std::unordered_map<shared_node_ptr, NodeSet, key_hash, Equal>;         ///< node -> adjacent nodes
  using NodeArgsMap = std::unordered_map<shared_node_ptr, NodeArgsSet, key_hash, Equal>; ///< node -> call arguments

  // Add

  template <typename... Ts>
  void add(shared_node_ptr const &node, Ts &&...preds) {
    if (!node)
      return;

    std::vector<edge_type> preds_list{};
    preds_list.reserve(sizeof...(preds));
    add_impl(preds_list, node, std::forward<Ts>(preds)...);
  }

  // In-place construction

  template <typename U, typename... Args>
  shared_node_ptr add(Args &&...preds_and_args) {
    std::vector<edge_type> preds_list{};
    preds_list.reserve(sizeof...(preds_and_args));
    return add_inplace_impl<U>(preds_list, std::forward<Args>(preds_and_args)...);
  }

  template <template <typename> typename U, typename... Args>
  shared_node_ptr add(Args &&...preds_and_args)
    requires(detail::has_data_type<T>)
  {
    using UT = U<typename T::data_type>;
    std::vector<edge_type> preds_list{};
    preds_list.reserve(sizeof...(preds_and_args));
    return add_inplace_impl<UT>(preds_list, std::forward<Args>(preds_and_args)...);
  }

  shared_node_ptr root(size_t root_input_size)
    requires(dag_node_base<T>)
  {
    shared_node_ptr node = std::make_shared<dag_root_type<T>>(root_input_size);
    add(node);
    return node;
  }

  template <typename U, typename... Args>
  shared_node_ptr root(Args &&...args) {
    shared_node_ptr node = std::make_shared<U>(std::forward<Args>(args)...);
    add(node);
    return node;
  }

  template <template <typename> typename U, typename... Args>
  shared_node_ptr root(Args &&...args)
    requires(detail::has_data_type<T>)
  {
    using UT = U<typename T::data_type>;
    shared_node_ptr node = std::make_shared<UT>(std::forward<Args>(args)...);
    add(node);
    return node;
  }

  // Output

  template <typename... Ts>
  void add_output(Ts &&...outputs) {
    add_output_impl(std::forward<Ts>(outputs)...);
  }

  template <typename... Ts>
  void set_output(Ts &&...outputs) {
    out.clear();
    add_output_impl(std::forward<Ts>(outputs)...);
  }

  // Auxiliary data

  template <typename A, typename... Ts>
  void set_aux(Ts &&...preds_and_ctor_args)
    requires(!std::is_void_v<AUX>)
  {
    auxiliary_args.clear();
    set_aux_impl<A>(std::forward<Ts>(preds_and_ctor_args)...);
  }

  template <template <typename> typename A, typename... Ts>
  void set_aux(Ts &&...preds_and_ctor_args)
    requires(!std::is_void_v<AUX> && detail::has_data_type<T>) // CONVENIENT FN FOR OP DO NOT TEST
  {
    using aux_t = A<typename T::data_type>;
    auxiliary_args.clear();
    set_aux_impl<aux_t>(std::forward<Ts>(preds_and_ctor_args)...);
  }

  // Edge manipulation

  template <range_of<shared_node_ptr> R>
  void add_edge(shared_node_ptr const &node, R &&preds) {
    for (auto const &pred : preds) {
      add_edge_impl(node, pred, 0);
    }
  }

  template <range_of<edge_type> R>
  void add_edge(shared_node_ptr const &node, R &&preds) {
    for (auto const &[pred, port] : preds) {
      add_edge_impl(node, pred, port);
    }
  }

  void add_edge(shared_node_ptr const &node, shared_node_ptr const &pred) { add_edge_impl(node, pred, 0); }

  void add_edge(shared_node_ptr const &node, edge_type const &pred) { add_edge_impl(node, pred.node, pred.port); }

  // Replacement

  bool replace(shared_node_ptr const &new_node, shared_node_ptr const &old_node) {
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

    // Update output
    for (auto &o : out) {
      if (o.node == old_node) {
        o.node = new_node;
      }
    }

    // Update auxiliary args
    for (auto &edge : auxiliary_args) {
      if (edge.node == old_node) {
        edge.node = new_node;
      }
    }

    // Remove old_node from all maps
    predecessor.erase(old_node);
    argmap.erase(old_node);
    successor.erase(old_node);

    return true;
  }

  bool replace(shared_node_ptr const &node, edge_type const &old_pred, edge_type const &new_pred) {
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
    out.clear();
  }

  bool contains(shared_node_ptr const &node) const noexcept { return predecessor.find(node) != predecessor.end(); }

  NodeSet const &pred_of(shared_node_ptr const &node) const {
    static NodeSet const empty_set{};
    auto it = predecessor.find(node);
    return (it != predecessor.end()) ? it->second : empty_set;
  }

  NodeMap const &pred() const noexcept { return predecessor; }

  NodeArgsSet const &args_of(shared_node_ptr const &node) const {
    static NodeArgsSet const empty_set{};
    auto it = argmap.find(node);
    return (it != argmap.end()) ? it->second : empty_set;
  }

  NodeArgsMap const &args() const noexcept { return argmap; }

  NodeSet const &succ_of(shared_node_ptr const &node) const {
    static NodeSet const empty_set{};
    auto it = successor.find(node);
    return (it != successor.end()) ? it->second : empty_set;
  }

  NodeMap const &succ() const noexcept { return successor; }

  NodeArgsSet const &output() const noexcept { return out; }

  shared_aux_ptr aux() const noexcept { return auxiliary; }

  NodeArgsSet const &aux_args() const noexcept { return auxiliary_args; }

  shared_node_ptr node(key_type const &node) const {
    if (predecessor.find(node) == predecessor.end()) {
      return nullptr; // Node doesn't exist
    }
    return node;
  }

  bool is_root(shared_node_ptr const &node) const {
    auto it = predecessor.find(node);
    return (it != predecessor.end() && it->second.empty());
  }

  bool is_leaf(shared_node_ptr const &node) const {
    auto it = successor.find(node);
    return (it != successor.end() && it->second.empty());
  }

  auto roots() const {
    std::vector<shared_node_ptr> roots;
    for (auto [node, preds] : predecessor) {
      if (preds.empty()) {
        roots.push_back(node);
      }
    }
    return roots;
  }

  auto leaves() const {
    std::vector<shared_node_ptr> leaves;
    for (auto [node, succs] : successor) {
      if (succs.empty()) {
        leaves.push_back(node);
      }
    }
    return leaves;
  }

  bool validate() const noexcept {
    for (auto const &o : out) {
      if (predecessor.find(o.node) == predecessor.end()) {
        return false; // Inconsistent: output node missing in predecessor map
      }
    }
    if constexpr (!std::is_void_v<AUX>) {
      for (auto const &edge : auxiliary_args) {
        if (predecessor.find(edge.node) == predecessor.end()) {
          return false; // Inconsistent: aux arg node missing in predecessor map
        }
      }
    }
    return true;
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
  void ensure_node(shared_node_ptr const &node) {
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
  void add_impl(std::vector<edge_type> &preds_list, shared_node_ptr const &node, R &&preds, Ts &&...args) {
    preds_list.insert(preds_list.end(), std::ranges::begin(preds), std::ranges::end(preds));
    add_impl(preds_list, node, std::forward<Ts>(args)...);
  }

  template <typename... Ts, range_of<shared_node_ptr> R>
  void add_impl(std::vector<edge_type> &preds_list, shared_node_ptr const &node, R &&preds, Ts &&...args) {
    for (auto const &pred : preds) {
      preds_list.emplace_back(pred, 0); // Add with default port 0
    }
    add_impl(preds_list, node, std::forward<Ts>(args)...);
  }

  template <typename... Ts>
  void add_impl(std::vector<edge_type> &preds_list, shared_node_ptr const &node, Ts &&...preds) {
    (preds_list.emplace_back(std::forward<Ts>(preds)), ...);

    ensure_node(node);
    for (auto const &pred : preds_list) {
      ensure_node(pred.node);
      add_edge_impl(node, pred);
    }
  }

  // add inplace - edge

  template <typename U, typename... Ts>
  shared_node_ptr add_inplace_impl(std::vector<edge_type> &preds_list, edge_type pred, Ts &&...args) {
    preds_list.emplace_back(pred);
    return add_inplace_impl<U>(preds_list, std::forward<Ts>(args)...);
  }

  template <typename U, typename... Ts>
  shared_node_ptr add_inplace_impl(std::vector<edge_type> &preds_list, shared_node_ptr pred, Ts &&...args) {
    preds_list.emplace_back(pred, 0); // Add with default port 0
    return add_inplace_impl<U>(preds_list, std::forward<Ts>(args)...);
  }

  template <typename U, range_of<edge_type> R, typename... Ts>
  shared_node_ptr add_inplace_impl(std::vector<edge_type> &preds_list, R &&preds, Ts &&...args) {
    preds_list.insert(preds_list.end(), std::ranges::begin(preds), std::ranges::end(preds));
    return add_inplace_impl<U>(preds_list, std::forward<Ts>(args)...);
  }

  template <typename U, range_of<shared_node_ptr> R, typename... Ts>
  shared_node_ptr add_inplace_impl(std::vector<edge_type> &preds_list, R &&preds, Ts &&...args) {
    for (auto const &pred : preds) {
      preds_list.emplace_back(pred, 0); // Add with default port 0
    }
    return add_inplace_impl<U>(preds_list, std::forward<Ts>(args)...);
  }

  // add inplace - ctor

  template <typename U, typename... Ts>
  shared_node_ptr add_inplace_impl(std::vector<edge_type> &preds_list, Ts &&...args) {
    auto node = std::make_shared<U>(std::forward<Ts>(args)...);
    add_impl(preds_list, node);
    return node;
  }

  template <typename U, typename... Ts>
  shared_node_ptr add_inplace_impl(std::vector<edge_type> &preds_list, ctor_args_tag, Ts &&...args) {
    auto node = std::make_shared<U>(std::forward<Ts>(args)...);
    add_impl(preds_list, node);
    return node;
  }

  // output

  template <typename... Ts>
  void add_output_impl(edge_type output, Ts &&...args) {
    out.emplace_back(output);
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <typename... Ts>
  void add_output_impl(shared_node_ptr output, Ts &&...args) {
    out.emplace_back(output, 0);
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <range_of<edge_type> R, typename... Ts>
  void add_output_impl(R &&outputs, Ts &&...args) {
    for (auto const &edge : outputs) {
      out.emplace_back(edge);
    }
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <range_of<shared_node_ptr> R, typename... Ts>
  void add_output_impl(R &&outputs, Ts &&...args) {
    for (auto const &output : outputs) {
      out.emplace_back(output, 0);
    }
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <typename... Ts>
  void add_output_impl() {} // base case

  // set_aux - edge

  template <typename A, typename... Ts>
  void set_aux_impl(edge_type edge, Ts &&...args) {
    auxiliary_args.emplace_back(edge);
    set_aux_impl<A>(std::forward<Ts>(args)...);
  }

  template <typename A, typename... Ts>
  void set_aux_impl(shared_node_ptr const &node, Ts &&...args) {
    auto edge = detail::graph_node_edge(node);
    auxiliary_args.emplace_back(edge);
    set_aux_impl<A>(std::forward<Ts>(args)...);
  }

  template <typename A, range_of<edge_type> R, typename... Ts>
  void set_aux_impl(R &&edges, Ts &&...args) {
    for (auto const &edge : edges) {
      auxiliary_args.emplace_back(edge);
    }
    set_aux_impl<A>(std::forward<Ts>(args)...);
  }

  template <typename A, range_of<shared_node_ptr> R, typename... Ts>
  void set_aux_impl(R &&nodes, Ts &&...args) {
    for (auto const &node : nodes) {
      auto edge = detail::graph_node_edge(node);
      auxiliary_args.emplace_back(edge);
    }
    set_aux_impl<A>(std::forward<Ts>(args)...);
  }

  // set_aux - ctor

  template <typename A, typename... Ts>
  void set_aux_impl(Ts &&...args) {
    auxiliary = std::make_shared<A>(std::forward<Ts>(args)...);
  }

  template <typename A, typename... Ts>
  void set_aux_impl(ctor_args_tag, Ts &&...args) {
    auxiliary = std::make_shared<A>(std::forward<Ts>(args)...);
  }

  // add edge node -> [pred:port]
  void add_edge_impl(shared_node_ptr const &node, edge_type const &pred) {
    predecessor[node].emplace(pred.node);
    argmap[node].emplace_back(pred);
    successor[pred.node].emplace(node);
  }

  void cleanup_adj(shared_node_ptr const &node, shared_node_ptr const &pred) {
    auto const &args = argmap[node];
    bool has_conn = std::any_of(args.begin(), args.end(), [&](auto const &arg) { return arg.node == pred; });
    if (!has_conn) {
      // If no other connections exist, remove old predecessor from adjacency maps
      predecessor[node].erase(pred);
      successor[pred].erase(node);
    }
  }

protected:
  NodeMap predecessor;        ///< Adjacency list: node -> [pred] i.e. set of nodes that it depends on
  NodeArgsMap argmap;         ///< node -> [pred:port] i.e. args for calling this op node, order preserved
  NodeMap successor;          ///< Reverse adjacency list: node -> [succ] i.e. set of nodes that depend on it
  NodeArgsSet out;            ///< Output [node:port]
  shared_aux_ptr auxiliary;   ///< Auxiliary data
  NodeArgsSet auxiliary_args; ///< Auxiliary args
};
} // namespace opflow
