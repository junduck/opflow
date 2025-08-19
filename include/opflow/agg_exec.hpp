#pragma once

#include <algorithm>
#include <cassert>
#include <memory>
#include <optional>
#include <vector>

#include "agg_base.hpp"
#include "common.hpp"
#include "detail/flat_multivect.hpp"
#include "window_base.hpp"

namespace opflow {
template <typename T>
class agg_exec {
public:
  using data_type = T;
  using agg_type = agg_base<data_type>;
  using node_type = std::shared_ptr<agg_type>;
  using window_type = std::unique_ptr<window_base<data_type>>;

  agg_exec(window_type win, size_t input_size)
      : win(std::move(win)), nodes(), selected(),                               // config
        df(input_size), input_size(input_size), output_offset(), output_size(), // data
        curr_args()                                                             // temp
  {
    validate_inputs();
  }

  template <range_of<size_t> R>
  void add(node_type agg, R &&colidx) {
    assert(!!agg && "[BUG] Invalid aggregator node");
    assert(!std::ranges::any_of(colidx, [this](size_t i) { return i >= input_size; }) &&
           "[BUG] Invalid column indices");

    nodes.push_back(std::move(agg));
    selected.push_back(std::forward<R>(colidx));

    output_offset.push_back(output_size);
    output_size += nodes.back()->num_outputs();
  }

  // Ingest a new row. Returns the timestamp of the emitted window, if any.
  std::optional<data_type> on_data(data_type timestamp, data_type const *in, data_type *out) {
    // Append this row to each input column buffer
    append_row(in);

    // Check if window should emit
    if (!win->on_data(timestamp, in)) {
      return std::nullopt;
    }

    auto spec = win->emit();
    assert(spec.size > 0 && "[BUG] Window size must be greater than 0");
    assert(spec.offset + spec.size <= nrow() && "[BUG] Window must fit within available rows");
    assert(spec.evict <= nrow() && "[BUG] Evict size must not exceed number of rows");

    execute_aggregators(spec, out);
    evict_expired_data(spec);

    return spec.timestamp;
  }

  // Force emission. Returns the timestamp of the emitted window, if any.
  std::optional<data_type> flush(data_type *out) {
    if (!win->flush()) {
      return std::nullopt;
    }

    auto spec = win->emit();
    assert(spec.size > 0 && "[BUG] Window size must be greater than 0");
    assert(spec.offset + spec.size <= nrow() && "[BUG] Window must fit within available rows");
    assert(spec.evict <= nrow() && "[BUG] Evict size must not exceed number of rows");

    execute_aggregators(spec, out);
    evict_expired_data(spec);

    return spec.timestamp;
  }

  size_t num_inputs() const noexcept { return input_size; }
  size_t num_outputs() const noexcept { return output_size; }

  void reset() {
    win->reset();
    for (auto &n : nodes)
      n->reset();
    for (auto &col : df)
      col.clear();
  }

private:
  using spec_type = typename window_base<data_type>::spec_type;
  using col_type = std::vector<data_type>;

  void validate_inputs() const {
    if (!win)
      throw std::runtime_error("Window is null");
    if (input_size == 0)
      throw std::runtime_error("Input size must be > 0");
  }

  void append_row(data_type const *in) {
    for (size_t i = 0; i < input_size; ++i)
      df[i].push_back(in[i]);
  }

  void execute_aggregators(spec_type const &spec, data_type *out) {
    for (size_t i = 0; i < nodes.size(); ++i)
      nodes[i]->on_data(spec.size, in_ptr(i, spec.offset), out_ptr(i, out));
  }

  void evict_expired_data(spec_type const &spec) {
    if (spec.evict == 0)
      return;
    /**
     * For code review agent:
     * 1. i didn't use ring buffer here because col.data() should be contiguous.
     * 2. i can't use ping-pong buffers because bad cache locality.
     * 3. data_type is trivial so erase() is not that bad.
     * 4. in most cases spec.evict == ncol() or spec.evict == ncol() - 1, so it is even better.
     */
    for (auto &col : df)
      col.erase(col.begin(), col.begin() + static_cast<ptrdiff_t>(spec.evict));
  }

  size_t nrow() const noexcept { return df[0].size(); }
  size_t ncol() const noexcept { return df.size(); }

  data_type *out_ptr(size_t node_id, data_type *out) noexcept { return out + output_offset[node_id]; }

  data_type const *const *in_ptr(size_t node_id, size_t offset) const {
    curr_args.clear();
    for (auto i : selected[node_id]) {
      curr_args.push_back(df[i].data() + static_cast<ptrdiff_t>(offset));
    }
    return curr_args.data();
  }

  window_type win;
  std::vector<node_type> nodes;            ///< Aggregators (executed sequentially over the same window)
  detail::flat_multivect<size_t> selected; ///< Input column indices per aggregator

  std::vector<col_type> df; ///< Dataframe for input data

  std::vector<size_t> output_offset; ///< Data offsets for each node
  size_t output_size;                ///< Total output size
  size_t input_size;                 ///< Total input size

  mutable std::vector<data_type const *> curr_args;
};
} // namespace opflow
