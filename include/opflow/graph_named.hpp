#pragma once

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

    auto const dot_pos = desc.find_last_of('.');
    if (dot_pos == std::string_view::npos) {
      name = desc;
      port = 0;
      return;
    }

    name = desc.substr(0, dot_pos);
    auto const port_str = desc.substr(dot_pos + 1);

    auto [_, ec] = std::from_chars(port_str.data(), port_str.end(), port);
    if (ec == std::errc{})
      return;

    if (ec == std::errc::invalid_argument) {
      // Not a number after dot, treat as name only
      name = desc;
      port = 0;
      return;
    }

    throw std::system_error(std::make_error_code(ec));
  }

  operator std::string() const { return port == 0 ? name : name + "." + std::to_string(port); }
  bool operator==(graph_named_edge const &) const noexcept = default;
};
} // namespace detail

inline auto make_edge(std::string_view name, uint32_t port = 0) { return detail::graph_named_edge(name, port); }

/*
Example of a graph:

graph TD
    Root --> A[Node A]
    Root --> B[Node B]
    Root --> C[Node C]

    A --> D[Node D]
    A --> E[Node E]
    B --> F[Node F]
    C --> G[Node G]
    D --> H[Node H]

    %% Multiple nodes connecting to output
    E --> Output[Output]
    F --> Output
    G --> Output
    H --> Output

    %% Auxiliary node directly connected to Root
    Root --> Aux[Aux Node]
    Aux --> AuxOutput[Aux Output<br/>Clock/Logger/etc]

    %% Supplementary root forming star pattern
    SuppRoot[Supp Root<br/>Params/Signals/etc] --> A
    SuppRoot --> D
    SuppRoot --> F
    SuppRoot --> G
*/

template <typename T, typename DefaultDT = void>
class graph_named {
  using Equal = std::equal_to<>;
  using str_view = std::string_view;

public:
  using node_type = T;
  using shared_node_ptr = std::shared_ptr<T>;

  using key_type = std::string;
  using key_hash = detail::str_hash;
  using edge_type = detail::graph_named_edge;

  using key_set = std::unordered_set<key_type, key_hash, Equal>;
  using args_set = std::vector<edge_type>;
  using port_set = std::vector<u32>;

  using node_map = std::unordered_map<key_type, key_set, key_hash, Equal>;  ///< node -> preds/succs
  using args_map = std::unordered_map<key_type, args_set, key_hash, Equal>; ///< node -> args
  using supp_map = std::unordered_map<key_type, port_set, key_hash, Equal>; ///< node -> supp ports

  class add_delegate {
    friend class graph_named;

    graph_named &self;

    key_type node_name;
    shared_node_ptr node;
    args_set pred_list;

    add_delegate(graph_named &self, key_type name, shared_node_ptr node)
        : self(self), node_name(std::move(name)), node(std::move(node)), pred_list() {}

    template <detail::string_like T0, typename... Ts>
    void add_preds(T0 &&edge, Ts &&...args) {
      pred_list.emplace_back(self.parse_edge(std::forward<T0>(edge)));
      add_preds(std::forward<Ts>(args)...);
    }

