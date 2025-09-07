#pragma once

#include <cassert>
#include <charconv>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common.hpp"
#include "detail/utils.hpp"

namespace opflow {
namespace detail {
struct graph_named_edge {
  std::string name;
  uint32_t port;

  graph_named_edge(std::string_view name, uint32_t port) : name(name), port(port) {}

  graph_named_edge(std::string_view desc) {
    // find last dot
    auto dot_pos = desc.find_last_of('.');
    if (dot_pos == std::string_view::npos) {
      name = desc;
      port = 0;
    } else {
      name = desc.substr(0, dot_pos);
      auto port_str = desc.substr(dot_pos + 1);
      auto [_, ec] = std::from_chars(port_str.data(), port_str.data() + port_str.size(), port);
      if (ec != std::errc{}) {
        switch (ec) {
        case std::errc::invalid_argument:
          // A.B
          name = desc;
          port = 0;
          break;
        case std::errc::result_out_of_range:
          throw std::out_of_range("Port number out of range in edge description: " + std::string(desc));
        default:
          throw std::runtime_error("Unknown error parsing port number in edge description: " + std::string(desc));
        }
      }
    }
  }

  operator std::string() const {
    if (port == 0) {
      return name;
    } else {
      return name + "." + std::to_string(port);
    }
  }

  bool operator==(graph_named_edge const &) const noexcept = default;
};
} // namespace detail

inline auto make_edge(std::string_view name, uint32_t port = 0) { return detail::graph_named_edge(name, port); }

template <typename T>
class graph_named {
  using Hash = detail::str_hash;
  using Equal = std::equal_to<>;

public:
  using str_view = std::string_view;
  using key_type = std::string;
  using node_type = std::shared_ptr<T>;
  using edge_type = detail::graph_named_edge;
  using NodeSet = std::unordered_set<key_type, Hash, Equal>;
  using NodeArgsSet = std::vector<edge_type>;
  using NodeMap = std::unordered_map<key_type, NodeSet, Hash, Equal>;
  using NodeArgsMap = std::unordered_map<key_type, NodeArgsSet, Hash, Equal>;
  using NodeStore = std::unordered_map<key_type, node_type, Hash, Equal>;

  template <typename Node, typename... Ts>
  graph_named &add(key_type const &name, Ts &&...preds_and_ctor_args) {
    ensure_adjacency_list(name);
    add_impl<Node>(name, std::forward<Ts>(preds_and_ctor_args)...);
    return *this;
  }

  template <typename Node, range_of<edge_type> R, typename... Ts>
  graph_named &add(key_type const &name, R &&preds, Ts &&...args) {
    ensure_adjacency_list(name);
    add_impl<Node>(name, ctor_args, std::forward<Ts>(args)...);

    for (auto const &pred : preds) {
      ensure_adjacency_list(pred.name);
      add_edge_impl(name, pred);
    }
    return *this;
  }

  template <typename Node, range_of<str_view> R, typename... Ts>
  graph_named &add(key_type const &name, R &&preds, Ts &&...args) {
    ensure_adjacency_list(name);
    add_impl<Node>(name, ctor_args, std::forward<Ts>(args)...);

    for (auto const &pred : preds) {
      ensure_adjacency_list(pred);
      add_edge_impl(name, edge_type(str_view(pred)));
    }
    return *this;
  }

  template <template <typename> typename Node, typename... Ts>
  graph_named &add(key_type const &name, Ts &&...preds_and_ctor_args)
    requires(detail::has_data_type<T>) // CONVENIENT FN FOR OP DO NOT TEST
  {
    using NodeT = Node<typename T::data_type>;
    add<NodeT>(name, std::forward<Ts>(preds_and_ctor_args)...);
    return *this;
  }

  graph_named &root(key_type const &name, size_t root_input_size)
    requires(dag_node_base<T>) // CONVENIENT FN FOR OP DO NOT TEST
  {
    add<dag_root_type<T>>(name, root_input_size);
    return *this;
  }

  template <typename Root, typename... Ts>
  graph_named &root(key_type const &name, Ts &&...args) {
    add<Root>(name, ctor_args, std::forward<Ts>(args)...);
    return *this;
  }

  template <template <typename> typename Root, typename... Ts>
  graph_named &root(key_type const &name, Ts &&...args)
    requires(detail::has_data_type<T>) // CONVENIENT FN FOR OP DO NOT TEST
  {
    using RootT = Root<typename T::data_type>;
    add<RootT>(name, ctor_args, std::forward<Ts>(args)...);
    return *this;
  }

  template <range_of<str_view> R>
  graph_named &add_output(R &&outputs) {
    for (auto const &name : outputs) {
      output.emplace_back(name);
    }
    return *this;
  }

