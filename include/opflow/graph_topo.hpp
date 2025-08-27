#pragma once

#include <algorithm>
#include <memory>
#include <queue>
#include <span>
#include <unordered_map>
#include <vector>

#include "common.hpp"
#include "detail/aligned_allocator.hpp"
#include "detail/fixed_buffer_resource.hpp"
#include "detail/flat_multivect.hpp"
#include "graph_node.hpp"

namespace opflow {
template <dag_node T>
class graph_topo {
  //  std::pmr::monotonic_buffer_resource arena;
  std::vector<std::byte, detail::cacheline_aligned_alloc<std::byte>> arena_storage;
  detail::fixed_buffer_resource arena;

public:
  using base_type = T;

  using node_type = std::unique_ptr<base_type, detail::arena_deleter<base_type>>;
  using shared_node_type = std::shared_ptr<base_type>;

  struct output_type {
    uint32_t id;   ///< output node id
    uint32_t size; ///< output size

    friend bool operator==(output_type const &lhs, output_type const &rhs) noexcept = default;
  };

  graph_topo(graph_node<base_type> const &g, size_t n_group = 1)
      : arena_storage(), arena(nullptr, 0), n_grp(n_group), n_nodes(), nodes(), outputs(), pred_map(), arg_map() {
    if (n_group == 0) {
      throw std::invalid_argument("Number of groups must be greater than 0.");
    }

    std::unordered_map<shared_node_type, size_t, detail::ptr_hash<base_type>, std::equal_to<>> in_degree, sorted_id;
    std::queue<shared_node_type> ready;
    std::vector<shared_node_type> sorted;

    in_degree.reserve(g.size());
    sorted_id.reserve(g.size());
    sorted.reserve(g.size());

    for (auto const &[node, pred] : g.get_pred()) {
      auto n_pred = pred.size();
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

    if (sorted.size() != g.size()) {
      throw std::runtime_error("Cyclic graph detected.");
    }

    // Build the sorted_id mapping first
    for (size_t i = 0; i < sorted.size(); ++i) {
      sorted_id[sorted[i]] = i;
    }

    // Then build the predecessors map
    std::vector<size_t> tmp_id;
    std::vector<arg_type> tmp_args;
    for (size_t i = 0; i < sorted.size(); ++i) {
      tmp_id.clear();
      tmp_args.clear();
      for (auto const &pred : g.pred_of(sorted[i])) {
        tmp_id.push_back(sorted_id[pred]);
      }
      for (auto const &arg : g.args_of(sorted[i])) {
        tmp_args.emplace_back(sorted_id[arg.node], arg.port);
      }
      auto test_id = pred_map.push_back(tmp_id);
      auto test_args_id = arg_map.push_back(tmp_args);
      assert(test_id == i && "[BUG] Preds ID mismatch when constructing preds map.");
      assert(test_args_id == i && "[BUG] Args ID mismatch when constructing args map.");
    }

    // prepare arena and copy nodes
    auto const &output_nodes = g.get_output();
    size_t n_output = output_nodes.size();
    init_arena(sorted, n_output);
    nodes = std::pmr::vector<node_type>(&arena);
    outputs = std::pmr::vector<output_type>(&arena);
    copy_nodes(sorted, n_output);
    n_nodes = sorted.size();

    for (auto const &node : output_nodes) {
      auto it = sorted_id.find(node);
      if (it == sorted_id.end()) {
        throw std::runtime_error("Output node not found in graph.");
      }
      outputs.emplace_back(static_cast<uint32_t>(it->second), static_cast<uint32_t>(node->num_outputs()));
    }
  }

  std::span<node_type> operator[](size_t igrp) noexcept { return nodes_of(igrp); }
  std::span<node_type const> operator[](size_t igrp) const noexcept { return nodes_of(igrp); }

  std::span<node_type> nodes_of(size_t igrp) noexcept { return {nodes.data() + igrp * n_nodes, n_nodes}; }
  std::span<node_type const> nodes_of(size_t igrp) const noexcept { return {nodes.data() + igrp * n_nodes, n_nodes}; }

  auto pred_of(size_t id) const noexcept { return pred_map[id]; }

  auto args_of(size_t id) const noexcept { return arg_map[id]; }

  std::span<output_type const> nodes_out() const noexcept { return {outputs.data(), outputs.size()}; }

  size_t size() const noexcept { return n_nodes; }

  size_t num_edges() const noexcept { return arg_map.total_size(); }

  size_t num_nodes() const noexcept { return n_nodes; }

  size_t num_groups() const noexcept { return n_grp; }

private:
  void init_arena(std::vector<std::shared_ptr<base_type>> const &graph_nodes, size_t n_output) {
    // before we copy to arena, we calculate total memory needed
    // 1. we need to make n copies of nodes
    // 2. we need to account for node ptr and node out storage
    // 3. initialise arena storage to proper size

    // storage:
    // | ptr | ptr | ... | out | out | ... | PADDING to cacheline | node grp | PADDING to cacheline | node grp | ... |

    size_t total = 0;
    for (size_t i = 0; i < graph_nodes.size(); ++i) {
      size_t align = graph_nodes[i]->clone_align();
      if (i == 0) {
        align = std::max(detail::cacheline_size, align);
      }
      total += detail::aligned_size(graph_nodes[i]->clone_size(), align);
    }
    total *= n_grp;

    size_t vec_ptr_size = detail::aligned_size(sizeof(node_type) * graph_nodes.size() * n_grp, alignof(node_type));
    size_t vec_out_size = detail::aligned_size(sizeof(output_type) * n_output, alignof(output_type));
    size_t vector_size =
        detail::aligned_size(vec_ptr_size + vec_out_size, std::max(detail::cacheline_size, alignof(node_type)));
    total += vector_size;

    // now initialise arena
    arena_storage.resize(total);
    arena = detail::fixed_buffer_resource(arena_storage.data(), arena_storage.size());
  }

  void copy_nodes(std::vector<std::shared_ptr<base_type>> const &graph_nodes, size_t n_output) {
    // because vector will grow and fragment our arena, we reserve first
    nodes.reserve(n_grp * graph_nodes.size());
    outputs.reserve(n_output);
    for (size_t i_copy = 0; i_copy < n_grp; ++i_copy) {
      for (size_t i = 0; i < graph_nodes.size(); ++i) {
        size_t align = graph_nodes[i]->clone_align();
        if (i == 0) {
          align = std::max(detail::cacheline_size, align);
        }
        void *mem = arena.allocate(graph_nodes[i]->clone_size(), align);
        nodes.emplace_back(graph_nodes[i]->clone_at(mem));
      }
    }
  }

  struct arg_type {
    uint32_t node;
    uint32_t port;
  };

  size_t n_grp;
  size_t n_nodes;
  std::pmr::vector<node_type> nodes;
  std::pmr::vector<output_type> outputs;

  detail::flat_multivect<size_t> pred_map;  ///< Flattened storage of predecessors: id -> [pred ids]
  detail::flat_multivect<arg_type> arg_map; ///< Flattened storage of arguments: id -> [pred:port]
};

} // namespace opflow
