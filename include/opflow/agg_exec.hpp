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
        curr_args() {}

  template <range_of<size_t> R>
  void add(node_type agg, R &&colidx) {
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
     * 4. in most cases spec.evict == nrow() or spec.evict == nrow() - 1, so it is even better.
     */
    for (auto &col : df)
      col.erase(col.begin(), col.begin() + static_cast<ptrdiff_t>(spec.evict));
  }

  size_t nrow() const noexcept { return df[0].size(); }
  size_t ncol() const noexcept { return df.size(); }

  data_type *out_ptr(size_t node_id, data_type *out) noexcept { return out + output_offset[node_id]; }

  data_type const *const *in_ptr(size_t node_id, size_t offset) {
    curr_args.clear();
    for (auto i : selected[node_id]) {
      curr_args.push_back(df[i].data() + static_cast<ptrdiff_t>(offset));
    }
    return curr_args.data();
  }

  window_type win;
  std::vector<node_type> nodes;            ///< Aggregators (executed sequentially over the same window)
  detail::flat_multivect<size_t> selected; ///< Input column indices per aggregator

  // TODO: add reserve hint from window api to minimise reallocations
  std::vector<col_type> df; ///< Buffer dataframe for input data

  std::vector<size_t> output_offset; ///< Data offsets for each node
  size_t output_size;                ///< Total output size
  size_t input_size;                 ///< Total input size

  std::vector<data_type const *> curr_args;
};

/**
 * @brief Aggregation builder
 *
 * Instruction for coding agent:
 *
 * 1. This is a fluent interface for creating an aggregation algorithm.
 * 2. The logic is: set input size -> define agg window -> add aggregators -> build executor -> reuse
 *
 * @code
  agg_builder<double> builder;
  builder.input_size(1024)
        .window<win::tumbling<double>>(100)
        .add<agg::ohlc<double>>({0}, extra_ctor_args) // selected cols as input to agg::ohlc
        .add<agg::sum>({2, 5}); // if agg is a template<typename> class, agg<T> is instantiated with T, may look cleaner
  auto exec = builder.build(); // builder will reset and can be used to build again
 * @endcode
 *
 * @tparam T Data type
 */
template <typename T>
class agg_builder {
public:
  agg_builder() noexcept = default;
  agg_builder(size_t input_size) : n(input_size), win(nullptr), aggs(), cols() {}

  agg_builder &input_size(size_t size) {
    n = size;
    return *this;
  }

  template <typename W, typename... Args>
  agg_builder &window(Args &&...args) {
    win = std::make_unique<W>(std::forward<Args>(args)...);
    return *this;
  }

  template <range_of<size_t> R, typename AggType, typename... Args>
  agg_builder &add(R &&colidx, Args &&...args) {
    auto agg = std::make_shared<AggType>(std::forward<Args>(args)...);
    aggs.push_back(std::move(agg));
    cols.emplace_back(colidx.begin(), colidx.end());
    return *this;
  }

  template <typename AggType, typename... Args>
  agg_builder &add(std::initializer_list<size_t> colidx, Args &&...args) {
    auto agg = std::make_shared<AggType>(std::forward<Args>(args)...);
    aggs.push_back(std::move(agg));
    cols.emplace_back(colidx);
    return *this;
  }

  template <template <typename> typename W, typename... Args>
  agg_builder &window(Args &&...args) {
    return window<W<T>>(std::forward<Args>(args)...);
  }

  template <range_of<size_t> R, template <typename> typename Agg, typename... Args>
  agg_builder &add(R &&colidx, Args &&...args) {
    return add<Agg<T>>(std::forward<R>(colidx), std::forward<Args>(args)...);
  }

  template <template <typename> typename Agg, typename... Args>
  agg_builder &add(std::initializer_list<size_t> colidx, Args &&...args) {
    return add<Agg<T>>(colidx, std::forward<Args>(args)...);
  }

  [[nodiscard]] agg_exec<T> build() {
    validate();
    // NOTE: no exception safety guarantees
    agg_exec<T> exec(std::move(win), n);
    for (size_t i = 0; i < aggs.size(); ++i) {
      exec.add(aggs[i], cols[i]);
    }
    reset(); // Reset state for next use
    return std::move(exec);
  }

  void reset() {
    win.reset();
    aggs.clear();
    cols.clear();
  }

private:
  using data_type = T;
  using agg_type = agg_base<data_type>;
  using node_type = std::shared_ptr<agg_type>;
  using window_type = std::unique_ptr<window_base<data_type>>;

  void validate() {
    if (!win)
      throw std::runtime_error("Window is not set");
    if (n == 0)
      throw std::runtime_error("Input size must be > 0");
    for (auto col : cols) {
      if (col.empty())
        throw std::runtime_error("Column index is empty");
      for (auto i : col) {
        if (i >= n)
          throw std::runtime_error("Column index out of bounds");
      }
    }
  }

  size_t n;
  window_type win;
  std::vector<node_type> aggs;
  std::vector<std::vector<size_t>> cols;
};
} // namespace opflow
