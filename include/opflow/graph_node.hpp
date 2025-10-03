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
  u32 port;

  graph_node_edge(T const &node, u32 port) : node(node), port(port) {}
  graph_node_edge(T &&node, u32 port) : node(std::move(node)), port(port) {}

  explicit graph_node_edge(T const &node) : node(node), port(0) {}
  explicit graph_node_edge(T &&node) : node(std::move(node)), port(0) {}

  friend bool operator==(graph_node_edge const &lhs, graph_node_edge const &rhs) noexcept = default;
};
} // namespace detail

template <typename T>
auto operator|(std::shared_ptr<T> const &node, u32 port) {
  return detail::graph_node_edge<std::shared_ptr<T>>{node, port};
}

template <typename T>
auto make_edge(T const &node, u32 port = 0) {
  return detail::graph_node_edge<T>{node, port};
}

template <typename T, typename DefaultDT = void>
class graph_node {
  using Equal = std::equal_to<>; ///< transparent equality for pointer to T

public:
  using node_type = T;
  using shared_node_ptr = std::shared_ptr<T>; ///< node type, shared_ptr to T

  using key_type = shared_node_ptr;                           ///< key type, shared_ptr to T
  using key_hash = detail::ptr_hash<T>;                       ///< transparent hashing for pointer to T
  using edge_type = detail::graph_node_edge<shared_node_ptr>; ///< edge type, represents an arg passed to node

  using key_set = std::unordered_set<shared_node_ptr, key_hash, Equal>; ///< set of nodes
  using args_set = std::vector<edge_type>;                              ///< set of node arguments
  using port_set = std::vector<u32>;

  using node_map = std::unordered_map<key_type, key_set, key_hash, Equal>;  ///< node -> adjacent nodes
  using args_map = std::unordered_map<key_type, args_set, key_hash, Equal>; ///< node -> call arguments
  using supp_map = std::unordered_map<key_type, port_set, key_hash, Equal>; ///< node -> supp ports

  class add_delegate {
    friend class graph_node;

    graph_node &self;

    shared_node_ptr node;
    args_set pred_list;

    template <typename... Ts>
    void add_preds(shared_node_ptr pred, Ts &&...args) {
      pred_list.emplace_back(pred, 0); // Add with default port 0
      add_preds(std::forward<Ts>(args)...);
    }

    template <range_of<shared_node_ptr> R, typename... Ts>
    void add_preds(R &&preds, Ts &&...args) {
      for (auto const &pred : preds) {
        pred_list.emplace_back(pred, 0); // Add with default port 0
      }
      add_preds(std::forward<Ts>(args)...);
    }

    template <typename... Ts>
    void add_preds(edge_type edge, Ts &&...args) {
      pred_list.emplace_back(edge);
      add_preds(std::forward<Ts>(args)...);
    }

    template <range_of<edge_type> R, typename... Ts>
    void add_preds(R &&edges, Ts &&...args) {
      for (auto const &edge : edges) {
        pred_list.emplace_back(edge);
      }
      add_preds(std::forward<Ts>(args)...);
    }

    void add_preds() {} // base case

    add_delegate(graph_node &self, shared_node_ptr node) : self(self), node(std::move(node)), pred_list() {}

  public:
    template <typename... Ts>
    shared_node_ptr depends(Ts &&...pred) && {
      add_preds(std::forward<Ts>(pred)...);
      self.add_edge_impl(node, pred_list);
      return node;
    }
  };

  class aux_delegate {
    friend class graph_node;

    graph_node &self;

    shared_node_ptr node;
    port_set port_list;

    template <std::integral T0, typename... Ts>
    void add_ports(T0 &&port, Ts &&...args) {
      port_list.emplace_back(port);
      add_ports(std::forward<Ts>(args)...);
    }

    template <range_of<u32> R, typename... Ts>
    void add_ports(R &&ports, Ts &&...args) {
      for (auto const &port : ports) {
        port_list.emplace_back(port);
      }
      add_ports(std::forward<Ts>(args)...);
    }

    template <typename... Ts>
    void add_ports(edge_type edge, Ts &&...args) {
      if (edge.node != self.root_node) {
        throw std::invalid_argument("Auxiliary node can only depend on root node.");
      }
      port_list.emplace_back(edge.port);
      add_ports(std::forward<Ts>(args)...);
    }

    template <range_of<edge_type> R, typename... Ts>
    void add_ports(R &&edges, Ts &&...args) {
      for (auto const &edge : edges) {
        if (edge.node != self.root_node) {
          throw std::invalid_argument("Auxiliary node can only depend on root node.");
        }
        port_list.emplace_back(edge.port);
      }
      add_ports(std::forward<Ts>(args)...);
    }

