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
  using data_type = typename T::data_type;

  using node_base = T;
  using aux_base = U;

  using node_type = std::unique_ptr<node_base, arena_deleter<node_base>>;
  using shared_node_type = std::shared_ptr<node_base>;

  using aux_type = std::unique_ptr<aux_base, arena_deleter<aux_base>>;
  using shared_aux_type = std::shared_ptr<aux_base>;

  template <typename G>
  graph_store(G const &g, size_t n_group, Alloc alloc)
      : // arena
        arena_storage(alloc), arena(nullptr, 0),
        // data
        n_grp(n_group), n_nodes(g.size()), win_ptrs(), node_ptrs(),
        // topo
        record_offset(alloc), input_offset(alloc), output_offset(alloc) {
    static_assert((std::same_as<typename G::node_base, node_base> && std::same_as<typename G::aux_base, aux_base> &&
                   std::same_as<typename G::node_type, shared_node_type> &&
                   std::same_as<typename G::aux_type, shared_aux_type>),
                  "Graph node and aux types must match graph_store node and aux types.");

    if (!g.validate()) {
      throw std::invalid_argument("Graph validation failed.");
    }

    if (n_grp == 0) {
      throw std::invalid_argument("Number of groups must be greater than 0.");
    }

    shared_aux_type aux{};
    if constexpr (!std::same_as<aux_base, void>) {
      aux = g.aux();
      if (!aux) {
        throw std::invalid_argument("Graph auxiliary data is null.");
      }
    }

    using key_type = typename G::key_type;
    using hash_type = typename G::key_hash;
    using lookup_type = std::unordered_map<key_type, u32, hash_type, std::equal_to<>>;
    std::vector<key_type> keys;          // sorted node keys
    std::vector<shared_node_type> nodes; // sorted node instances
    lookup_type idx;                     // node key -> sorted index

    lookup_type in_degree;
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
      assert(succ_it != g.succ().end() && "[BUG] Node not found in successors map.");
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

    u32 record_size = 0;
    record_offset.reserve(n_nodes);
    for (size_t i = 0; i < n_nodes; ++i) {
      record_offset.emplace_back(record_size);
      record_size += nodes[i]->num_outputs();
    }

    u32 num_edges = aux ? static_cast<u32>(g.aux_args().size()) : 0;
    for (auto const &[_, args] : g.args()) {
      num_edges += args.size();
    }
    input_offset.reserve(n_nodes, num_edges);

    std::vector<u32> arg_offset{};
    if (aux) {
      for (auto const &[key, port] : g.aux_args()) {
        if (port >= nodes[0]->num_outputs()) {
          throw std::runtime_error("Incompatible auxiliary node connections in graph.");
        }
        arg_offset.push_back(port); // aux is always node 0
      }
      input_offset.push_back(arg_offset);
    }

    for (size_t i = 0; i < n_nodes; ++i) {
      arg_offset.clear();
      for (auto const &[key, port] : g.args_of(keys[i])) {
        if (port >= g.node(key)->num_outputs()) {
          throw std::runtime_error("Incompatible node connections in graph.");
        }
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

  std::span<node_type const> operator[](size_t igrp) const noexcept {
    return {node_ptrs.data() + igrp * n_nodes, n_nodes};
  }

  bool has_window() const noexcept { return !win_ptrs.empty(); }

  aux_type const &window(size_t igrp) const noexcept { return win_ptrs[igrp]; }

  size_t size() const noexcept { return n_nodes; }
  size_t num_nodes() const noexcept { return n_nodes; }
  size_t num_groups() const noexcept { return n_grp; }

private:
  void init_arena(std::vector<shared_node_type> const &nodes, shared_aux_type const &win) {
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
    if constexpr (!std::is_void_v<aux_base>) {
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
    if constexpr (!std::is_void_v<aux_base>) {
      win_ptrs = std::pmr::vector<aux_type>(&arena);
      win_ptrs.reserve(n_grp);
    }
    node_ptrs = std::pmr::vector<node_type>(&arena);
    node_ptrs.reserve(n_grp * n_nodes);
  }

  void copy_data(std::vector<shared_node_type> const &nodes, shared_aux_type const &win) {
    void *mem;
    if constexpr (!std::is_void_v<aux_base>) {
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
  std::pmr::vector<aux_type> win_ptrs;
  std::pmr::vector<node_type> node_ptrs;

  using u32_alloc = rebind_alloc<Alloc, u32>;

public:
  std::vector<u32, u32_alloc> record_offset;        // i-th node -> offset in record
  flat_multivect<u32, u32, u32_alloc> input_offset; // i-th node -> [offset in rec]
  std::vector<u32, u32_alloc> output_offset;        // i-th out -> <offset in rec, node->num_outputs()>
};

// deduction guide: G::node_base -> T, G::aux_base -> U

template <typename G>
graph_store(G const &) -> graph_store<typename G::node_base, typename G::aux_base, std::allocator<void>>;

template <typename G>
graph_store(G const &, size_t) -> graph_store<typename G::node_base, typename G::aux_base, std::allocator<void>>;

template <typename G, typename Alloc>
graph_store(G const &, size_t, Alloc) -> graph_store<typename G::node_base, typename G::aux_base, Alloc>;
} // namespace opflow::detail
