#pragma once

#include <queue>

#include "../common.hpp"
#include "fixed_buffer_resource.hpp"
#include "flat_multivect.hpp"
#include "utils.hpp"

namespace opflow::detail {

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

    %% Aux is used as window node
    %% Supp is used as param node
    cloneable --> node_type
    cloneable --> aux_type
    node_type <-. "Non-covariant" .-> aux_type
*/

template <typename T, typename U, typename Alloc = std::allocator<T>>
class graph_store {
  using byte_alloc = rebind_alloc<Alloc, std::byte>;
  std::vector<std::byte, byte_alloc> arena_storage;
  fixed_buffer_resource arena;

public:
  using node_type = T;
  using aux_type = U;

  using unique_node_ptr = std::unique_ptr<node_type, arena_deleter<node_type>>;
  using shared_node_ptr = std::shared_ptr<node_type>;

  using unique_aux_ptr = std::unique_ptr<aux_type, arena_deleter<aux_type>>;
  using shared_aux_ptr = std::shared_ptr<aux_type>;

  template <typename G>
  graph_store(G const &g, size_t n_group, Alloc alloc)
      : // arena
        arena_storage(alloc), arena(nullptr, 0),
        // data
        n_grp(n_group), n_nodes(g.size()), win_ptrs(), param_ptrs(), node_ptrs(),
        // topo
        record_size(), param_size(), record_offset(alloc), input_offset(alloc), output_offset(alloc), param_node(alloc),
        param_port(alloc) {
    if (n_grp == 0) {
      throw std::invalid_argument("Number of groups must be greater than 0.");
    }

    // Validate topology
    if (!g.validate()) {
      throw std::invalid_argument("Graph validation failed.");
    }

    // Validate node connections
    validate(g);

    shared_aux_ptr aux{};
    if constexpr (!std::is_void_v<aux_type>) {
      if (!g.aux()) {
        throw std::invalid_argument("Auxiliary node not set in graph.");
      }
      aux = std::static_pointer_cast<aux_type>(g.aux());
    }

    shared_node_ptr supp = std::static_pointer_cast<node_type>(g.supp_root());

    using key_type = typename G::key_type;
    using hash_type = typename G::key_hash;
    using lookup_type = std::unordered_map<key_type, u32, hash_type, std::equal_to<>>;
    std::vector<key_type> keys{};         // sorted node keys
    std::vector<shared_node_ptr> nodes{}; // sorted node instances
    lookup_type idx{};                    // node key -> sorted index

    lookup_type in_degree{};
    std::queue<key_type> ready{};

    keys.reserve(n_nodes);
    nodes.reserve(n_nodes);
    idx.reserve(n_nodes);

    in_degree.reserve(n_nodes);
    for (auto const &[node, preds] : g.pred()) {
      auto n_pred = preds.size();
      in_degree.emplace(node, n_pred);
      if (n_pred == 0) {
        ready.push(node);
      }
    }

    while (!ready.empty()) {
      auto current = std::move(ready.front());
      ready.pop();
      keys.push_back(current);

      // update successors
      auto succ_it = g.succ().find(current);
      for (auto const &succ : succ_it->second) {
        if (--in_degree[succ] == 0) {
          ready.push(succ);
        }
      }
    }

    if (keys.size() != n_nodes) {
      throw std::runtime_error("Cyclic graph detected.");
    }

    for (u32 i = 0; i < keys.size(); ++i) {
      idx[keys[i]] = i;
      nodes.push_back(std::static_pointer_cast<node_type>(g.node(keys[i])));
    }

    record_size = 0;
    record_offset.reserve(n_nodes);
    for (size_t i = 0; i < n_nodes; ++i) {
      record_offset.emplace_back(record_size);
      record_size += nodes[i]->num_outputs();
    }

    auto num_edges = aux ? g.aux_args().size() : 0;
    auto const &g_args = g.args();
    for (auto const &[_, args] : g_args) {
      num_edges += args.size();
    }
    input_offset.reserve(n_nodes, num_edges);

    std::vector<u32> arg_offset{};
    if (aux) {
      // when aux exists, we steal slot 0 for aux args
      input_offset.push_back(g.aux_args());
    } else {
      // aux does not exist, put empty vector at slot 0 (root, never used)
      input_offset.push_back(arg_offset);
    }
    for (size_t i = 1; i < n_nodes; ++i) {
      arg_offset.clear();
      for (auto const &[key, port] : g_args.at(keys[i])) {
        arg_offset.push_back(record_offset[idx[key]] + port);
      }
      input_offset.push_back(arg_offset);
    }

    if (supp) {
      num_edges = 0;
      for (auto const &[_, ports] : g.supp_link()) {
        num_edges += ports.size();
      }
      param_node.reserve(g.supp_link().size());
      param_port.reserve(g.supp_link().size(), num_edges);
      for (auto const &[key, ports] : g.supp_link()) {
        param_node.push_back(idx[key]);
        param_port.push_back(ports);
      }
      param_size = static_cast<u32>(supp->num_outputs());
    }

    output_offset.reserve(g.output().size());
    for (auto const &[key, port] : g.output()) {
      output_offset.emplace_back(record_offset[idx[key]] + port);
    }

    init_arena(nodes, aux, supp);
    copy_data(nodes, aux, supp);
  }

