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

template <typename T, typename AUX = void>
class graph_named {
  using Equal = std::equal_to<>;
  using str_view = std::string_view;

public:
  using key_type = std::string;
  using key_hash = detail::str_hash;

  using node_type = T;
  using shared_node_ptr = std::shared_ptr<T>;

  using aux_type = AUX;
  using shared_aux_ptr = std::shared_ptr<aux_type>;

  using edge_type = detail::graph_named_edge;
  using NodeSet = std::unordered_set<key_type, key_hash, Equal>;
  using NodeArgsSet = std::vector<edge_type>;
  using NodeMap = std::unordered_map<key_type, NodeSet, key_hash, Equal>;
  using NodeArgsMap = std::unordered_map<key_type, NodeArgsSet, key_hash, Equal>;

  using PortSet = std::vector<u32>;
  using SuppLinksMap = std::unordered_map<key_type, PortSet, key_hash, Equal>;

  class add_delegate {
    friend class graph_named;

    graph_named &self;
    key_type node_name;
    shared_node_ptr node;
    std::vector<edge_type> pred_list;

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

  public:
    template <typename... Ts>
    graph_named &depends(Ts &&...pred) && {
      add_preds(std::forward<Ts>(pred)...);

      self.ensure_adjacency_list(node_name);
      for (auto const &edge : pred_list) {
        self.ensure_adjacency_list(edge.name);
        self.add_edge_impl(node_name, edge);
      }
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
    std::vector<key_type> port_list;

    root_delegate(graph_named &self, key_type name, shared_node_ptr node)
        : self(self), node_name(std::move(name)), node(std::move(node)), port_list() {}

    template <detail::string_like T0, typename... Ts>
    void add_ports(T0 &&port, Ts &&...args) {
      port_list.emplace_back(std::forward<T0>(port));
      add_ports(std::forward<Ts>(args)...);
    }

    template <range_of<str_view> R, typename... Ts>
    void add_ports(R &&port_names, Ts &&...args) {
      for (auto const &port_name : port_names) {
        port_list.emplace_back(port_name);
      }
      add_ports(std::forward<Ts>(args)...);
    }

    void add_ports() {}

  public:
    template <typename... Ts>
    graph_named &ports(Ts &&...port_names) && {
      add_ports(std::forward<Ts>(port_names)...);

      self.store.emplace(node_name, std::move(node));

      // Populate port map and adjacency list
      if constexpr (SUPP) {
        self.pmap_supp.clear();
        for (u32 port = 0; port < port_list.size(); ++port) {
          self.pmap_supp.emplace(port_list[port], port);
        }
        self.supp_name = node_name;
      } else {
        self.ensure_adjacency_list(node_name);
        self.pmap_root.clear();
        for (u32 port = 0; port < port_list.size(); ++port) {
          self.pmap_root.emplace(port_list[port], port);
        }
        self.root_name = node_name;
      }

      return self;
    }
  };

  class aux_delegate {
    friend class graph_named;

    graph_named &self;
    shared_aux_ptr aux;
    std::vector<edge_type> pred_list;

    aux_delegate(graph_named &self, shared_aux_ptr aux) : self(self), aux(std::move(aux)), pred_list() {}

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

  public:
    template <typename... Ts>
    graph_named &depends(Ts &&...pred) && {
      add_preds(std::forward<Ts>(pred)...);
      self.auxiliary = std::move(aux);
      for (auto const &edge : pred_list) {
        self.ensure_adjacency_list(edge.name);
        self.auxiliary_args.emplace_back(edge);
      }
      return self;
    }
  };

  // Add

