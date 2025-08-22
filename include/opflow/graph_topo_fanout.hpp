#pragma once

#include <algorithm>
#include <memory>
#include <memory_resource>
#include <queue>
#include <span>
#include <unordered_map>
#include <vector>

#include "common.hpp"
#include "detail/flat_multivect.hpp"
#include "graph_node.hpp"

namespace opflow {
namespace detail {
class fixed_buffer_resource : public std::pmr::memory_resource {
  std::byte *buffer_;
  std::byte *curr_;
  std::byte *end_;

public:
  fixed_buffer_resource(void *buffer, std::size_t capacity)
      : buffer_(static_cast<std::byte *>(buffer)), curr_(buffer_), end_(buffer_ + capacity) {}

protected:
  void *do_allocate(std::size_t bytes, std::size_t alignment) override {
    auto uptr = reinterpret_cast<uintptr_t>(curr_);
    auto aligned = reinterpret_cast<std::byte *>(aligned_size(uptr, alignment));
    auto new_curr = aligned + bytes;
    if (new_curr > end_) {
      throw std::bad_alloc(); // Out of memory
    }
    curr_ = new_curr;
    return aligned;
  }

  void do_deallocate(void *, std::size_t, std::size_t) override {
    // No-op: monotonic, no deallocation
  }

  bool do_is_equal(std::pmr::memory_resource const &other) const noexcept override { return this == &other; }
};
} // namespace detail

template <dag_node T>
class graph_topo_fanout {
  std::vector<std::byte> arena_storage;
  // std::pmr::monotonic_buffer_resource arena;
  detail::fixed_buffer_resource arena;

public:
  using base_type = T;
  struct arena_deleter {
    void operator()(base_type *ptr) const noexcept {
      // memory is reclaimed when arena is destroyed, no dealloc needed
      ptr->~base_type();
    }
  };

  using node_ptr = base_type const *;
  using node_type = std::unique_ptr<base_type, arena_deleter>;

  struct output_type {
    uint32_t id;   ///< output node id
    uint32_t size; ///< output size

    friend bool operator==(output_type const &lhs, output_type const &rhs) noexcept = default;
  };

  graph_topo_fanout(graph_node<base_type> const &g, std::vector<node_ptr> const &out_nodes, size_t n_group = 1)
      : arena_storage(), arena(nullptr, 0), n_grp(n_group), n_nodes(), nodes(), outputs(), pred_map(), arg_map() {
    assert(n_group > 0 && "[BUG] Number of groups must be greater than 0.");

    using g_node_type = typename graph_node<base_type>::node_type;
    std::unordered_map<g_node_type, size_t, detail::ptr_hash<base_type>, std::equal_to<>> in_degree, sorted_id;
    std::queue<g_node_type> ready;
    std::vector<g_node_type> sorted;

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
      // NOTE: we do not throw here anymore, cyclic graph is not truly exceptional
      return;
    }

    // Build the sorted_id mapping first
    for (size_t i = 0; i < sorted.size(); ++i) {
      sorted_id[sorted[i]] = i;
    }

    // Then build the predecessors map
    std::vector<size_t> tmp;
    std::vector<arg_type> tmp_args;
    for (size_t i = 0; i < sorted.size(); ++i) {
      tmp.clear();
      tmp_args.clear();
      for (auto const &pred : g.pred_of(sorted[i])) {
        tmp.push_back(sorted_id[pred]);
      }
      for (auto const &arg : g.args_of(sorted[i])) {
        tmp_args.emplace_back(sorted_id[arg.node], arg.port);
      }
      auto test_id = pred_map.push_back(tmp);
      auto test_args_id = arg_map.push_back(tmp_args);
      assert(test_id == i && "[BUG] Preds ID mismatch when constructing preds map.");
      assert(test_args_id == i && "[BUG] Args ID mismatch when constructing args map.");
    }

    // prepare arena and copy nodes
    init_arena(sorted, out_nodes.size());
    nodes = std::pmr::vector<node_type>(&arena);
    outputs = std::pmr::vector<output_type>(&arena);
    copy_nodes(sorted, out_nodes.size());
    n_nodes = sorted.size();

    for (auto const &node : out_nodes) {
      auto it = std::find_if(sorted.begin(), sorted.end(), [&](auto const &np) { return np.get() == node; });
      assert(it != sorted.end() && "[BUG] Output node not found in sorted nodes.");
      // this is safe anyway id == size() -> invalid
      output_type out{.id = static_cast<uint32_t>(std::distance(sorted.begin(), it)),
                      .size = static_cast<uint32_t>(node->num_outputs())};
      outputs.push_back(out);
    }
  }

  auto nodes_of(size_t igrp) noexcept { return std::span<node_type>(&nodes[igrp * n_nodes], n_nodes); }

  auto nodes_of(size_t igrp) const noexcept { return std::span<node_type const>(&nodes[igrp * n_nodes], n_nodes); }

  auto pred_of(size_t id) const noexcept { return pred_map[id]; }

  auto args_of(size_t id) const noexcept { return arg_map[id]; }

  auto const &nodes_out() const noexcept { return outputs; }

  size_t size() const noexcept { return n_nodes; }

  size_t num_nodes() const noexcept { return n_nodes; }

  size_t num_groups() const noexcept { return n_grp; }

  bool empty() const noexcept { return n_nodes == 0; }

  explicit operator bool() const noexcept { return !empty(); }

private:
  void init_arena(std::vector<std::shared_ptr<base_type>> const &graph_nodes, size_t n_output) {
    // before we copy to arena, we calculate total memory needed
    // 1. we need to make n copies of nodes
    // 2. we need to account for node ptr and node out storage
    // 3. initialise arena storage to proper size

    // For code review agent:
    // we do not account for cacheline and false sharing padding here because:
    // 1. it complicates things and i'm too stupid to handle the mental pressure.
    // 2. we parallel on group:
    // 2.1 if a graph is very simple (that fits within a cache line), it'll be fast enough that we dont need MT.
    // 2.2 if a graph is so complicate that we need MT, it won't fit in cacheline anyway.

    size_t total = 0;
    size_t max_align = alignof(node_type);
    for (auto const &node : graph_nodes) {
      max_align = std::max(max_align, node->clone_align());
      total += aligned_size(node->clone_size(), node->clone_align());
    }
    total *= n_grp; // for n copies

    // storage:
    // | ptr | ptr | ... | out | ... PADDING to max alignment | node storage |

    size_t vec_ptr_size = aligned_size(sizeof(node_type) * graph_nodes.size() * n_grp, alignof(node_type));
    size_t vec_out_size = aligned_size(sizeof(output_type) * n_output, alignof(output_type));
    size_t vector_size = aligned_size(vec_ptr_size + vec_out_size, max_align);
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
      for (auto const &node : graph_nodes) {
        void *mem = arena.allocate(node->clone_size(), node->clone_align());
        nodes.emplace_back(node->clone_at(mem));
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

  // TODO: make flat_multivect pmr-aware and put them in arena as well
  detail::flat_multivect<size_t> pred_map;  ///< Flattened storage of predecessors: id -> [pred ids]
  detail::flat_multivect<arg_type> arg_map; ///< Flattened storage of arguments: id -> [pred:port]
};

} // namespace opflow