  std::span<unique_node_ptr const> operator[](size_t igrp) const noexcept {
    return {node_ptrs.data() + igrp * n_nodes, n_nodes};
  }

  bool has_window() const noexcept { return !win_ptrs.empty(); }
  bool has_param() const noexcept { return !param_ptrs.empty(); }

  unique_aux_ptr const &window(size_t igrp) const noexcept { return win_ptrs[igrp]; }
  unique_node_ptr const &param(size_t igrp) const noexcept { return param_ptrs[igrp]; }

  size_t size() const noexcept { return n_nodes; }
  size_t num_nodes() const noexcept { return n_nodes; }
  size_t num_groups() const noexcept { return n_grp; }

private:
  template <typename G>
  void validate(G const &g) {
    auto const &g_preds = g.pred();
    auto const &g_args = g.args();
    auto const &g_out = g.output();

    bool root_found = false;
    for (auto const &[key, preds] : g_preds) {
      if (preds.empty()) {
        if (root_found) {
          throw std::invalid_argument("Multiple root nodes detected in graph.");
        }
        auto root = std::dynamic_pointer_cast<node_type>(g.node(key));
        if (g.aux()) {
          auto root_size = root->num_outputs();
          for (auto port : g.aux_args()) {
            if (port >= root_size) {
              throw std::invalid_argument("Incompatible auxiliary node connections in graph.");
            }
          }
        }
        root_found = true;
      }
    }

    auto supp = std::dynamic_pointer_cast<node_type>(g.supp_root());

    for (auto const &[key, _] : g_preds) {
      for (auto const &[arg_key, arg_port] : g_args.at(key)) {
        auto pred = std::dynamic_pointer_cast<node_type>(g.node(arg_key));
        if (arg_port >= pred->num_outputs()) {
          throw std::invalid_argument("Incompatible node connections in graph.");
        }
      }
      if (supp && g.supp_link().contains(key)) {
        for (auto port : g.supp_link().at(key)) {
          if (port >= supp->num_outputs()) {
            throw std::invalid_argument("Incompatible parameter node connections in graph.");
          }
        }
      }
    }

    for (auto const &[key, port] : g_out) {
      auto out = std::dynamic_pointer_cast<node_type>(g.node(key));
      if (out == nullptr) {
        throw std::invalid_argument("Invalid output node.");
      }
      if (port >= out->num_outputs()) {
        throw std::invalid_argument("Incompatible output node connections in graph.");
      }
    }
  }