  graph_named &add_output(key_type const &name) {
    output.push_back(name);
    return *this;
  }

  template <range_of<str_view> R>
  graph_named &set_output(R &&outputs) {
    output.clear();
    add_output(std::forward<R>(outputs));
    return *this;
  }

  // Removal

  bool rm(key_type const &name) {
    if (predecessor.find(name) == predecessor.end()) {
      return false; // Node doesn't exist
    }

    // Remove this node from pred map of all successors
    for (auto const &succ : successor[name]) {
      predecessor[succ].erase(name);
      [[maybe_unused]] auto n = std::erase_if(argmap[succ], [&](auto const &edge) { return edge.name == name; });
      assert(n && "[BUG] Inconsistent graph state.");
    }

    // Remove this node from succ map of all predecessors
    for (auto const &pred : predecessor[name]) {
      successor[pred].erase(name);
    }

    // Remove the node from adj maps and store
    predecessor.erase(name);
    argmap.erase(name);
    successor.erase(name);
    store.erase(name);

    return true;
  }

  // Edge manipulation

  template <range_of<edge_type> R>
  void add_edge(key_type const &name, R &&preds) {
    for (auto const &pred : preds) {
      add_edge_impl(name, pred);
    }
  }

  template <range_of<str_view> R>
  void add_edge(key_type const &name, R &&preds) {
    for (auto const &pred : preds) {
      add_edge_impl(name, edge_type(str_view(pred)));
    }
  }

  void add_edge(key_type const &name, edge_type const &pred) { add_edge_impl(name, pred); }

  void add_edge(key_type const &name, key_type const &pred) { add_edge_impl(name, edge_type(str_view(pred))); }

  template <range_of<edge_type> R>
  bool rm_edge(key_type const &name, R &&preds) {
    if (predecessor.find(name) == predecessor.end()) {
      return false;
    }
    bool rm = false;
    for (auto const &pred : preds) {
      rm |= rm_edge_impl(name, pred);
    }
    return rm;
  }

  template <range_of<str_view> R>
  bool rm_edge(key_type const &name, R &&preds) {
    if (predecessor.find(name) == predecessor.end()) {
      return false;
    }
    bool rm = false;
    for (auto const &pred : preds) {
      rm |= rm_edge_impl(name, edge_type(str_view(pred)));
    }
    return rm;
  }

  bool rm_edge(key_type const &name, edge_type const &pred) {
    if (predecessor.find(name) == predecessor.end()) {
      return false; // Node doesn't exist
    }
    return rm_edge_impl(name, pred);
  }

  bool rm_edge(key_type const &name, key_type const &pred) {
    if (predecessor.find(name) == predecessor.end()) {
      return false; // Node doesn't exist
    }
    return rm_edge_impl(name, edge_type(str_view(pred)));
  }

  // Replacement

  bool rename(key_type const &old_name, key_type const &new_name) {
    if (predecessor.find(old_name) == predecessor.end()) {
      return false; // Old node doesn't exist
    }
    if (predecessor.find(new_name) != predecessor.end()) {
      return false; // Can't replace with an existing node
    }
    if (old_name == new_name) {
      return true; // No change needed
    }

    // Copy all adjacency information from old_name to new_name
    predecessor.emplace(new_name, std::move(predecessor[old_name]));
    argmap.emplace(new_name, std::move(argmap[old_name]));
    successor.emplace(new_name, std::move(successor[old_name]));
    store.emplace(new_name, std::move(store[old_name]));

    // Update all predecessors to point to new_name instead of old_name
    for (auto const &pred : predecessor[new_name]) {
      successor[pred].erase(old_name);
      successor[pred].insert(new_name);
    }

    // Update all successors to depend on new_name instead of old_name
    for (auto const &succ : successor[new_name]) {
      predecessor[succ].erase(old_name);
      predecessor[succ].insert(new_name);

      // Update args to replace old_name with new_name
      for (auto &arg : argmap[succ]) {
        if (arg.name == old_name) {
          arg.name = new_name;
        }
      }
    }

    // Update output references
    for (auto &out : output) {
      if (out == old_name) {
        out = new_name;
      }
    }

    // Remove old_name from all maps
    predecessor.erase(old_name);
    argmap.erase(old_name);
    successor.erase(old_name);
    store.erase(old_name);

    return true;
  }

  template <typename Node, typename... Ts>
  bool replace(key_type const &old_node, key_type const &new_node, Ts &&...args) {
    if (predecessor.find(old_node) == predecessor.end()) {
      return false; // Old node doesn't exist
    }
    if (predecessor.find(new_node) != predecessor.end()) {
      return false; // Can't replace with an existing node
    }
    // Rename, so adjacency lists point to new_node
    rename(old_node, new_node);
    // Replace the stored node
    store[new_node] = std::make_shared<Node>(std::forward<Ts>(args)...);

    return true;
  }

