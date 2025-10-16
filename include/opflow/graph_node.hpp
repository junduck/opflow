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
    args_set args;

    add_delegate(graph_node &self, shared_node_ptr node) : self(self), node(std::move(node)), args() {}

    template <typename... Ts>
    void add_args(shared_node_ptr pred, Ts &&...t) {
      args.emplace_back(std::move(pred), 0); // Add with default port 0
      add_args(std::forward<Ts>(t)...);
    }

    template <range_of<shared_node_ptr> R, typename... Ts>
    void add_args(R &&preds, Ts &&...t) {
      for (auto const &pred : preds) {
        args.emplace_back(pred, 0); // Add with default port 0
      }
      add_args(std::forward<Ts>(t)...);
    }

    template <typename... Ts>
    void add_args(edge_type edge, Ts &&...t) {
      args.emplace_back(std::move(edge));
      add_args(std::forward<Ts>(t)...);
    }

    template <range_of<edge_type> R, typename... Ts>
    void add_args(R &&edges, Ts &&...t) {
      for (auto const &edge : edges) {
        args.emplace_back(edge);
      }
      add_args(std::forward<Ts>(t)...);
    }

    void add_args() {} // base case

    void check() const {
      for (auto const &arg : args) {
        if (arg.node == self.aux_node) {
          throw std::invalid_argument("Cannot depend on auxiliary node.");
        }
        if (arg.node == self.supp_node) {
          throw std::invalid_argument("Cannot depend on supplementary root node.");
        }
      }
      self.check_node(node);
    }

  public:
    template <typename... Ts>
    shared_node_ptr depends(Ts &&...pred) && {
      add_args(std::forward<Ts>(pred)...);

      check();
      self.add_edge_impl(node, args);
      return node;
    }
  };

  class aux_delegate {
    friend class graph_node;

    graph_node &self;

    shared_node_ptr node;
    port_set args;

    aux_delegate(graph_node &self, shared_node_ptr node) : self(self), node(std::move(node)), args() {}

    template <std::integral T0, typename... Ts>
    void add_ports(T0 &&port, Ts &&...t) {
      args.emplace_back(port);
      add_ports(std::forward<Ts>(t)...);
    }

    template <range_of<u32> R, typename... Ts>
    void add_ports(R &&ports, Ts &&...t) {
      for (auto const &port : ports) {
        args.emplace_back(port);
      }
      add_ports(std::forward<Ts>(t)...);
    }

    void add_ports() {} // base case

    void check() const { self.check_node(node); }

  public:
    template <typename... Ts>
    shared_node_ptr depends(Ts &&...pred_ports) && {
      add_ports(std::forward<Ts>(pred_ports)...);

      check();
      self.add_aux_impl(node, std::move(args));
      return node;
    }
  };

  // Constructed nodes are used as key_type so we can't provide fluent interface

  auto add(shared_node_ptr const &node) { return add_delegate(*this, node); }

  template <typename Node, typename... Ts>
  auto add(Ts &&...args) {
    return add(std::make_shared<Node>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Node, typename... Ts>
  auto add(Ts &&...args)
    requires(!std::is_void_v<DefaultDT>) // Template shorthand for ops with default data type
  {
    return add<Node<DefaultDT>>(std::forward<Ts>(args)...);
  }

  shared_node_ptr node(key_type const &node) const { return contains(node) ? node : nullptr; }

  auto aux(shared_node_ptr const &node) {
    if (aux_node) {
      throw std::invalid_argument("Auxiliary node already exists in graph.");
    }
    return aux_delegate(*this, node);
  }

  template <typename Aux, typename... Ts>
  auto aux(Ts &&...args) {
    return aux(std::make_shared<Aux>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Aux, typename... Ts>
  auto aux(Ts &&...args)
    requires(!std::is_void_v<DefaultDT>) // Template shorthand for ops with default data type
  {
    return aux<Aux<DefaultDT>>(std::forward<Ts>(args)...);
  }

  shared_node_ptr aux() const noexcept { return aux_node; }

  port_set const &aux_args() const noexcept { return aux_args_; }

  shared_node_ptr root(shared_node_ptr const &node) {
    if (root_node) {
      throw std::invalid_argument("Root node already exists in graph.");
    }
    check_node(node);
    ensure_node(node);
    root_node = node;
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
    if (supp_node) {
      throw std::invalid_argument("Supplementary root node already exists in graph.");
    }
    check_node(node);
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
  void supp_link(key_type const &node, Ts &&...ports) {
    port_set port_list{};
    supp_link_impl(port_list, node, std::forward<Ts>(ports)...);
  }

  supp_map const &supp_link() const noexcept { return supp_link_; }

  port_set const &supp_link_of(key_type const &node) const noexcept {
    static port_set const empty_set{};
    auto it = supp_link_.find(node);
    return (it != supp_link_.end()) ? it->second : empty_set;
  }

  // Output

  template <typename... Ts>
  graph_node &add_output(Ts &&...outputs) {
    add_output_impl(std::forward<Ts>(outputs)...);
    return *this;
  }

  args_set const &output() const noexcept { return soutput_; }

  // Utilities

  size_t size() const noexcept { return pred_.size(); }
  bool empty() const noexcept { return pred_.empty(); }
  bool contains(key_type const &node) const noexcept { return pred_.contains(node); }

  void clear() noexcept {
    pred_.clear();
    args_.clear();
    succ_.clear();
    soutput_.clear();

    root_node.reset();
    aux_node.reset();
    supp_node.reset();

    aux_args_.clear();
    supp_link_.clear();
  }

  key_set const &pred_of(shared_node_ptr const &node) const {
    static key_set const empty_set{};
    auto it = pred_.find(node);
    return (it != pred_.end()) ? it->second : empty_set;
  }

  key_set const &succ_of(shared_node_ptr const &node) const {
    static key_set const empty_set{};
    auto it = succ_.find(node);
    return (it != succ_.end()) ? it->second : empty_set;
  }

  args_set const &args_of(shared_node_ptr const &node) const {
    static args_set const empty_set{};
    auto it = args_.find(node);
    return (it != args_.end()) ? it->second : empty_set;
  }

  node_map const &pred() const noexcept { return pred_; }
  node_map const &succ() const noexcept { return succ_; }
  args_map const &args() const noexcept { return args_; }

  bool is_root(shared_node_ptr const &node) const {
    auto it = pred_.find(node);
    return (it != pred_.end() && it->second.empty());
  }

  bool is_leaf(shared_node_ptr const &node) const {
    auto it = succ_.find(node);
    return (it != succ_.end() && it->second.empty());
  }

  auto roots() const {
    std::vector<shared_node_ptr> r;
    for (auto [node, preds] : pred_) {
      if (preds.empty()) {
        r.push_back(node);
      }
    }
    return r;
  }

  auto leaves() const {
    std::vector<shared_node_ptr> l;
    for (auto [node, succs] : succ_) {
      if (succs.empty()) {
        l.push_back(node);
      }
    }
    return l;
  }

  bool validate() const noexcept {
    for (auto const &o : soutput_) {
      if (pred_.find(o.node) == pred_.end()) {
        return false; // Inconsistent: output node missing in predecessor map
      }
    }
    for (auto const &[node, _] : supp_link_) {
      if (pred_.find(node) == pred_.end()) {
        return false; // Inconsistent: supp link node missing in predecessor map
      }
    }
    return true;
  }

private:
  // add slot for node
  void ensure_node(key_type const &node) {
    if (!pred_.contains(node)) {
      pred_.emplace(node, key_set{});
    }
    if (!args_.contains(node)) {
      args_.emplace(node, args_set{});
    }
    if (!succ_.contains(node)) {
      succ_.emplace(node, key_set{});
    }
  }

  void check_node(key_type const &node) {
    if (!node) {
      throw std::invalid_argument("Null node.");
    }
    if (pred_.contains(node) || node == supp_node || node == aux_node) {
      throw std::invalid_argument("Node already exists.");
    }
  }

  // edge

  void add_edge_impl(key_type const &node, args_set const &edges) {
    ensure_node(node);
    for (auto const &edge : edges) {
      ensure_node(edge.node);
      pred_[node].emplace(edge.node);
      args_[node].emplace_back(edge);
      succ_[edge.node].emplace(node);
    }
  }

  void add_aux_impl(key_type const &node, port_set port_list) {
    aux_node = node;
    aux_args_ = std::move(port_list);
  }

  // supp link

  template <std::integral T0, typename... Ts>
  void supp_link_impl(port_set &port_list, key_type const &node, T0 &&port, Ts &&...args) {
    port_list.emplace_back(port);
    supp_link_impl(port_list, node, std::forward<Ts>(args)...);
  }

  template <range_of<u32> R, typename... Ts>
  void supp_link_impl(port_set &port_list, key_type const &node, R &&ports, Ts &&...args) {
    for (auto const &port : ports) {
      port_list.emplace_back(port);
    }
    supp_link_impl(port_list, node, std::forward<Ts>(args)...);
  }

  void supp_link_impl(port_set &port_list, key_type const &node) {
    supp_link_.insert_or_assign(node, std::move(port_list));
  }

  // output

  template <typename... Ts>
  void add_output_impl(key_type output, Ts &&...args) {
    soutput_.emplace_back(std::move(output), 0);
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <range_of<key_type> R, typename... Ts>
  void add_output_impl(R &&outputs, Ts &&...args) {
    for (auto const &output : outputs) {
      soutput_.emplace_back(output, 0);
    }
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <typename... Ts>
  void add_output_impl(edge_type output, Ts &&...args) {
    soutput_.emplace_back(output);
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <range_of<edge_type> R, typename... Ts>
  void add_output_impl(R &&outputs, Ts &&...args) {
    for (auto const &edge : outputs) {
      soutput_.emplace_back(edge);
    }
    add_output_impl(std::forward<Ts>(args)...);
  }

  void add_output_impl() {}

protected:
  node_map pred_;    ///< Adjacency list: node -> [pred] i.e. set of nodes that it depends on
  args_map args_;    ///< node -> [pred:port] i.e. args for calling this op node, order preserved
  node_map succ_;    ///< Reverse adjacency list: node -> [succ] i.e. set of nodes that depend on it
  args_set soutput_; ///< Output [node:port]

  shared_node_ptr root_node; ///< Root node
  shared_node_ptr aux_node;  ///< Auxiliary node
  shared_node_ptr supp_node; ///< Supplementary root node

  port_set aux_args_;  ///< Auxiliary args [root:port]
  supp_map supp_link_; ///< node -> [supp:port]
};
} // namespace opflow
