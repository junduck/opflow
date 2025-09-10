#pragma once

#include "common.hpp"
#include "window_base.hpp"

#include "detail/flat_multivect.hpp"
#include "detail/utils.hpp"

namespace opflow {

// graph to define an aggregation. tho not truly a graph, it is designed to have similar interface to graph_node

template <dag_node T>
class graph_agg {
  using data_type = typename T::data_type;
  using Hash = detail::ptr_hash<T>;
  using Equal = std::equal_to<>;

public:
  using node_type = std::shared_ptr<T>;
  using window_type = std::shared_ptr<window_base<data_type>>;

  template <typename Win, typename... Args>
  graph_agg &window(Args &&...args) {
    win = std::make_shared<Win>(std::forward<Args>(args)...);
    win_cols.clear();
    return *this;
  }

  template <template <typename> typename Win, typename... Args>
  graph_agg &window(Args &&...args) {
    return window<Win<data_type>>(std::forward<Args>(args)...);
  }

  template <range_of<size_t> R, typename Win, typename... Args>
  graph_agg &window(R &&colidx, Args &&...args) {
    win = std::make_shared<Win>(std::forward<Args>(args)...);
    win_cols.assign(std::ranges::begin(colidx), std::ranges::end(colidx));
    return *this;
  }

  template <range_of<size_t> R, template <typename> typename Win, typename... Args>
  graph_agg &window(R &&colidx, Args &&...args) {
    return window<Win<data_type>>(std::forward<R>(colidx), std::forward<Args>(args)...);
  }

  template <typename Win, typename... Args>
  graph_agg &window(std::initializer_list<size_t> colidx, Args &&...args) {
    win = std::make_shared<Win>(std::forward<Args>(args)...);
    win_cols.assign(colidx.begin(), colidx.end());
    return *this;
  }

  template <template <typename> typename Win, typename... Args>
  graph_agg &window(std::initializer_list<size_t> colidx, Args &&...args) {
    return window<Win<data_type>>(colidx, std::forward<Args>(args)...);
  }

  template <range_of<size_t> R>
  graph_agg &add(node_type agg, R &&colidx) {
    aggs.push_back(agg);
    cols.push_back(std::forward<R>(colidx));
    return *this;
  }

  graph_agg &add(node_type agg, std::initializer_list<size_t> colidx) {
    aggs.push_back(agg);
    cols.push_back(colidx);
    return *this;
  }

  template <range_of<size_t> R, typename AggType, typename... Args>
  graph_agg &add(R &&colidx, Args &&...args) {
    auto agg = std::make_shared<AggType>(std::forward<Args>(args)...);
    aggs.push_back(agg);
    cols.push_back(std::forward<R>(colidx));
    return *this;
  }

  template <typename AggType, typename... Args>
  graph_agg &add(std::initializer_list<size_t> colidx, Args &&...args) {
    auto agg = std::make_shared<AggType>(std::forward<Args>(args)...);
    aggs.push_back(agg);
    cols.push_back(colidx);
    return *this;
  }

  template <range_of<size_t> R, template <typename> typename Agg, typename... Args>
  graph_agg &add(R &&colidx, Args &&...args) {
    return add<Agg<data_type>>(std::forward<R>(colidx), std::forward<Args>(args)...);
  }

  template <template <typename> typename Agg, typename... Args>
  graph_agg &add(std::initializer_list<size_t> colidx, Args &&...args) {
    return add<Agg<data_type>>(colidx, std::forward<Args>(args)...);
  }

  size_t size() const noexcept { return aggs.size(); }

  bool empty() const noexcept { return aggs.empty(); }

  void clear() noexcept {
    win.reset();
    aggs.clear();
    cols.clear();
  }

  auto get_window() const noexcept { return win; }

  auto const &window_input_column() const noexcept { return win_cols; }

  auto get_nodes() const noexcept { return std::span(aggs); }

  auto input_column_of(size_t id) const noexcept { return cols[id]; }

  auto const &get_input_column() const noexcept { return cols; }

private:
  window_type win;
  std::vector<size_t> win_cols;
  std::vector<node_type> aggs;
  detail::flat_multivect<size_t> cols;
};
} // namespace opflow
