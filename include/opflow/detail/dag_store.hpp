#pragma once

#include <algorithm>
#include <memory>
#include <queue>
#include <span>
#include <unordered_map>
#include <vector>

#include "../common.hpp"

#include "fixed_buffer_resource.hpp"
#include "flat_multivect.hpp"
#include "utils.hpp"

namespace opflow::detail {
template <dag_node_base T, typename Alloc = std::allocator<T>>
class dag_store {
  using byte_alloc = rebind_alloc<Alloc, std::byte>;
  std::vector<std::byte, byte_alloc> arena_storage;
  fixed_buffer_resource arena;

public:
  using data_type = typename T::data_type;
  using base_type = T;

  using node_type = std::unique_ptr<base_type, arena_deleter<base_type>>;
  using shared_node_type = std::shared_ptr<base_type>;

  template <typename G>
  dag_store(G const &g, size_t n_group = 1, Alloc alloc = Alloc{})
      : arena_storage(alloc), arena(nullptr, 0), n_grp(n_group), n_nodes(g.size()), ptrs(), record_size(),
        record_offset(alloc), input_offset(alloc), output_offset(alloc) {
    if (n_group == 0) {
      throw std::invalid_argument("Number of groups must be greater than 0.");
    }

    using key_type = typename G::key_type;
    using key_hash = typename G::key_hash;

    std::unordered_map<key_type, u32, key_hash, std::equal_to<>> in_degree;
    std::queue<key_type> ready;

    std::vector<key_type> sorted;
    std::vector<shared_node_type> sorted_nodes;
    std::unordered_map<key_type, u32, key_hash, std::equal_to<>> sorted_id;

    in_degree.reserve(n_nodes);

    sorted.reserve(n_nodes);
    sorted_nodes.reserve(n_nodes);
    sorted_id.reserve(n_nodes);

    for (auto const &[node, preds] : g.get_pred()) {
      auto n_pred = preds.size();
      in_degree.emplace(node, n_pred);
      if (n_pred == 0) {
        ready.push(node);
      }
    }

    while (!ready.empty()) {
      auto current = std::move(ready.front());
      ready.pop();
      sorted.push_back(current);

      // update successors
      auto succ_it = g.get_succ().find(current);
      assert(succ_it != g.get_succ().end() && "[BUG] Node not found in successors map.");
      for (auto const &succ : succ_it->second) {
        if (--in_degree[succ] == 0) {
          ready.push(succ);
        }
      }
    }

    if (sorted.size() != n_nodes) {
      throw std::runtime_error("Cyclic graph detected.");
    }

    // Build the sorted_id mapping first
    for (u32 i = 0; i < sorted.size(); ++i) {
      sorted_id[sorted[i]] = i;
      sorted_nodes.push_back(g.get_node(sorted[i]));
    }

    // prepare arena and copy nodes

    init_arena(sorted_nodes);
    copy_nodes(sorted_nodes);

    size_t num_edges = 0;
    for (auto const &[_, args] : g.get_args()) {
      num_edges += args.size();
    }

    record_size = 0;
    record_offset.reserve(n_nodes);
    for (size_t i = 0; i < n_nodes; ++i) {
      record_offset.emplace_back(record_size);
      record_size += ptrs[i]->num_outputs();
    }

    std::vector<u32> args{};
    input_offset.reserve(n_nodes, num_edges);
    for (size_t i = 0; i < n_nodes; ++i) {
      args.clear();
      for (auto const &[node, port] : g.args_of(sorted[i])) {
        if (port >= g.get_node(node)->num_outputs()) {
          throw std::runtime_error("Incompatible node connections in graph.");
        }
        args.push_back(record_offset[sorted_id[node]] + port);
      }
      input_offset.push_back(args);
    }

    auto const &output_nodes = g.get_output();
    output_offset.reserve(output_nodes.size());
    for (auto const &node : output_nodes) {
      auto it = sorted_id.find(node);
      if (it == sorted_id.end()) {
        throw std::runtime_error("Output node not found in graph.");
      }
      output_offset.emplace_back(record_offset[it->second], g.get_node(node)->num_outputs());
    }
  }

  std::span<node_type const> operator[](size_t igrp) const noexcept { return {ptrs.data() + igrp * n_nodes, n_nodes}; }

  size_t size() const noexcept { return n_nodes; }
  size_t num_nodes() const noexcept { return n_nodes; }
  size_t num_groups() const noexcept { return n_grp; }

private:
  void init_arena(std::vector<shared_node_type> const &nodes) {
    // Memory layout:
    // | ptr grp 0 id 0 | ptr grp 0 id 1 | ... | ptr grp m id n | PADDING | node grp 0 | PADDING | ... |
    // | <---          CONCURRENT READONLY ACCESS          ---> |
    //
    // node grp = | node 0 | node 1 | ... | node n |
    //            | <--- SINGLE THREAD ACCESS ---> |

    size_t total = 0;

    size_t ptr_size = heap_alloc_size(ptrs, n_grp * n_nodes);
    total = aligned_size(ptr_size, cacheline_size);

    size_t align = 0;
    size_t max_align = cacheline_size;

    size_t node_size = 0;
    for (auto const &node : nodes) {
      align = node->clone_align();
      max_align = std::max(max_align, align);
      node_size += aligned_size(node->clone_size(), align);
    }
    size_t node_grp_size = aligned_size(node_size, max_align);
    total += node_grp_size * n_grp;

    // add extra max_align to ensure space fits
    total += max_align;

    // now initialise arena
    arena_storage.resize(total);
    arena = fixed_buffer_resource(arena_storage.data(), arena_storage.size());

    // consolidate memory layout
    ptrs = std::pmr::vector<node_type>(&arena);
    ptrs.reserve(n_grp * n_nodes);
  }

  void copy_nodes(std::vector<std::shared_ptr<base_type>> const &nodes) {
    for (size_t i_copy = 0; i_copy < n_grp; ++i_copy) {
      for (size_t i = 0; i < nodes.size(); ++i) {
        size_t align = nodes[i]->clone_align();
        if (i == 0) {
          align = std::max(cacheline_size, align);
        }
        void *mem = arena.allocate(nodes[i]->clone_size(), align);
        ptrs.emplace_back(nodes[i]->clone_at(mem));
      }
    }
  }

  size_t const n_grp;
  size_t const n_nodes;
  std::pmr::vector<node_type> ptrs;

  using offset = offset_type<u32>;

  using u32_alloc = rebind_alloc<Alloc, u32>;
  using offset_alloc = rebind_alloc<Alloc, offset>;

public:
  u32 record_size;
  std::vector<u32, u32_alloc> record_offset;        // i-th node -> offset in record
  flat_multivect<u32, u32, u32_alloc> input_offset; // i-th node -> [offset in rec]
  std::vector<offset, offset_alloc> output_offset;  // i-th out -> <offset in rec, node->num_outputs()>
};
} // namespace opflow::detail