  template <template <typename> typename Node, typename... Ts>
  bool replace(key_type const &old_node, key_type const &new_node, Ts &&...args)
    requires(detail::has_data_type<T>) // CONVENIENT FN FOR OP DO NOT TEST
  {
    using NodeT = Node<typename T::data_type>;
    return replace<NodeT>(old_node, new_node, std::forward<Ts>(args)...);
  }

  bool replace(key_type const &node, edge_type const &old_pred, edge_type const &new_pred) {
    if (predecessor.find(node) == predecessor.end()) {
      return false; // Node doesn't exist
    }
    if (predecessor.find(new_pred.name) == predecessor.end()) {
      return false; // Node doesn't exist
    }
    if (old_pred == new_pred) {
      return true; // No change needed
    }

    auto &args = argmap[node];

    if (std::find(args.begin(), args.end(), old_pred) == args.end()) {
      return false; // Old edge doesn't exist, nothing to replace
    }

    // Ensure new predecessor node exists in adjacency lists
    ensure_adjacency_list(new_pred.name);

    // Update adjacency maps
    predecessor[node].emplace(new_pred.name);
    successor[new_pred.name].emplace(node);

    // Replace old_pred with new_pred in argmap
    for (auto &arg : args) {
      if (arg.name == old_pred.name && arg.port == old_pred.port) {
        arg = new_pred;
      }
    }

    // Check if old_pred is still needed
    cleanup_adj(node, old_pred.name);

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
    store.clear();
  }

  bool contains(key_type const &name) const noexcept { return predecessor.find(name) != predecessor.end(); }

  NodeSet const &pred_of(key_type const &name) const {
    static NodeSet empty_set{};
    auto it = predecessor.find(name);
    return (it != predecessor.end()) ? it->second : empty_set;
  }

  NodeMap const &get_pred() const noexcept { return predecessor; }

  NodeArgsSet const &args_of(key_type const &name) const {
    static NodeArgsSet empty_set{};
    auto it = argmap.find(name);
    return (it != argmap.end()) ? it->second : empty_set;
  }

  NodeArgsMap const &get_args() const noexcept { return argmap; }

  NodeSet const &succ_of(key_type const &name) const {
    static NodeSet empty_set{};
    auto it = successor.find(name);
    return (it != successor.end()) ? it->second : empty_set;
  }

  NodeMap const &get_succ() const noexcept { return successor; }

  auto const &get_output() const noexcept { return output; }

  node_type get_node(key_type const &name) const {
    auto it = store.find(name);
    return (it != store.end()) ? it->second : nullptr;
  }

  bool is_root(key_type const &name) const {
    auto it = predecessor.find(name);
    return (it != predecessor.end() && it->second.empty());
  }

  bool is_leaf(key_type const &name) const {
    auto it = successor.find(name);
    return (it != successor.end() && it->second.empty());
  }

  auto get_roots() const {
    std::vector<key_type> roots;
    for (auto const &[name, preds] : predecessor) {
      if (preds.empty()) {
        roots.push_back(name);
      }
    }
    return roots;
  }

  auto get_leaves() const {
    std::vector<key_type> leaves;
    for (auto const &[name, succs] : successor) {
      if (succs.empty()) {
        leaves.push_back(name);
      }
    }
    return leaves;
  }

  bool validate() const noexcept {
    for (auto const &[name, preds] : predecessor) {
      for (auto const &pred : preds) {
        if (successor.find(pred) == successor.end() || successor.at(pred).find(name) == successor.at(pred).end()) {
          return false; // Inconsistent: pred missing in successor map
        }
      }
    }

    for (auto const &[name, succs] : successor) {
      for (auto const &succ : succs) {
        if (predecessor.find(succ) == predecessor.end() ||
            predecessor.at(succ).find(name) == predecessor.at(succ).end()) {
          return false; // Inconsistent: succ missing in predecessor map
        }
      }
    }

    for (auto const &[name, args] : argmap) {
      for (auto const &arg : args) {
        if (predecessor.find(name) == predecessor.end() ||
            predecessor.at(name).find(arg.name) == predecessor.at(name).end()) {
          return false; // Inconsistent: arg missing in predecessor map
        }
        if (successor.find(arg.name) == successor.end() ||
            successor.at(arg.name).find(name) == successor.at(arg.name).end()) {
          return false; // Inconsistent: arg missing in successor map
        }
      }
    }

    for (auto const &[name, _] : predecessor) {
      if (store.find(name) == store.end()) {
        return false; // Inconsistent: node missing in store
      }
    }
    for (auto const &[name, _] : successor) {
      if (store.find(name) == store.end()) {
        return false; // Inconsistent: node missing in store
      }
    }
    for (auto const &[name, _] : store) {
      if (predecessor.find(name) == predecessor.end()) {
        return false; // Inconsistent: node missing in predecessor map
      }
      if (successor.find(name) == successor.end()) {
        return false; // Inconsistent: node missing in successor map
      }
    }

    return true;
  }

