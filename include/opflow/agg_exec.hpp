#pragma once

#include <algorithm>
#include <cassert>
#include <memory>
#include <vector>

#include "agg_base.hpp"
#include "common.hpp"
#include "def.hpp"
#include "detail/flat_multivect.hpp"
#include "window_base.hpp"

namespace opflow {
template <typename T>
class agg_exec {
public:
  using data_type = T;
  using agg_type = agg_base<data_type>;
  using node_type = std::shared_ptr<agg_type>;

  agg_exec(auto win, size_t input_size)
      : win(win), nodes(), cols_idx(), cols(), data_offset(), data_size(0), mem(),
        last_emitted{min_time<data_type>(), 0, 0}, input_size(input_size), curr_args() {
    cols.resize(input_size);
  }

  template <range_of<size_t> R>
  void add(node_type agg, R &&colidx) {
    nodes.push_back(std::move(agg));
    size_t const idx = cols_idx.push_back(std::forward<R>(colidx));
    for (auto i : cols_idx[idx]) {
      assert(i < input_size && "Aggregator column index out of bounds");
    }
    assert(nodes.back()->num_inputs() == cols_idx.size(idx) && "Aggregator input arity mismatch with column indices");
    data_offset.push_back(data_size);
    data_size += agg->num_outputs();
    mem.resize(data_size);
  }

  // Ingest a new row. Returns true if a window was emitted and results are available via value().
  bool on_data(data_type timestamp, data_type const *in) {
    // window processes full input row for potential inspection
    bool should_emit = win->process(timestamp, in);
    // append this row to each input column buffer
    for (size_t i = 0; i < input_size; ++i) {
      cols[i].push_back(in[i]);
    }
    if (!should_emit) {
      return false;
    }
    auto spec = win->emit();

    // Compute window start (all columns share same logical length)
    assert(spec.size > 0 && "[BUG] Window size must be > 0");
    assert(cols.size() == input_size && "[BUG] Input columns container not initialised");
    assert(cols.empty() || cols[0].size() >= spec.size);
    size_t const win_start = cols.empty() ? 0 : (cols[0].size() - spec.size);

    // Run all aggregators over the current window
    for (size_t i = 0; i < nodes.size(); ++i) {
      nodes[i]->on_data(spec.size, in_ptr(i, win_start), out_ptr(i));
    }

    // Record last emitted
    last_emitted = spec;

    // Evict rows from the front of each input column buffer
    if (spec.evict > 0) {
      for (auto &col : cols) {
        assert(col.size() >= spec.evict && "[BUG] Attempting to evict more rows than available");
        col.erase(col.begin(), col.begin() + static_cast<typename col_type::difference_type>(spec.evict));
      }
    }

    return true;
  }

  // Force emission if the window indicates one is available. Returns true if a window was emitted.
  bool flush() {
    if (!win->flush())
      return false;
    auto spec = win->emit();

    assert(spec.size > 0 && "[BUG] Window size must be > 0");
    assert(cols.size() == input_size && "[BUG] Input columns container not initialised");
    assert(cols.empty() || cols[0].size() >= spec.size);
    size_t const win_start = cols.empty() ? 0 : (cols[0].size() - spec.size);

    for (size_t i = 0; i < nodes.size(); ++i) {
      nodes[i]->on_data(spec.size, in_ptr(i, win_start), out_ptr(i));
    }
    last_emitted = spec;

    if (spec.evict > 0) {
      for (auto &col : cols) {
        assert(col.size() >= spec.evict && "[BUG] Attempting to evict more rows than available");
        col.erase(col.begin(), col.begin() + static_cast<typename col_type::difference_type>(spec.evict));
      }
    }
    return true;
  }

  data_type value(data_type *OPFLOW_RESTRICT out) const noexcept {
    for (size_t i = 0; i < data_size; ++i) {
      out[i] = mem[i];
    }
    return last_emitted.timestamp;
  }
  size_t num_inputs() const noexcept { return input_size; }
  size_t num_outputs() const noexcept { return data_size; }

  void reset() {
    // Reset window and aggregators
    if (win)
      win->reset();
    for (auto &n : nodes) {
      if (n)
        n->reset();
    }
    // Clear buffered inputs
    for (auto &c : cols)
      c.clear();
    last_emitted = spec_type{min_time<data_type>(), 0, 0};
  }

private:
  using spec_type = typename window_base<data_type>::spec_type;
  using window_type = std::shared_ptr<window_base<data_type>>;
  using col_type = std::vector<data_type>;

  data_type *out_ptr(size_t node_id) noexcept { return mem.data() + data_offset[node_id]; }

  data_type const *const *in_ptr(size_t node_id, size_t win_start) const {
    curr_args.clear();
    for (auto i : cols_idx[node_id]) {
      assert(i < cols.size());
      assert(cols[i].size() >= win_start);
      curr_args.push_back(cols[i].data() + static_cast<typename col_type::difference_type>(win_start));
    }
    return curr_args.data();
  }

  window_type win;
  std::vector<node_type> nodes;            ///< Aggregators (executed sequentially over the same window)
  detail::flat_multivect<size_t> cols_idx; ///< Input column indices per aggregator

  std::vector<col_type> cols; ///< Column buffers for input data

  std::vector<size_t> data_offset; ///< Data offsets for each node
  size_t data_size;
  std::vector<data_type> mem; ///< Memory buffer for results

  spec_type last_emitted;

  size_t input_size;

  mutable std::vector<data_type const *> curr_args;
};
} // namespace opflow