  void init_arena(std::vector<shared_node_ptr> const &nodes, shared_aux_ptr const &win, shared_node_ptr const &param) {
    // Memory layout:
    // | win_ptrs | param_ptrs | node_ptrs | PADDING | node grp | PADDING | ... | node grp | PADDING |
    //
    // node grp = | win | param | node 0 | node 1 | ... | node n |

    size_t arena_size = 0;

    size_t win_ptrs_size = win ? heap_alloc_size(win_ptrs, n_grp) : 0;
    size_t param_ptrs_size = param ? heap_alloc_size(param_ptrs, n_grp) : 0;
    size_t node_ptrs_size = heap_alloc_size(node_ptrs, n_grp * n_nodes);
    arena_size += aligned_size(win_ptrs_size + param_ptrs_size + node_ptrs_size, cacheline_size);

    size_t max_align = cacheline_size;

    size_t win_size = 0;
    if constexpr (!std::is_void_v<aux_type>)
      if (win) {
        auto align = win->clone_align();
        max_align = std::max(max_align, align);
        win_size = aligned_size(win->clone_size(), align);
      }

    size_t par_size = 0;
    if (param) {
      auto align = param->clone_align();
      max_align = std::max(max_align, align);
      par_size = aligned_size(param->clone_size(), align);
    }

    size_t node_size = 0;
    for (auto const &node : nodes) {
      auto align = node->clone_align();
      max_align = std::max(max_align, align);
      node_size += aligned_size(node->clone_size(), align);
    }
    size_t node_grp_size = aligned_size(win_size + par_size + node_size, max_align);

    arena_size += n_grp * node_grp_size;
    // add extra max_align to ensure space fits
    arena_size += max_align;

    arena_storage.resize(arena_size);
    arena = fixed_buffer_resource{arena_storage.data(), arena_storage.size()};

    // consolidate ptrs at start of arena
    if (win) {
      win_ptrs = std::pmr::vector<unique_aux_ptr>(&arena);
      win_ptrs.reserve(n_grp);
    }
    if (param) {
      param_ptrs = std::pmr::vector<unique_node_ptr>(&arena);
      param_ptrs.reserve(n_grp);
    }
    node_ptrs = std::pmr::vector<unique_node_ptr>(&arena);
    node_ptrs.reserve(n_grp * n_nodes);
  }

  void copy_data(std::vector<shared_node_ptr> const &nodes, shared_aux_ptr const &win, shared_node_ptr const &param) {
    void *mem;
    bool grp_aligned;
    for (size_t igrp = 0; igrp < n_grp; ++igrp) {
      grp_aligned = false;
      if constexpr (!std::is_void_v<aux_type>)
        if (win) {
          auto win_size = win->clone_size();
          auto win_align = win->clone_align();
          if (!grp_aligned) {
            win_align = std::max(cacheline_size, win_align);
            grp_aligned = true;
          }
          mem = arena.allocate(win_size, win_align);
          win_ptrs.emplace_back(win->clone_at(mem));
        }
      if (param) {
        auto par_size = param->clone_size();
        auto par_align = param->clone_align();
        if (!grp_aligned) {
          par_align = std::max(cacheline_size, par_align);
          grp_aligned = true;
        }
        mem = arena.allocate(par_size, par_align);
        param_ptrs.emplace_back(param->clone_at(mem));
      }
      for (size_t i = 0; i < nodes.size(); ++i) {
        auto node_size = nodes[i]->clone_size();
        auto node_align = nodes[i]->clone_align();
        if (!grp_aligned) {
          node_align = std::max(cacheline_size, node_align);
          grp_aligned = true;
        }
        mem = arena.allocate(node_size, node_align);
        node_ptrs.emplace_back(nodes[i]->clone_at(mem));
      }
    }
  }

  size_t const n_grp;
  size_t const n_nodes;
  std::pmr::vector<unique_aux_ptr> win_ptrs;
  std::pmr::vector<unique_node_ptr> param_ptrs;
  std::pmr::vector<unique_node_ptr> node_ptrs;

  using u32_alloc = rebind_alloc<Alloc, u32>;

public:
  u32 record_size;                                  // total size of record
  u32 param_size;                                   // total size of param record
  std::vector<u32, u32_alloc> record_offset;        // i-th node -> write offset in rec
  flat_multivect<u32, u32, u32_alloc> input_offset; // i-th node -> [read offset in rec], 0-th is aux args if exists
  std::vector<u32, u32_alloc> output_offset;        // i-th output -> offset in rec
  std::vector<u32, u32_alloc> param_node;           // node ids that connect to param root
  flat_multivect<u32, u32, u32_alloc> param_port;   // i-th param node -> [port in param root]
};
} // namespace opflow::detail