  void merge(graph_named const &other) {
    if (!other.validate()) {
      throw std::invalid_argument("Cannot merge: other graph is invalid.");
    }

    // Collect new nodes to add
    NodeSet nodes_to_add{};
    for (auto const &[other_name, _] : other.predecessor) {
      if (!contains(other_name)) {
        nodes_to_add.emplace(other_name);
      }
    }

    // Add all new nodes to the graph
    for (auto const &new_name : nodes_to_add) {
      auto other_node = other.get_node(new_name);

      // Add edges
      ensure_adjacency_list(new_name);
      for (auto const &edge : other.args_of(new_name)) {
        add_edge_impl(new_name, edge);
      }

      // Copy the node from other's store
      store.emplace(new_name, other_node);
    }
  }

  graph_named &operator+=(graph_named const &rhs) {
    merge(rhs);
    return *this;
  }

  friend graph_named operator+(graph_named const &lhs, graph_named const &rhs) {
    graph_named result(lhs);
    result.merge(rhs);
    return result;
  }

private:
  void ensure_adjacency_list(key_type const &name) {
    if (predecessor.find(name) == predecessor.end()) {
      predecessor.emplace(name, NodeSet{});
    }
    if (argmap.find(name) == argmap.end()) {
      argmap.emplace(name, NodeArgsSet{});
    }
    if (successor.find(name) == successor.end()) {
      successor.emplace(name, NodeSet{});
    }
  }

  template <typename Node, detail::string_like T0, typename... Ts>
  void add_impl(key_type const &name, T0 &&edge_desc, Ts &&...args) {
    auto edge = detail::graph_named_edge(edge_desc);
    ensure_adjacency_list(edge.name);
    add_edge_impl(name, edge);

    add_impl<Node>(name, std::forward<Ts>(args)...);
  }

  template <typename Node, typename... Ts>
  void add_impl(key_type const &name, Ts &&...args) {
    store[name] = std::make_shared<Node>(std::forward<Ts>(args)...);
  }

  /*
  g.add<my_node>("node1", "input.0", "input.3", 2.3, v); // OK, no ambiguity since first arg to ctor is not string like
  g.add<my_node2>("node2", "input.1", ctor_args, "first_ctor_arg", 2.3, v); // Need explicit splitter for pack
  */
  template <typename Node, typename... Ts>
  void add_impl(key_type const &name, ctor_args_tag, Ts &&...args) {
    store[name] = std::make_shared<Node>(std::forward<Ts>(args)...);
  }

  void add_edge_impl(key_type const &name, edge_type const &pred) {
    predecessor[name].emplace(pred.name);
    argmap[name].emplace_back(pred);
    successor[pred.name].emplace(name);
  }

  // remove all edges name -> [pred:port]
  bool rm_edge_impl(key_type const &name, edge_type const &pred) {
    auto &args = argmap[name];

    if (std::find(args.begin(), args.end(), pred) == args.end()) {
      return false; // Edge doesn't exist, nothing to remove
    }

    // Remove all edges [pred:port]
    std::erase(args, pred);

    // Cleanup adjacency maps
    cleanup_adj(name, pred.name);

    return true;
  }

  void cleanup_adj(key_type const &name, key_type const &pred) {
    auto const &args = argmap[name];
    bool has_conn = std::any_of(args.begin(), args.end(), [&](auto const &arg) { return arg.name == pred; });
    if (!has_conn) {
      assert(predecessor[name].find(pred) != predecessor[name].end() &&
             "[BUG] Inconsistent graph state: predecessor not found in adj map.");
      assert(successor[pred].find(name) != successor[pred].end() &&
             "[BUG] Inconsistent graph state: successor not found in reverse_adj map.");
      // If no other connections exist, remove old predecessor from adjacency maps
      predecessor[name].erase(pred);
      successor[pred].erase(name);
    }
  }

protected:
  NodeMap predecessor;          ///< Adjacency list: node -> [pred] i.e. set of nodes that it depends on
  NodeArgsMap argmap;           ///< node -> [pred:port] i.e. args for calling this op node, order preserved
  NodeMap successor;            ///< Reverse adjacency list: node -> [succ] i.e. set of nodes that depend on it
  std::vector<key_type> output; ///< Output nodes
  NodeStore store;              ///< Store for actual node instances
};
} // namespace opflow