    void add_ports() {} // base case

    aux_delegate(graph_node &self, shared_node_ptr node) : self(self), node(std::move(node)), port_list() {}

  public:
    template <typename... Ts>
    shared_node_ptr depends(Ts &&...pred) && {
      add_ports(std::forward<Ts>(pred)...);
      self.add_aux_impl(node, port_list);
      return node;
    }
  };

  // Add

  auto add(shared_node_ptr const &node) {
    if (!node)
      throw std::invalid_argument("Cannot add null node.");

    return add_delegate(*this, node);
  }

  template <typename Node, typename... Ts>
  auto add(Ts &&...args) {
    return add_delegate(*this, std::make_shared<Node>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Node, typename... Ts>
  auto add(Ts &&...args)
    requires(!std::is_void_v<DefaultDT>) // Template shorthand for ops with default data type
  {
    return add<Node<DefaultDT>>(std::forward<Ts>(args)...);
  }

  auto aux(shared_node_ptr const &node) {
    if (!node)
      throw std::invalid_argument("Cannot add null node.");

    return aux_delegate(*this, node);
  }

  template <typename Aux, typename... Ts>
  auto aux(Ts &&...args) {
    return aux_delegate(*this, std::make_shared<Aux>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Aux, typename... Ts>
  auto aux(Ts &&...args)
    requires(!std::is_void_v<DefaultDT>) // Template shorthand for ops with default data type
  {
    return aux<Aux<DefaultDT>>(std::forward<Ts>(args)...);
  }

  shared_node_ptr aux() const noexcept { return aux_node; }

  port_set const &aux_args() const noexcept { return aux_argmap; }

  shared_node_ptr root(shared_node_ptr const &node) {
    if (!node)
      throw std::invalid_argument("Cannot set null node as root.");
    root_node = node;
    ensure_node(node);
    return node;
  }

  template <typename Root, typename... Ts>
  shared_node_ptr root(Ts &&...args) {
    shared_node_ptr node = std::make_shared<Root>(std::forward<Ts>(args)...);
    return root(node);
  }

  template <template <typename> typename Root, typename... Ts>
  shared_node_ptr root(Ts &&...args)
    requires(!std::is_void_v<DefaultDT>) // Template shorthand for ops with default data type
  {
    return root<Root<DefaultDT>>(std::forward<Ts>(args)...);
  }

  shared_node_ptr root() const noexcept { return root_node; }

  shared_node_ptr supp_root(shared_node_ptr const &node) {
    if (!node)
      throw std::invalid_argument("Cannot set null node as supplementary root.");
    supp_node = node;
    return node;
  }

  template <typename Supp, typename... Ts>
  shared_node_ptr supp_root(Ts &&...args) {
    shared_node_ptr node = std::make_shared<Supp>(std::forward<Ts>(args)...);
    return supp_root(node);
  }

  template <template <typename> typename Supp, typename... Ts>
  shared_node_ptr supp_root(Ts &&...args)
    requires(!std::is_void_v<DefaultDT>) // Template shorthand for ops with default
  {
    return supp_root<Supp<DefaultDT>>(std::forward<Ts>(args)...);
  }

  shared_node_ptr supp_root() const noexcept { return supp_node; }

  template <typename... Ts>
  void supp_link(key_type const &node, Ts &&...preds) {
    if (!supp_node) {
      throw std::invalid_argument("Supplementary root node not set.");
    }
    port_set port_list{};
    supp_link_impl(node, port_list, std::forward<Ts>(preds)...);
  }

  supp_map const &supp_link() const noexcept { return supp_links; }

  port_set const &supp_args(key_type const &node) const noexcept {
    static port_set const empty_set{};
    return supp_links.contains(node) ? supp_links.at(node) : empty_set;
  }

  // Output

  template <typename... Ts>
  graph_node &add_output(Ts &&...outputs) {
    add_output_impl(std::forward<Ts>(outputs)...);
    return *this;
  }

  args_set const &output() const noexcept { return out; }

  // Utilities

  size_t size() const noexcept { return predecessor.size(); }

  bool empty() const noexcept { return predecessor.empty(); }

  void clear() noexcept {
    predecessor.clear();
    argmap.clear();
    successor.clear();
    out.clear();

    root_node.reset();

    aux_node.reset();
    aux_argmap.clear();

    supp_node.reset();
    supp_links.clear();
  }

  bool contains(shared_node_ptr const &node) const noexcept { return predecessor.find(node) != predecessor.end(); }

  key_set const &pred_of(shared_node_ptr const &node) const {
    static key_set const empty_set{};
    auto it = predecessor.find(node);
    return (it != predecessor.end()) ? it->second : empty_set;
  }

  node_map const &pred() const noexcept { return predecessor; }

  args_set const &args_of(shared_node_ptr const &node) const {
    static args_set const empty_set{};
    auto it = argmap.find(node);
    return (it != argmap.end()) ? it->second : empty_set;
  }

  args_map const &args() const noexcept { return argmap; }

  key_set const &succ_of(shared_node_ptr const &node) const {
    static key_set const empty_set{};
    auto it = successor.find(node);
    return (it != successor.end()) ? it->second : empty_set;
  }

  node_map const &succ() const noexcept { return successor; }

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
    return true;
  }

private:
  // add slot for node
  void ensure_node(key_type const &node) {
    if (!predecessor.contains(node)) {
      predecessor.emplace(node, key_set{});
    }
    if (!argmap.contains(node)) {
      argmap.emplace(node, args_set{});
    }
    if (!successor.contains(node)) {
      successor.emplace(node, key_set{});
    }
  }

  void check_node(key_type const &node) {
    if (!node) {
      throw std::invalid_argument("Null node.");
    }
    if (predecessor.contains(node)) {
      throw std::invalid_argument("Node already exists in graph.");
    }
  }

  // edge

  void add_edge_impl(key_type const &node, args_set const &edges) {
    ensure_node(node);
    for (auto const &edge : edges) {
      ensure_node(edge.node);
      predecessor[node].emplace(edge.node);
      argmap[node].emplace_back(edge);
      successor[edge.node].emplace(node);
    }
  }

  void add_aux_impl(key_type const &node, port_set const &ports) {
    aux_node = node;
    for (auto port : ports) {
      aux_argmap.emplace_back(port);
    }
  }

  // supp link

  template <std::integral T0, typename... Ts>
  void supp_link_impl(key_type const &node, port_set &port_list, T0 &&port, Ts &&...args) {
    port_list.emplace_back(port);
    supp_link_impl(node, port_list, std::forward<Ts>(args)...);
  }

  template <range_of<u32> R, typename... Ts>
  void supp_link_impl(key_type const &node, port_set &port_list, R &&ports, Ts &&...args) {
    for (auto const &port : ports) {
      port_list.emplace_back(port);
    }
    supp_link_impl(node, port_list, std::forward<Ts>(args)...);
  }

  template <typename... Ts>
  void supp_link_impl(key_type const &node, port_set &port_list, edge_type edge, Ts &&...args) {
    if (edge.node != supp_node) {
      throw std::invalid_argument("Supplementary link can only depend on supplementary root node.");
    }
    port_list.emplace_back(edge.port);
    supp_link_impl(node, port_list, std::forward<Ts>(args)...);
  }

  template <range_of<edge_type> R, typename... Ts>
  void supp_link_impl(key_type const &node, port_set &port_list, R &&edges, Ts &&...args) {
    for (auto const &edge : edges) {
      if (edge.node != supp_node) {
        throw std::invalid_argument("Supplementary link can only depend on supplementary root node.");
      }
      port_list.emplace_back(edge.port);
    }
    supp_link_impl(node, port_list, std::forward<Ts>(args)...);
  }

  void supp_link_impl(key_type const &node, port_set &port_list) {
    supp_links.insert_or_assign(node, std::move(port_list));
  }

  // output

  template <typename... Ts>
  void add_output_impl(key_type output, Ts &&...args) {
    out.emplace_back(output, 0);
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <range_of<key_type> R, typename... Ts>
  void add_output_impl(R &&outputs, Ts &&...args) {
    for (auto const &output : outputs) {
      out.emplace_back(output, 0);
    }
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <typename... Ts>
  void add_output_impl(edge_type output, Ts &&...args) {
    out.emplace_back(output);
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <range_of<edge_type> R, typename... Ts>
  void add_output_impl(R &&outputs, Ts &&...args) {
    for (auto const &edge : outputs) {
      out.emplace_back(edge);
    }
    add_output_impl(std::forward<Ts>(args)...);
  }

  void add_output_impl() {} // base case

protected:
  node_map predecessor; ///< Adjacency list: node -> [pred] i.e. set of nodes that it depends on
  args_map argmap;      ///< node -> [pred:port] i.e. args for calling this op node, order preserved
  node_map successor;   ///< Reverse adjacency list: node -> [succ] i.e. set of nodes that depend on it
  args_set out;         ///< Output [node:port]

  shared_node_ptr root_node; ///< Root node

  shared_node_ptr aux_node; ///< Auxiliary node
  port_set aux_argmap;      ///< Auxiliary args

  shared_node_ptr supp_node; ///< Supplementary root node
  supp_map supp_links;       ///< Supplementary succ (node <- set of depending ports)
};
} // namespace opflow
