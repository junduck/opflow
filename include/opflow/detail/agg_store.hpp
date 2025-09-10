#pragma once

#include <memory>
#include <vector>

#include "opflow/common.hpp"
#include "opflow/graph_agg.hpp"
#include "opflow/window_base.hpp"

#include "fixed_buffer_resource.hpp"
#include "flat_multivect.hpp"
#include "utils.hpp"

namespace opflow::detail {
template <dag_node_base T, typename Alloc = std::allocator<T>>
struct agg_store {
  using byte_alloc = rebind_alloc<Alloc, std::byte>;
  std::vector<std::byte, byte_alloc> arena_storage;
  fixed_buffer_resource arena;

public:
  using data_type = typename T::data_type;
  using base_type = T;
  using window_base_type = window_base<data_type>;

  using window_type = std::unique_ptr<window_base_type, arena_deleter<window_base_type>>;
  using shared_window_type = std::shared_ptr<window_base_type>;

  using node_type = std::unique_ptr<base_type, arena_deleter<base_type>>;
  using shared_node_type = std::shared_ptr<base_type>;

  template <typename G>
  agg_store(G const &g, size_t n_group = 1, Alloc alloc = Alloc{})
      : arena_storage(alloc), arena(nullptr, 0), n_grp(n_group), n_nodes(g.size()), win_ptrs(), node_ptrs(),
        record_size(), record_offset(alloc), input_column(alloc), win_column(alloc) {
    if (n_group == 0) {
      throw std::invalid_argument("Number of groups must be greater than 0.");
    }

    init_arena(g);
    copy_nodes(g);

    auto nodes = g.get_nodes();

    record_size = 0;
    record_offset.reserve(n_nodes);
    for (size_t i = 0; i < n_nodes; ++i) {
      record_offset.emplace_back(record_size);
      record_size += nodes[i]->num_outputs();
    }

    size_t total = 0;
    for (size_t i = 0; i < n_nodes; ++i) {
      total += g.input_column(i).size();
    }
    input_column.reserve(n_nodes, total);
    for (size_t i = 0; i < n_nodes; ++i) {
      input_column.push_back(g.input_column(i));
    }

    auto win_col = g.window_input_column();
    win_column.assign(win_col.begin(), win_col.end());
  }

  window_type const &window(size_t igrp) const noexcept { return win_ptrs[igrp]; }

  std::span<node_type const> operator[](size_t igrp) const noexcept {
    return {node_ptrs.data() + igrp * n_nodes, n_nodes};
  }

  size_t size() const noexcept { return n_nodes; }
  size_t num_nodes() const noexcept { return n_nodes; }
  size_t num_groups() const noexcept { return n_grp; }

private:
  template <typename G>
  void init_arena(G const &graph) {
    // Memory layout:
    // | ptr win | ..... | ptr node | ..... | PADDING | node grp | PADDING | ... | node grp | ... |
    // | <---CONCURRENT READONLY ACCESS---> |
    //
    // ptr win = | ptr win 0 | ptr win 1 | ... | ptr win n |
    // ptr node = | ptr grp 0 node 0 | ptr grp 0 node 1 | ... | ptr grp 0 node n | ... | ptr grp m node n |
    //
    // node grp = | win | node 0 | node 1 | ... | node n |
    //            | <---    SINGLE THREAD ACCESS    ---> |

    auto nodes = graph.get_nodes();
    auto const &win = graph.get_window();

    size_t total = 0;

    size_t win_ptr_size = heap_alloc_size(win_ptrs, n_grp);
    size_t node_ptr_size = heap_alloc_size(node_ptrs, n_grp * n_nodes);
    total = aligned_size(win_ptr_size + node_ptr_size, cacheline_size);

    size_t align = 0;
    size_t max_align = cacheline_size;

    align = win->clone_align();
    max_align = std::max(max_align, align);
    size_t win_size = aligned_size(win->clone_size(), align);

    size_t node_size = 0;
    for (auto const &node : nodes) {
      align = node->clone_align();
      max_align = std::max(max_align, align);
      node_size += aligned_size(node->clone_size(), align);
    }
    size_t node_grp_size = aligned_size(win_size + node_size, max_align);
    total += node_grp_size * n_grp;

    // add extra max_align to ensure space fits
    total += max_align;

    // now initialise arena
    arena_storage.resize(total);
    arena = fixed_buffer_resource(arena_storage.data(), arena_storage.size());

    // consolidate memory layout
    win_ptrs = std::pmr::vector<window_type>(&arena);
    win_ptrs.reserve(n_grp);

    node_ptrs = std::pmr::vector<node_type>(&arena);
    node_ptrs.reserve(n_grp * n_nodes);
  }

  template <typename G>
  void copy_nodes(G const &graph) {
    auto nodes = graph.get_nodes();
    auto const &win = graph.get_window();

    auto win_size = win->clone_size();
    auto win_align = std::max(cacheline_size, win->clone_align());

    void *mem = nullptr;
    for (size_t i_copy = 0; i_copy < n_grp; ++i_copy) {
      mem = arena.allocate(win_size, win_align);
      win_ptrs.emplace_back(win->clone_at(mem));
      for (auto const &node : nodes) {
        mem = arena.allocate(node->clone_size(), node->clone_align());
        node_ptrs.emplace_back(node->clone_at(mem));
      }
    }
  }

  size_t const n_grp;
  size_t const n_nodes;

  std::pmr::vector<window_type> win_ptrs; ///< windows, size = n_grp
  std::pmr::vector<node_type> node_ptrs;  ///< nodes, size = n_grp * n_nodes

  using u32_alloc = rebind_alloc<Alloc, u32>;

public:
  u32 record_size;
  std::vector<u32> record_offset;                   // i-th node -> offset in record
  flat_multivect<u32, u32, u32_alloc> input_column; // i-th node -> [input column]
  std::vector<u32> win_column;                      // [window column]
};
} // namespace opflow::detail
