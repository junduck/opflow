#pragma once

#include "def.hpp"
#include "fn_base.hpp"
#include "tumble_base.hpp"

#include "detail/graph_store.hpp"
#include "detail/vector_store.hpp"

namespace opflow {
template <typename T, typename Alloc = std::allocator<T>>
class tumble_exec {
public:
  using data_type = T;
  using fn_type = fn_base<data_type>;
  using root_type = fn_root<data_type>;
  using tumble_type = tumble_base<data_type>;

  template <typename G>
  tumble_exec(G const &g, size_t num_groups, Alloc const &alloc = Alloc{})
      : // DAG
        ngrp(num_groups), dag(g, ngrp, alloc),
        // data
        history(dag.record_size, ngrp, alloc),
        // tmp
        curr_args(0, ngrp, alloc) {
    size_t max_inputs = 0;
    for (size_t i = 0; i < dag.size(); ++i) {
      max_inputs = std::max(max_inputs, dag.input_offset.size(i));
    }
    curr_args.ensure_group_capacity(max_inputs);
  }

  std::optional<data_type> on_data(data_type timestamp,            //
                                   data_type const *in,            //
                                   data_type *OPFLOW_RESTRICT out, //
                                   size_t igrp) noexcept {
    auto record = history[igrp];
    auto nodes = dag[igrp];
    auto const &win = dag.window(igrp);

    root_type *root = static_cast<root_type *>(nodes[0].get());
    root->on_data(in, out_ptr(record, 0));

    bool should_emit = win->on_data(timestamp, in_ptr(record, 0, igrp));
    if (!should_emit) {
      for (size_t i = 1; i < nodes.size(); ++i) {
        // call node
        nodes[i]->on_data(in_ptr(record, i, igrp), out_ptr(record, i));
      }
      return std::nullopt;
    }

    auto spec = win->emit();
    if (spec.include) {
      // update -> flush -> reset
      for (size_t i = 1; i < nodes.size(); ++i) {
        nodes[i]->on_data(in_ptr(record, i, igrp), out_ptr(record, i));
        nodes[i]->reset();
      }
      flush(out, igrp);
    } else {
      // flush -> reset -> update
      flush(out, igrp);
      for (size_t i = 1; i < nodes.size(); ++i) {
        nodes[i]->reset();
        nodes[i]->on_data(in_ptr(record, i, igrp), out_ptr(record, i));
      }
    }

    return spec.timestamp;
  }

  void flush(data_type *OPFLOW_RESTRICT out, size_t igrp) noexcept {
    auto record = history[igrp];
    for (size_t i = 0; i < dag.size(); ++i) {
      out[i] = record[dag.output_offset[i]];
    }
  }

  size_t num_inputs() const noexcept {
    auto nodes = dag[0];
    root_type const *root = static_cast<root_type const *>(nodes[0].get());
    return root->input_size;
  }

  size_t num_outputs() const noexcept { return dag.output_offset.size(); }

  size_t num_groups() const noexcept { return ngrp; }

private:
  data_type *out_ptr(auto record, size_t node_id) noexcept { return record.data() + dag.record_offset[node_id]; }

  data_type const *in_ptr(auto record, size_t node_id, size_t grp_id) noexcept {
    auto args = curr_args[grp_id];
    auto offsets = dag.input_offset[node_id];
    for (size_t i = 0; i < offsets.size(); ++i) {
      args[i] = record[offsets[i]];
    }
    return args.data();
  }

  size_t const ngrp;
  detail::graph_store<fn_type, tumble_type, Alloc> const dag;
  detail::vector_store<data_type, Alloc> history;

  detail::vector_store<data_type, Alloc> curr_args;
};
} // namespace opflow
