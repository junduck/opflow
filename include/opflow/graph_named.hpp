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
template <char Delim = '.'>
struct graph_named_edge {
  std::string name;
  uint32_t port;

  graph_named_edge(std::string_view name, uint32_t port) : name(name), port(port) {}

  graph_named_edge(std::string_view desc) {
    auto const dot_pos = desc.find_last_of(Delim);
    if (desc.find_first_of(Delim) != dot_pos) {
      throw std::invalid_argument("Multiple delimiters found in edge descriptor.");
    }
    if (dot_pos == std::string_view::npos) {
      name = desc;
      port = 0;
      return;
    }
    name = desc.substr(0, dot_pos);
    auto [_, ec] = std::from_chars(desc.data() + dot_pos + 1, desc.end(), port);
    if (ec != std::errc{}) {
      throw std::system_error(std::make_error_code(ec));
    }
  }

  operator std::string() const { return name + Delim + std::to_string(port); }
  bool operator==(graph_named_edge const &) const noexcept = default;
};
} // namespace detail

inline auto make_edge(std::string_view name, uint32_t port = 0) { return detail::graph_named_edge(name, port); }

template <typename T, typename DefaultDT = void, char Delim = '.'>
class graph_named {
  using Equal = std::equal_to<>;

public:
  using node_type = T;
  using shared_node_ptr = std::shared_ptr<T>;

  using str_view = std::string_view;
  using key_type = std::string;
  using key_hash = detail::str_hash;
  using edge_type = detail::graph_named_edge<Delim>;

  using key_set = std::unordered_set<key_type, key_hash, Equal>;
  using args_set = std::vector<edge_type>;
  using port_set = std::vector<u32>;

  using node_map = std::unordered_map<key_type, key_set, key_hash, Equal>;  ///< node -> preds/succs
  using args_map = std::unordered_map<key_type, args_set, key_hash, Equal>; ///< node -> args
  using supp_map = std::unordered_map<key_type, port_set, key_hash, Equal>; ///< node -> supp ports

  class add_delegate {
    friend class graph_named;

    graph_named &self;

    key_type name;
    shared_node_ptr node;
    args_set args;

    template <detail::string_like T0>
    add_delegate(graph_named &self, T0 &&name, shared_node_ptr node)
        : self(self), name(std::forward<T0>(name)), node(std::move(node)), args() {}

    template <detail::string_like T0, typename... Ts>
    void add_args(T0 &&edge, Ts &&...t) {
      args.emplace_back(self.parse_edge(std::forward<T0>(edge)));
      add_args(std::forward<Ts>(t)...);
    }

