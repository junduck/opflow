#pragma once

#include <queue>

#include "../common.hpp"
#include "fixed_buffer_resource.hpp"
#include "flat_multivect.hpp"
#include "utils.hpp"

namespace opflow::detail {
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
        n_grp(n_group), n_nodes(g.size()), win_ptrs(), node_ptrs(),
        // topo
        record_size(), record_offset(alloc), input_offset(alloc), output_offset(alloc) {
    static_assert((std::same_as<typename G::node_type, node_type> && //
                   std::same_as<typename G::aux_type, aux_type> &&
                   std::same_as<typename G::shared_node_ptr, shared_node_ptr> &&
                   std::same_as<typename G::shared_aux_ptr, shared_aux_ptr>),
                  "Graph node and aux types must match graph_store node and aux types.");
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
    if constexpr (!std::is_void_v<typename G::aux_type>) {
      aux = g.aux();
    }

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
      nodes.push_back(g.node(keys[i]));
    }

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
      for (auto const &[key, port] : g.aux_args()) {
        arg_offset.push_back(port); // aux is always node 0
      }
      input_offset.push_back(arg_offset);
    }

    for (size_t i = 0; i < n_nodes; ++i) {
      arg_offset.clear();
      for (auto const &[key, port] : g_args.at(keys[i])) {
        arg_offset.push_back(record_offset[idx[key]] + port);
      }
      input_offset.push_back(arg_offset);
    }

    auto const &outputs = g.output();
    output_offset.reserve(outputs.size());
    for (auto const &[key, port] : outputs) {
      output_offset.emplace_back(record_offset[idx[key]] + port);
    }

    init_arena(nodes, aux);
    copy_data(nodes, aux);
  }

  std::span<unique_node_ptr const> operator[](size_t igrp) const noexcept {
    return {node_ptrs.data() + igrp * n_nodes, n_nodes};
  }

  bool has_window() const noexcept { return !win_ptrs.empty(); }

  unique_aux_ptr const &window(size_t igrp) const noexcept { return win_ptrs[igrp]; }

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
        } else if constexpr (!std::is_void_v<typename G::aux_type>) {
          auto aux = g.aux();
          if (!aux) {
            throw std::invalid_argument("Graph auxiliary data is null.");
          }

          auto root = g.node(key);
          if (typeid(*root) != typeid(dag_root_type<typename G::node_type>)) {
            throw std::invalid_argument("Root node must be of type dag_root<T>.");
          }

          auto root_size = root->num_outputs();
          for (auto const &[arg_key, arg_port] : g.aux_args()) {
            if (g.node(arg_key) != root) {
              throw std::invalid_argument("Auxiliary node can only connect to root node.");
            }
            if (arg_port >= root_size) {
              throw std::invalid_argument("Incompatible auxiliary node connections in graph.");
            }
          }
        }
        root_found = true;
      }
    }

    for (auto const &[key, _] : g_preds) {
      for (auto const &[arg_key, arg_port] : g_args.at(key)) {
        if (arg_port >= g.node(arg_key)->num_outputs()) {
          throw std::invalid_argument("Incompatible node connections in graph.");
        }
      }
    }

    for (auto const &[key, port] : g_out) {
      if (g.node(key) == nullptr) {
        throw std::invalid_argument("Output node is null.");
      }
      if (port >= g.node(key)->num_outputs()) {
        throw std::invalid_argument("Incompatible output node connections in graph.");
      }
    }
  }

  void init_arena(std::vector<shared_node_ptr> const &nodes, shared_aux_ptr const &win) {
    // Memory layout:
    // | [win_ptrs] | node_ptrs | PADDING | node grp | PADDING | ... | node grp | PADDING |
    //
    // node grp = | [win] | node 0 | node 1 | ... | node n |

    size_t arena_size = 0;

    size_t win_ptrs_size = win ? 0 : heap_alloc_size(win_ptrs, n_grp);
    size_t node_ptrs_size = heap_alloc_size(node_ptrs, n_grp * n_nodes);
    arena_size += aligned_size(win_ptrs_size + node_ptrs_size, cacheline_size);

    size_t align;
    size_t max_align = cacheline_size;

    size_t win_size = 0;
    if constexpr (!std::is_void_v<aux_type>) {
      align = win->clone_align();
      max_align = std::max(max_align, align);
      win_size = aligned_size(win->clone_size(), align);
    }

    size_t node_size = 0;
    for (auto const &node : nodes) {
      align = node->clone_align();
      max_align = std::max(max_align, align);
      node_size += aligned_size(node->clone_size(), align);
    }
    size_t node_grp_size = aligned_size(win_size + node_size, max_align);

    arena_size += n_grp * node_grp_size;
    // add extra max_align to ensure space fits
    arena_size += max_align;

    arena_storage.resize(arena_size);
    arena = fixed_buffer_resource{arena_storage.data(), arena_storage.size()};

    // consolidate memory layout
    if constexpr (!std::is_void_v<aux_type>) {
      win_ptrs = std::pmr::vector<unique_aux_ptr>(&arena);
      win_ptrs.reserve(n_grp);
    }
    node_ptrs = std::pmr::vector<unique_node_ptr>(&arena);
    node_ptrs.reserve(n_grp * n_nodes);
  }

  void copy_data(std::vector<shared_node_ptr> const &nodes, shared_aux_ptr const &win) {
    void *mem;
    if constexpr (!std::is_void_v<aux_type>) {
      auto win_size = win->clone_size();
      auto win_align = std::max(cacheline_size, win->clone_align());
      for (size_t i = 0; i < n_grp; ++i) {
        mem = arena.allocate(win_size, win_align);
        win_ptrs.emplace_back(win->clone_at(mem));
        for (auto const &node : nodes) {
          mem = arena.allocate(node->clone_size(), node->clone_align());
          node_ptrs.emplace_back(node->clone_at(mem));
        }
      }
    } else {
      for (size_t igrp = 0; igrp < n_grp; ++igrp) {
        for (size_t i = 0; i < nodes.size(); ++i) {
          auto align = nodes[i]->clone_align();
          if (i == 0) {
            align = std::max(cacheline_size, align);
          }
          mem = arena.allocate(nodes[i]->clone_size(), align);
          node_ptrs.emplace_back(nodes[i]->clone_at(mem));
        }
      }
    }
  }

  size_t const n_grp;
  size_t const n_nodes;
  std::pmr::vector<unique_aux_ptr> win_ptrs;
  std::pmr::vector<unique_node_ptr> node_ptrs;

  using u32_alloc = rebind_alloc<Alloc, u32>;

public:
  u32 record_size;                                  // total size of record
  std::vector<u32, u32_alloc> record_offset;        // i-th node -> offset in record
  flat_multivect<u32, u32, u32_alloc> input_offset; // i-th node -> [offset in rec]
  std::vector<u32, u32_alloc> output_offset;        // i-th out -> offset in record
};

// deduction guide: G::node_type -> T, G::aux_type -> U

template <typename G>
graph_store(G const &) -> graph_store<typename G::node_type, typename G::aux_type, std::allocator<void>>;

template <typename G>
graph_store(G const &, size_t) -> graph_store<typename G::node_type, typename G::aux_type, std::allocator<void>>;

template <typename G, typename Alloc>
graph_store(G const &, size_t, Alloc) -> graph_store<typename G::node_type, typename G::aux_type, Alloc>;
} // namespace opflow::detail