  template <typename Node, typename... Ts>
  auto add(key_type const &name, Ts &&...args) {
    return add_delegate(*this, name, std::make_shared<Node>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Node, typename... Ts>
  auto add(key_type const &name, Ts &&...args)
    requires(detail::has_data_type<T>) // CONVENIENT FN FOR OP DO NOT TEST
  {
    return add<Node<typename T::data_type>>(name, std::forward<Ts>(args)...);
  }

  template <typename Root, typename... Ts>
  auto root(key_type const &name, Ts &&...args) {
    return root_delegate<false>(*this, name, std::make_shared<Root>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Root, typename... Ts>
  auto root(key_type const &name, Ts &&...args)
    requires(detail::has_data_type<T>) // CONVENIENT FN FOR OP DO NOT TEST
  {
    return root<typename T::data_type>(name, std::forward<Ts>(args)...);
  }

  shared_node_ptr root() const { return store.contains(root_name) ? store.at(root_name) : nullptr; }

  template <typename Supp, typename... Ts>
  auto supp_root(key_type const &name, Ts &&...args) {
    return root_delegate<true>(*this, name, std::make_shared<Supp>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Supp, typename... Ts>
  auto supp_root(key_type const &name, Ts &&...args)
    requires(detail::has_data_type<T>) // CONVENIENT FN FOR OP DO NOT TEST
  {
    return supp_root<Supp<typename T::data_type>>(name, std::forward<Ts>(args)...);
  }

  shared_node_ptr supp_root() const { return store.contains(supp_name) ? store.at(supp_name) : nullptr; }

  template <typename Aux, typename... Ts>
  auto aux(Ts &&...args)
    requires(!std::is_void_v<AUX>)
  {
    return aux_delegate(*this, std::make_shared<Aux>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Aux, typename... Ts>
  auto aux(Ts &&...args)
    requires(!std::is_void_v<AUX> && detail::has_data_type<T>) // CONVENIENT FN FOR OP DO NOT TEST
  {
    return aux<Aux<typename T::data_type>>(std::forward<Ts>(args)...);
  }

  shared_aux_ptr aux() const noexcept { return auxiliary; }

  NodeArgsSet const &aux_args() const noexcept { return auxiliary_args; }

  template <typename... Ts>
  graph_named &supp_link(key_type const &node, Ts &&...preds) {
    std::vector<u32> ports{};
    supp_link_impl(ports, node, std::forward<Ts>(preds)...);
    return *this;
  }

  SuppLinksMap const &supp_link() const noexcept { return supp_links; }

  PortSet const &supp_link_of(key_type const &node) const noexcept {
    static PortSet const empty_set{};
    return supp_links.contains(node) ? supp_links.at(node) : empty_set;
  }

  // Output

  template <typename... Ts>
  graph_named &add_output(Ts &&...outputs) {
    add_output_impl(std::forward<Ts>(outputs)...);
    return *this;
  }

  template <typename... Ts>
  graph_named &set_output(Ts &&...outputs) {
    out.clear();
    add_output_impl(std::forward<Ts>(outputs)...);
    return *this;
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

  // Replacement

  bool rename(key_type const &old_name, key_type const &new_name) {
    if (!predecessor.contains(old_name)) {
      return false; // Old node doesn't exist
    }
    if (predecessor.contains(new_name)) {
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
    for (auto &o : out) {
      if (o.name == old_name) {
        o.name = new_name;
      }
    }

    // Update auxiliary arg references
    for (auto &edge : auxiliary_args) {
      if (edge.name == old_name) {
        edge.name = new_name;
      }
    }

    // Update root_name and supp_name if needed
    if (root_name == old_name) {
      root_name = new_name;
    }
    if (supp_name == old_name) {
      supp_name = new_name;
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
    if (!predecessor.contains(old_node)) {
      return false; // Old node doesn't exist
    }
    if (predecessor.contains(new_node)) {
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
    if (!predecessor.contains(node)) {
      return false; // Node doesn't exist
    }
    if (!predecessor.contains(new_pred.name)) {
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
      if (arg == old_pred) {
        arg = new_pred;
      }
    }

    // Check if old_pred is still needed
    cleanup_adj(node, old_pred.name);

    return true;
  }

  // Utilities

  // this does not include aux and supp

  size_t size() const noexcept { return predecessor.size(); }

  bool empty() const noexcept { return predecessor.empty(); }

  void clear() noexcept {
    predecessor.clear();
    argmap.clear();
    successor.clear();
    out.clear();
    store.clear();
  }

  // this does not check aux and supp

  bool contains(key_type const &name) const noexcept { return predecessor.contains(name); }

  NodeSet const &pred_of(key_type const &name) const {
    static NodeSet const empty_set{};
    auto it = predecessor.find(name);
    return (it != predecessor.end()) ? it->second : empty_set;
  }

  NodeMap const &pred() const noexcept { return predecessor; }

  NodeArgsSet const &args_of(key_type const &name) const {
    static NodeArgsSet const empty_set{};
    auto it = argmap.find(name);
    return (it != argmap.end()) ? it->second : empty_set;
  }

  NodeArgsMap const &args() const noexcept { return argmap; }

  NodeSet const &succ_of(key_type const &name) const {
    static NodeSet const empty_set{};
    auto it = successor.find(name);
    return (it != successor.end()) ? it->second : empty_set;
  }

  NodeMap const &succ() const noexcept { return successor; }

  NodeArgsSet const &output() const noexcept { return out; }

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
    if constexpr (!std::is_void_v<AUX>) {
      for (auto const &edge : auxiliary_args) {
        if (!store.contains(edge.name)) {
          return false; // Inconsistent: aux arg node missing in store
        }
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
      auto other_node = other.node(new_name);

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
    if (!predecessor.contains(name)) {
      predecessor.emplace(name, NodeSet{});
    }
    if (!argmap.contains(name)) {
      argmap.emplace(name, NodeArgsSet{});
    }
    if (!successor.contains(name)) {
      successor.emplace(name, NodeSet{});
    }
  }

  template <detail::string_like T0>
  edge_type parse_edge(T0 &&desc) const {
    auto s = key_type(std::forward<T0>(desc));
    // check if is a named root port
    if (pmap_root.contains(s)) {
      auto port = pmap_root.at(s);
      return edge_type(root_name, port);
    } else {
      return edge_type(s);
    }
  }

  template <detail::string_like T0>
  u32 parse_supp(T0 &&desc) const {
    auto s = key_type(std::forward<T0>(desc));
    if (pmap_supp.contains(s)) {
      return pmap_supp.at(s);
    } else {
      auto e = edge_type(s);
      return e.port;
    }
  }

  // supp link

  template <detail::string_like T0, typename... Ts>
  void supp_link_impl(std::vector<u32> &ports, key_type const &node, T0 &&port_name, Ts &&...args) {
    ports.emplace_back(parse_supp(std::forward<T0>(port_name)));
    supp_link_impl(ports, node, std::forward<Ts>(args)...);
  }

  template <range_of<str_view> R, typename... Ts>
  void supp_link_impl(std::vector<u32> &ports, key_type const &node, R &&port_names_range, Ts &&...args) {
    for (auto const &port_name : port_names_range) {
      ports.emplace_back(parse_supp(port_name));
    }
    supp_link_impl(ports, node, std::forward<Ts>(args)...);
  }

  template <typename... Ts>
  void supp_link_impl(std::vector<u32> &ports, key_type const &node, edge_type edge, Ts &&...args) {
    ports.emplace_back(edge.port);
    supp_link_impl(ports, node, std::forward<Ts>(args)...);
  }

  template <range_of<edge_type> R, typename... Ts>
  void supp_link_impl(std::vector<u32> &ports, key_type const &node, R &&edges, Ts &&...args) {
    for (auto const &edge : edges) {
      ports.emplace_back(edge.port);
    }
    supp_link_impl(ports, node, std::forward<Ts>(args)...);
  }

  void supp_link_impl(std::vector<u32> &ports, key_type const &node) { supp_links.emplace(node, std::move(ports)); }

  // output

  template <detail::string_like T0, typename... Ts>
  void add_output_impl(T0 &&edge_desc, Ts &&...args) {
    out.emplace_back(std::forward<T0>(edge_desc));
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <typename... Ts>
  void add_output_impl(edge_type edge, Ts &&...args) {
    out.emplace_back(edge);
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <range_of<str_view> R, typename... Ts>
  void add_output_impl(R &&edges, Ts &&...args) {
    for (auto const &edge_desc : edges) {
      out.emplace_back(edge_desc);
    }
    add_output_impl(std::forward<Ts>(args)...);
  }

  void add_output_impl() {} // base case

  template <range_of<edge_type> R, typename... Ts>
  void add_output_impl(R &&edges, Ts &&...args) {
    for (auto const &edge : edges) {
      out.emplace_back(edge);
    }
    add_output_impl(std::forward<Ts>(args)...);
  }

  // set_aux - edge

  template <typename A, detail::string_like T0, typename... Ts>
  void set_aux_impl(T0 &&edge_desc, Ts &&...args) {
    auto edge = edge_type(std::forward<T0>(edge_desc));
    auxiliary_args.emplace_back(edge);
    set_aux_impl<A>(std::forward<Ts>(args)...);
  }

  template <typename A, typename... Ts>
  void set_aux_impl(edge_type edge, Ts &&...args) {
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

  template <typename A, range_of<str_view> R, typename... Ts>
  void set_aux_impl(R &&edges, Ts &&...args) {
    for (auto const &edge_desc : edges) {
      auto edge = edge_type(edge_desc);
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

  void add_edge_root(key_type const &name, key_type const &pred) {
    auto port = pmap_root.at(pred);
    add_edge_impl(name, edge_type(root_name, port));
  }

  void add_edge_impl(key_type const &name, edge_type const &pred) {
    predecessor[name].emplace(pred.name);
    argmap[name].emplace_back(pred);
    successor[pred.name].emplace(name);
  }

  void cleanup_adj(key_type const &name, key_type const &pred) {
    auto const &args = argmap[name];
    bool has_conn = std::any_of(args.begin(), args.end(), [&](auto const &arg) { return arg.name == pred; });
    if (!has_conn) {
      // If no other connections exist, remove old predecessor from adjacency maps
      predecessor[name].erase(pred);
      successor[pred].erase(name);
    }
  }

protected:
  using NodeStore = std::unordered_map<key_type, shared_node_ptr, key_hash, Equal>;

  NodeMap predecessor;        ///< Adjacency list: node -> [pred] i.e. set of nodes that it depends on
  NodeArgsMap argmap;         ///< node -> [pred:port] i.e. args for calling this op node, order preserved
  NodeMap successor;          ///< Reverse adjacency list: node -> [succ] i.e. set of nodes that depend on it
  NodeArgsSet out;            ///< Output [node:port]
  NodeStore store;            ///< Store for actual node instances
  shared_aux_ptr auxiliary;   ///< Auxiliary data
  NodeArgsSet auxiliary_args; ///< Auxiliary args

  key_type root_name; ///< Name of the root node
  key_type supp_name; ///< Name of the support node

  using PortMap = std::unordered_map<key_type, u32, key_hash, Equal>;
  PortMap pmap_root; ///< name -> port mapping
  PortMap pmap_supp; ///< name -> port mapping

  SuppLinksMap supp_links; ///< node -> [supp_port] essentially successor list for supp root
};
} // namespace opflow