    template <range_of<str_view> R, typename... Ts>
    void add_preds(R &&edges, Ts &&...args) {
      for (auto const &edge_desc : edges) {
        pred_list.emplace_back(self.parse_edge(edge_desc));
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

    void add_preds() {}

    void check_node_deps() {
      for (auto const &edge : pred_list) {
        if (edge.name == self.aux_name) {
          throw std::invalid_argument("Cannot depend on auxiliary node.");
        }
        if (edge.name == self.supp_name) {
          throw std::invalid_argument("Cannot depend on supplementary root node.");
        }
      }
    }

  public:
    template <typename... Ts>
    graph_named &depends(Ts &&...pred) && {
      add_preds(std::forward<Ts>(pred)...);

      check_node_deps();
      self.add_edge_impl(node_name, pred_list);
      self.store.emplace(node_name, std::move(node));

      return self;
    }
  };

  class aux_delegate {
    friend class graph_named;

    graph_named &self;

    key_type node_name;
    shared_node_ptr node;
    port_set port_list;

    aux_delegate(graph_named &self, key_type name, shared_node_ptr node)
        : self(self), node_name(std::move(name)), node(std::move(node)), port_list() {}

    template <detail::string_like T0, typename... Ts>
    void add_ports(T0 &&port_name, Ts &&...args) {
      if (auto it = self.root_pmap.find(port_name); it != self.root_pmap.end()) {
        port_list.emplace_back(it->second);
      } else {
        throw std::invalid_argument("Invalid root port alias.");
      }
      add_ports(std::forward<Ts>(args)...);
    }

    template <range_of<str_view> R, typename... Ts>
    void add_ports(R &&port_names, Ts &&...args) {
      for (auto const &port_name : port_names) {
        if (auto it = self.root_pmap.find(port_name); it != self.root_pmap.end()) {
          port_list.emplace_back(it->second);
        } else {
          throw std::invalid_argument("Invalid root port alias.");
        }
      }
      add_ports(std::forward<Ts>(args)...);
    }

    template <std::integral T0, typename... Ts>
    void add_ports(T0 &&port, Ts &&...args) {
      port_list.emplace_back(port);
      add_ports(std::forward<Ts>(args)...);
    }

    template <range_idx R, typename... Ts>
    void add_ports(R &&ports, Ts &&...args) {
      for (auto const &port : ports) {
        port_list.emplace_back(port);
      }
      add_ports(std::forward<Ts>(args)...);
    }

    void add_ports() {} // base case

  public:
    template <typename... Ts>
    graph_named &depends(Ts &&...port_or_alias) && {
      add_ports(std::forward<Ts>(port_or_alias)...);

      self.add_aux_impl(node_name, port_list);
      self.store.emplace(node_name, std::move(node));

      return self;
    }
  };

  template <bool SUPP>
  class root_delegate {
    friend class graph_named;

    graph_named &self;

    key_type node_name;
    shared_node_ptr node;
    std::vector<key_type> port_name_list;

    root_delegate(graph_named &self, key_type name, shared_node_ptr node)
        : self(self), node_name(std::move(name)), node(std::move(node)), port_name_list() {}

    template <detail::string_like T0, typename... Ts>
    void add_names(T0 &&port, Ts &&...args) {
      port_name_list.emplace_back(std::forward<T0>(port));
      add_names(std::forward<Ts>(args)...);
    }

    template <range_of<str_view> R, typename... Ts>
    void add_names(R &&port_names, Ts &&...args) {
      for (auto const &port_name : port_names) {
        port_name_list.emplace_back(port_name);
      }
      add_names(std::forward<Ts>(args)...);
    }

    void add_names() {}

  public:
    template <typename... Ts>
    graph_named &ports(Ts &&...port_aliases) && {
      add_names(std::forward<Ts>(port_aliases)...);

      key_set name_test{};
      for (auto const &name : port_name_list) {
        self.check_name(name);
        name_test.emplace(name);
      }
      if (name_test.size() != port_name_list.size()) {
        throw std::invalid_argument("Duplicate port alias.");
      }
      if constexpr (SUPP) {
        self.add_supp_impl(node_name, port_name_list);
      } else {
        self.add_root_impl(node_name, port_name_list);
      }
      self.store.emplace(node_name, std::move(node));

      return self;
    }
  };

  // Add

  template <typename Node, typename... Ts>
  auto add(key_type const &name, Ts &&...args) {
    check_name(name);
    return add_delegate(*this, name, std::make_shared<Node>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Node, typename... Ts>
  auto add(key_type const &name, Ts &&...args)
    requires(!std::is_void_v<DefaultDT>) // Template shorthand for ops with default data type
  {
    return add<Node<DefaultDT>>(name, std::forward<Ts>(args)...);
  }

  template <typename Aux, typename... Ts>
  auto aux(key_type const &name, Ts &&...args) {
    if (!aux_name.empty()) {
      throw std::invalid_argument("Auxiliary node already exists in graph.");
    }
    check_name(name);
    return aux_delegate(*this, name, std::make_shared<Aux>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Aux, typename... Ts>
  auto aux(key_type const &name, Ts &&...args)
    requires(!std::is_void_v<DefaultDT>) // Template shorthand for ops with default data type
  {
    return aux<Aux<DefaultDT>>(name, std::forward<Ts>(args)...);
  }

  shared_node_ptr aux() const noexcept { return store.contains(aux_name) ? store.at(aux_name) : nullptr; }

  port_set const &aux_args() const noexcept { return aux_argmap; }

  template <typename Root, typename... Ts>
  auto root(key_type const &name, Ts &&...args) {
    if (!root_name.empty()) {
      throw std::invalid_argument("Root node already exists in graph.");
    }
    check_name(name);
    return root_delegate<false>(*this, name, std::make_shared<Root>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Root, typename... Ts>
  auto root(key_type const &name, Ts &&...args)
    requires(!std::is_void_v<DefaultDT>) // Template shorthand for ops with default data type
  {
    return root<Root<DefaultDT>>(name, std::forward<Ts>(args)...);
  }

  shared_node_ptr root() const { return store.contains(root_name) ? store.at(root_name) : nullptr; }

  template <typename Supp, typename... Ts>
  auto supp_root(key_type const &name, Ts &&...args) {
    if (!supp_name.empty()) {
      throw std::invalid_argument("Supplementary root node already exists in graph.");
    }
    check_name(name);
    return root_delegate<true>(*this, name, std::make_shared<Supp>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Supp, typename... Ts>
  auto supp_root(key_type const &name, Ts &&...args)
    requires(!std::is_void_v<DefaultDT>) // Template shorthand for ops with default data type
  {
    return supp_root<Supp<DefaultDT>>(name, std::forward<Ts>(args)...);
  }

  shared_node_ptr supp_root() const { return store.contains(supp_name) ? store.at(supp_name) : nullptr; }

  template <typename... Ts>
  graph_named &supp_link(key_type const &node, Ts &&...supp_ports_or_alias) {
    std::vector<u32> ports{};
    supp_link_impl(ports, node, std::forward<Ts>(supp_ports_or_alias)...);
    return *this;
  }

  supp_map const &supp_link() const noexcept { return supp_links; }

  port_set const &supp_link_of(key_type const &node) const noexcept {
    static port_set const empty_set{};
    return supp_links.contains(node) ? supp_links.at(node) : empty_set;
  }

  // Output

  template <typename... Ts>
  graph_named &add_output(Ts &&...outputs) {
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
    store.clear();

    aux_name.clear();
    aux_argmap.clear();

    root_name.clear();
    root_pmap.clear();

    supp_name.clear();
    supp_pmap.clear();
    supp_links.clear();
  }

  bool contains(key_type const &name) const noexcept { return predecessor.contains(name); }

  key_set const &pred_of(key_type const &name) const {
    static key_set const empty_set{};
    auto it = predecessor.find(name);
    return (it != predecessor.end()) ? it->second : empty_set;
  }

  node_map const &pred() const noexcept { return predecessor; }

  args_set const &args_of(key_type const &name) const {
    static args_set const empty_set{};
    auto it = argmap.find(name);
    return (it != argmap.end()) ? it->second : empty_set;
  }

  args_map const &args() const noexcept { return argmap; }

  key_set const &succ_of(key_type const &name) const {
    static key_set const empty_set{};
    auto it = successor.find(name);
    return (it != successor.end()) ? it->second : empty_set;
  }

  node_map const &succ() const noexcept { return successor; }

  shared_node_ptr node(key_type const &name) const {
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

  auto roots() const {
    std::vector<key_type> roots;
    for (auto const &[name, preds] : predecessor) {
      if (preds.empty()) {
        roots.push_back(name);
      }
    }
    return roots;
  }

  auto leaves() const {
    std::vector<key_type> leaves;
    for (auto const &[name, succs] : successor) {
      if (succs.empty()) {
        leaves.push_back(name);
      }
    }
    return leaves;
  }

  bool validate() const noexcept {
    for (auto const &[name, _] : predecessor) {
      if (!store.contains(name)) {
        return false; // Inconsistent: node missing in store
      }
    }
    for (auto const &o : out) {
      if (!store.contains(o.name)) {
        return false; // Inconsistent: output node missing in store
      }
    }
    if (!aux_name.empty() && !store.contains(aux_name)) {
      return false; // Inconsistent: aux node missing in store
    }
    if (!root_name.empty() && !store.contains(root_name)) {
      return false; // Inconsistent: root node missing in store
    }
    if (!supp_name.empty()) {
      if (!store.contains(supp_name)) {
        return false; // Inconsistent: supp node missing in store
      }
      for (auto const &[name, _] : supp_links) {
        if (!store.contains(name)) {
          return false; // Inconsistent: supp link node missing in store
        }
      }
    }
    return true;
  }

private:
  void ensure_adjacency_list(key_type const &name) {
    if (!predecessor.contains(name)) {
      predecessor.emplace(name, key_set{});
    }
    if (!argmap.contains(name)) {
      argmap.emplace(name, args_set{});
    }
    if (!successor.contains(name)) {
      successor.emplace(name, key_set{});
    }
  }

  void check_name(key_type const &name) {
    if (name.empty()) {
      throw std::invalid_argument("Empty node name.");
    }
    if (store.contains(name)) {
      throw std::invalid_argument("Node already exists.");
    }
    if (root_pmap.contains(name) || supp_pmap.contains(name)) {
      throw std::invalid_argument("Node name conflicts with existing root port aliases.");
    }
  }

  template <detail::string_like T0>
  edge_type parse_edge(T0 &&desc) const {
    auto s = key_type(std::forward<T0>(desc));
    // check if is a named root port
    if (root_pmap.contains(s)) {
      auto port = root_pmap.at(s);
      return edge_type(root_name, port);
    } else {
      return edge_type(s);
    }
  }

  // supp link

  template <detail::string_like T0, typename... Ts>
  void supp_link_impl(std::vector<u32> &ports, key_type const &node, T0 &&port_name, Ts &&...args) {
    if (auto it = supp_pmap.find(port_name); it != supp_pmap.end()) {
      ports.emplace_back(it->second);
    } else {
      throw std::invalid_argument("Invalid supplementary root port alias.");
    }
    supp_link_impl(ports, node, std::forward<Ts>(args)...);
  }

  template <range_of<str_view> R, typename... Ts>
  void supp_link_impl(std::vector<u32> &ports, key_type const &node, R &&port_names_range, Ts &&...args) {
    for (auto const &port_name : port_names_range) {
      if (auto it = supp_pmap.find(port_name); it != supp_pmap.end()) {
        ports.emplace_back(it->second);
      } else {
        throw std::invalid_argument("Invalid supplementary root port alias.");
      }
    }
    supp_link_impl(ports, node, std::forward<Ts>(args)...);
  }

  template <std::integral T0, typename... Ts>
  void supp_link_impl(std::vector<u32> &ports, key_type const &node, T0 &&port, Ts &&...args) {
    ports.emplace_back(port);
    supp_link_impl(ports, node, std::forward<Ts>(args)...);
  }

  template <range_idx R, typename... Ts>
  void supp_link_impl(std::vector<u32> &ports, key_type const &node, R &&port_range, Ts &&...args) {
    for (auto const &port : port_range) {
      ports.emplace_back(port);
    }
    supp_link_impl(ports, node, std::forward<Ts>(args)...);
  }

  void supp_link_impl(std::vector<u32> &ports, key_type const &node) {
    supp_links.insert_or_assign(node, std::move(ports));
  }

  // output

  template <detail::string_like T0, typename... Ts>
  void add_output_impl(T0 &&edge_desc, Ts &&...args) {
    out.emplace_back(parse_edge(std::forward<T0>(edge_desc)));
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <range_of<str_view> R, typename... Ts>
  void add_output_impl(R &&edges, Ts &&...args) {
    for (auto const &edge_desc : edges) {
      out.emplace_back(parse_edge(edge_desc));
    }
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <typename... Ts>
  void add_output_impl(edge_type edge, Ts &&...args) {
    out.emplace_back(edge);
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <range_of<edge_type> R, typename... Ts>
  void add_output_impl(R &&edges, Ts &&...args) {
    for (auto const &edge : edges) {
      out.emplace_back(edge);
    }
    add_output_impl(std::forward<Ts>(args)...);
  }

  void add_output_impl() {} // base case

  // edge

  void add_edge_impl(key_type const &name, args_set const &edge_list) {
    ensure_adjacency_list(name);
    for (auto const &edge : edge_list) {
      ensure_adjacency_list(edge.name);
      predecessor[name].emplace(edge.name);
      argmap[name].emplace_back(edge);
      successor[edge.name].emplace(name);
    }
  }

  void add_aux_impl(key_type const &name, port_set const &port_list) {
    aux_name = name;
    aux_argmap = port_list;
  }

  void add_root_impl(key_type const &name, std::vector<key_type> const &port_names) {
    ensure_adjacency_list(name);
    root_name = name;
    root_pmap.clear();
    for (u32 port = 0; port < port_names.size(); ++port)
      root_pmap.emplace(port_names[port], port);
  }

  void add_supp_impl(key_type const &name, std::vector<key_type> const &port_names) {
    supp_name = name;
    supp_pmap.clear();
    for (u32 port = 0; port < port_names.size(); ++port)
      supp_pmap.emplace(port_names[port], port);
  }

protected:
  using NodeStore = std::unordered_map<key_type, shared_node_ptr, key_hash, Equal>;

  node_map predecessor; ///< Adjacency list: node -> [pred] i.e. set of nodes that it depends on
  args_map argmap;      ///< node -> [pred:port] i.e. args for calling this op node, order preserved
  node_map successor;   ///< Reverse adjacency list: node -> [succ] i.e. set of nodes that depend on it
  args_set out;         ///< Output [node:port]
  NodeStore store;      ///< Store for actual node instances

  key_type aux_name;   ///< Name of the auxiliary node
  port_set aux_argmap; ///< Auxiliary args

  key_type root_name; ///< Name of the root node
  key_type supp_name; ///< Name of the support node

  using PortMap = std::unordered_map<key_type, u32, key_hash, Equal>;
  PortMap root_pmap; ///< name -> port mapping
  PortMap supp_pmap; ///< name -> port mapping

  supp_map supp_links; ///< node -> [supp_port] essentially successor list for supp root
};
} // namespace opflow