    template <range_of<str_view> R, typename... Ts>
    void add_args(R &&edges, Ts &&...t) {
      for (auto const &edge_desc : edges) {
        args.emplace_back(self.parse_edge(edge_desc));
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

    void add_args() {}

    void check() const {
      for (auto const &arg : args) {
        if (arg.name == self.aux_name) {
          throw std::invalid_argument("Cannot depend on auxiliary node.");
        }
        if (arg.name == self.supp_name) {
          throw std::invalid_argument("Cannot depend on supplementary root node.");
        }
      }
      self.check_name(name);
    }

  public:
    template <typename... Ts>
    graph_named &depends(Ts &&...pred) && {
      add_args(std::forward<Ts>(pred)...);

      check();
      self.add_edge_impl(name, args);
      self.store.emplace(name, std::move(node));

      return self;
    }
  };

  class aux_delegate {
    friend class graph_named;

    graph_named &self;

    key_type name;
    shared_node_ptr node;
    port_set args;

    template <detail::string_like T0>
    aux_delegate(graph_named &self, T0 &&name, shared_node_ptr node)
        : self(self), name(std::forward<T0>(name)), node(std::move(node)), args() {}

    template <detail::string_like T0, typename... Ts>
    void add_args(T0 &&port_name, Ts &&...t) {
      if (auto it = self.root_pmap.find(port_name); it != self.root_pmap.end()) {
        args.emplace_back(it->second);
      } else {
        throw std::invalid_argument("Invalid root port alias.");
      }
      add_args(std::forward<Ts>(t)...);
    }

    template <range_of<str_view> R, typename... Ts>
    void add_args(R &&port_names, Ts &&...t) {
      for (auto const &port_name : port_names) {
        if (auto it = self.root_pmap.find(port_name); it != self.root_pmap.end()) {
          args.emplace_back(it->second);
        } else {
          throw std::invalid_argument("Invalid root port alias.");
        }
      }
      add_args(std::forward<Ts>(t)...);
    }

    template <std::integral T0, typename... Ts>
    void add_args(T0 &&port, Ts &&...t) {
      args.emplace_back(port);
      add_args(std::forward<Ts>(t)...);
    }

    template <range_idx R, typename... Ts>
    void add_args(R &&ports, Ts &&...t) {
      for (auto const &port : ports) {
        args.emplace_back(port);
      }
      add_args(std::forward<Ts>(t)...);
    }

    void add_args() {}

    void check() const { self.check_name(name); }

  public:
    template <typename... Ts>
    graph_named &depends(Ts &&...port_or_alias) && {
      add_args(std::forward<Ts>(port_or_alias)...);

      check();
      self.add_aux_impl(name, std::move(args));
      self.store.emplace(name, std::move(node));

      return self;
    }
  };

  template <bool SUPP>
  class root_delegate {
    friend class graph_named;

    graph_named &self;

    key_type name;
    shared_node_ptr node;
    std::vector<key_type> aliases;

    template <detail::string_like T0>
    root_delegate(graph_named &self, T0 &&name, shared_node_ptr node)
        : self(self), name(std::forward<T0>(name)), node(std::move(node)), aliases() {}

    template <detail::string_like T0, typename... Ts>
    void add_aliases(T0 &&port_name, Ts &&...args) {
      aliases.emplace_back(std::forward<T0>(port_name));
      add_aliases(std::forward<Ts>(args)...);
    }

    template <range_of<str_view> R, typename... Ts>
    void add_aliases(R &&port_names, Ts &&...args) {
      for (auto const &port_name : port_names) {
        aliases.emplace_back(port_name);
      }
      add_aliases(std::forward<Ts>(args)...);
    }

    void add_aliases() {}

    void check() const {
      key_set name_test{};
      for (auto const &a : aliases) {
        // For port alias, we need to check declared names so port alias won't conflict with nodes not constructed yet
        self.check_name_declared(a);
        name_test.emplace(a);
      }
      if (name_test.size() != aliases.size()) {
        throw std::invalid_argument("Duplicate port alias.");
      }
    }

  public:
    template <typename... Ts>
    graph_named &alias(Ts &&...port_aliases) && {
      add_aliases(std::forward<Ts>(port_aliases)...);

      check();
      if constexpr (SUPP) {
        self.add_supp_impl(name, aliases);
      } else {
        self.add_root_impl(name, aliases);
      }
      self.store.emplace(name, std::move(node));

      return self;
    }
  };

  template <typename Node, typename... Ts>
  auto add(str_view name, Ts &&...args) {
    return add_delegate(*this, name, std::make_shared<Node>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Node, typename... Ts>
  auto add(str_view name, Ts &&...args)
    requires(!std::is_void_v<DefaultDT>) // Template shorthand for ops with default
  {
    return add<Node<DefaultDT>>(name, std::forward<Ts>(args)...);
  }

  shared_node_ptr node(str_view name) const {
    auto it = store.find(name);
    return it != store.end() ? it->second : nullptr;
  }

  template <typename Aux, typename... Ts>
  auto aux(str_view name, Ts &&...args) {
    if (!aux_name.empty()) {
      throw std::invalid_argument("Auxiliary node already exists in graph.");
    }
    return aux_delegate(*this, name, std::make_shared<Aux>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Aux, typename... Ts>
  auto aux(str_view name, Ts &&...args)
    requires(!std::is_void_v<DefaultDT>) // Template shorthand for ops with default data type
  {
    return aux<Aux<DefaultDT>>(name, std::forward<Ts>(args)...);
  }

  shared_node_ptr aux() const noexcept { return store.contains(aux_name) ? store.at(aux_name) : nullptr; }

  port_set const &aux_args() const noexcept { return aux_args_; }

  template <typename Root, typename... Ts>
  auto root(str_view name, Ts &&...args) {
    if (!root_name.empty()) {
      throw std::invalid_argument("Root node already exists in graph.");
    }
    return root_delegate<false>(*this, name, std::make_shared<Root>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Root, typename... Ts>
  auto root(str_view name, Ts &&...args)
    requires(!std::is_void_v<DefaultDT>) // Template shorthand for ops with default data type
  {
    return root<Root<DefaultDT>>(name, std::forward<Ts>(args)...);
  }

  shared_node_ptr root() const { return store.contains(root_name) ? store.at(root_name) : nullptr; }

  template <typename Supp, typename... Ts>
  auto supp_root(str_view name, Ts &&...args) {
    if (!supp_name.empty()) {
      throw std::invalid_argument("Supplementary root node already exists in graph.");
    }
    return root_delegate<true>(*this, name, std::make_shared<Supp>(std::forward<Ts>(args)...));
  }

  template <template <typename> typename Supp, typename... Ts>
  auto supp_root(str_view name, Ts &&...args)
    requires(!std::is_void_v<DefaultDT>) // Template shorthand for ops with default data type
  {
    return supp_root<Supp<DefaultDT>>(name, std::forward<Ts>(args)...);
  }

  shared_node_ptr supp_root() const { return store.contains(supp_name) ? store.at(supp_name) : nullptr; }

  template <typename... Ts>
  graph_named &supp_link(str_view node, Ts &&...supp_ports_or_alias) {
    port_set ports;
    supp_link_impl(ports, node, std::forward<Ts>(supp_ports_or_alias)...);
    return *this;
  }

  supp_map const &supp_link() const noexcept { return supp_link_; }

  port_set const &supp_link_of(str_view node) const noexcept {
    static port_set const empty_set{};
    auto it = supp_link_.find(node);
    return it != supp_link_.end() ? it->second : empty_set;
  }

  template <typename... Ts>
  graph_named &add_output(Ts &&...outputs) {
    add_output_impl(std::forward<Ts>(outputs)...);
    return *this;
  }

  template <typename... Ts>
  graph_named &set_output(Ts &&...outputs) {
    output_.clear();
    add_output_impl(std::forward<Ts>(outputs)...);
    return *this;
  }

  args_set const &output() const noexcept { return output_; }

  // Utilities

  size_t size() const noexcept { return store.size(); }
  bool empty() const noexcept { return store.empty(); }
  bool contains(str_view name) const noexcept { return store.contains(name); }
  bool declared(str_view name) const noexcept { return names.contains(name); }

  void clear() {
    pred_.clear();
    args_.clear();
    succ_.clear();
    output_.clear();
    store.clear();
    names.clear();
    root_name.clear();
    aux_name.clear();
    aux_args_.clear();
    supp_name.clear();
    root_pmap.clear();
    supp_pmap.clear();
    supp_link_.clear();
  }

  key_set const &pred_of(str_view name) const noexcept {
    static key_set const empty_set{};
    auto it = pred_.find(name);
    return it != pred_.end() ? it->second : empty_set;
  }

  key_set const &succ_of(str_view name) const noexcept {
    static key_set const empty_set{};
    auto it = succ_.find(name);
    return it != succ_.end() ? it->second : empty_set;
  }

  args_set const &args_of(str_view name) const noexcept {
    static args_set const empty_set{};
    auto it = args_.find(name);
    return it != args_.end() ? it->second : empty_set;
  }

  node_map const &pred() const noexcept { return pred_; }
  node_map const &succ() const noexcept { return succ_; }
  args_map const &args() const noexcept { return args_; }

  bool is_root(str_view name) const {
    auto it = pred_.find(name);
    return it != pred_.end() && it->second.empty();
  }

  bool is_leaf(str_view name) const {
    auto it = succ_.find(name);
    return it != succ_.end() && it->second.empty();
  }

  auto roots() const {
    std::vector<key_type> r;
    for (auto const &[name, preds] : pred_) {
      if (preds.empty()) {
        r.push_back(name);
      }
    }
    return r;
  }

  auto leaves() const {
    std::vector<key_type> l;
    for (auto const &[name, succs] : succ_) {
      if (succs.empty()) {
        l.push_back(name);
      }
    }
    return l;
  }

  bool validate() const noexcept {
    for (auto const &[name, _] : pred_) {
      if (!store.contains(name)) {
        return false;
      }
    }
    for (auto const &out : output_) {
      if (!store.contains(out.name)) {
        return false;
      }
    }
    if (!supp_name.empty()) {
      for (auto const &[name, _] : supp_link_) {
        if (!store.contains(name)) {
          return false;
        }
      }
    }
    return true;
  }

private:
  void check_name(str_view name) const {
    if (name.empty()) {
      throw std::invalid_argument("Empty name.");
    }
    // add() implicitly declares parent nodes if not exist
    // we only check for constructed nodes here
    if (store.contains(name) || root_pmap.contains(name) || supp_pmap.contains(name)) {
      throw std::invalid_argument("Name already exists.");
    }
  }

  void check_name_declared(str_view name) const {
    if (name.empty()) {
      throw std::invalid_argument("Empty name.");
    }
    // add() implicitly declares parent nodes if not exist
    // we check for all declared names here
    if (names.contains(name)) {
      throw std::invalid_argument("Name already exists.");
    }
  }

  void ensure_adjacency_list(str_view name) {
    if (!pred_.contains(name)) {
      pred_.emplace(name, key_set{});
    }
    if (!args_.contains(name)) {
      args_.emplace(name, args_set{});
    }
    if (!succ_.contains(name)) {
      succ_.emplace(name, key_set{});
    }
    names.emplace(name);
  }

  template <detail::string_like T0>
  edge_type parse_edge(T0 &&desc) const {
    str_view s{std::forward<T0>(desc)};
    if (auto const it = root_pmap.find(s); it != root_pmap.end()) {
      return edge_type(root_name, it->second);
    } else {
      return edge_type(s);
    }
  }

  // supp link

  template <detail::string_like T0, typename... Ts>
  void supp_link_impl(port_set &ports, str_view node, T0 &&port_name, Ts &&...args) {
    if (auto it = supp_pmap.find(port_name); it != supp_pmap.end()) {
      ports.emplace_back(it->second);
    } else {
      throw std::invalid_argument("Invalid supplementary root port alias.");
    }
    supp_link_impl(ports, node, std::forward<Ts>(args)...);
  }

  template <range_of<str_view> R, typename... Ts>
  void supp_link_impl(port_set &ports, str_view node, R &&port_names_range, Ts &&...args) {
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
  void supp_link_impl(port_set &ports, str_view node, T0 &&port, Ts &&...args) {
    ports.emplace_back(port);
    supp_link_impl(ports, node, std::forward<Ts>(args)...);
  }

  template <range_idx R, typename... Ts>
  void supp_link_impl(port_set &ports, str_view node, R &&port_range, Ts &&...args) {
    for (auto const &port : port_range) {
      ports.emplace_back(port);
    }
    supp_link_impl(ports, node, std::forward<Ts>(args)...);
  }

  void supp_link_impl(port_set &ports, str_view node) { supp_link_.insert_or_assign(std::string(node), ports); }

  // output

  template <detail::string_like T0, typename... Ts>
  void add_output_impl(T0 &&edge_desc, Ts &&...args) {
    output_.emplace_back(parse_edge(std::forward<T0>(edge_desc)));
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <range_of<str_view> R, typename... Ts>
  void add_output_impl(R &&edges, Ts &&...args) {
    for (auto const &edge_desc : edges) {
      output_.emplace_back(parse_edge(edge_desc));
    }
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <typename... Ts>
  void add_output_impl(edge_type edge, Ts &&...args) {
    output_.emplace_back(edge);
    add_output_impl(std::forward<Ts>(args)...);
  }

  template <range_of<edge_type> R, typename... Ts>
  void add_output_impl(R &&edges, Ts &&...args) {
    for (auto const &edge : edges) {
      output_.emplace_back(edge);
    }
    add_output_impl(std::forward<Ts>(args)...);
  }

  void add_output_impl() {}

  // add

  void add_edge_impl(str_view name, args_set const &edge_list) {
    ensure_adjacency_list(name);
    for (auto const &edge : edge_list) {
      ensure_adjacency_list(edge.name);
      {
        auto it = pred_.find(name);
        it->second.emplace(edge.name);
      }
      {
        auto it = args_.find(name);
        it->second.emplace_back(edge);
      }
      succ_[edge.name].emplace(name);
    }
  }

  void add_aux_impl(str_view name, port_set port_list) {
    aux_name = name;
    aux_args_ = std::move(port_list);
    names.emplace(name);
  }

#define SET_NAME_AND_PORT_ALIASES(WHAT, ARG_NAME, ARG_PORT_ALIASES)                                                    \
  do {                                                                                                                 \
    WHAT##_name = ARG_NAME;                                                                                            \
    WHAT##_pmap.clear();                                                                                               \
    for (u32 port = 0; port < ARG_PORT_ALIASES.size(); ++port) {                                                       \
      WHAT##_pmap.emplace(ARG_PORT_ALIASES[port], port);                                                               \
      names.emplace(ARG_PORT_ALIASES[port]);                                                                           \
    }                                                                                                                  \
    names.emplace(ARG_NAME);                                                                                           \
  } while (0)

  void add_root_impl(str_view name, std::vector<key_type> const &port_aliases) {
    ensure_adjacency_list(name);
    SET_NAME_AND_PORT_ALIASES(root, name, port_aliases);
  }

  void add_supp_impl(str_view name, std::vector<key_type> const &port_aliases) {
    // Supp root node is not part of DAG
    SET_NAME_AND_PORT_ALIASES(supp, name, port_aliases);
  }

#undef SET_NAME_AND_PORT_ALIASES

protected:
  using NodeStore = std::unordered_map<key_type, shared_node_ptr, key_hash, Equal>;
  using NameStore = std::unordered_set<key_type, key_hash, Equal>;
  using PortMap = std::unordered_map<key_type, u32, key_hash, Equal>;

  node_map pred_;   ///< Adjacency list: node -> [pred] i.e. set of nodes that it depends on
  args_map args_;   ///< node -> [pred:port] i.e. args for calling this op node, order preserved
  node_map succ_;   ///< Reverse adjacency list: node -> [succ] i.e. set of nodes that depend on it
  args_set output_; ///< Output [node:port]
  NodeStore store;  ///< Store for actual node instances
  NameStore names;  ///< Set of all declared names

  key_type root_name; ///< Name of the root node
  key_type aux_name;  ///< Name of the auxiliary node
  key_type supp_name; ///< Name of the supplementary root node

  PortMap root_pmap; ///< root port name -> port index
  PortMap supp_pmap; ///< supp port name -> port index

  port_set aux_args_;  ///< Auxiliary args [root:port]
  supp_map supp_link_; ///< node -> [supp:port]
};
} // namespace opflow
