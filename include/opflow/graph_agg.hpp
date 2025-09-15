#pragma once

#include "common.hpp"
#include "win_base.hpp"

#include "detail/flat_multivect.hpp"
#include "detail/utils.hpp"

namespace opflow {

// graph to define an aggregation. tho not truly a graph, it is designed to have similar interface to graph_node

template <dag_node_base T>
class graph_agg {
  using data_type = typename T::data_type;

public:
  using node_type = std::shared_ptr<T>;
  using win_type = std::shared_ptr<win_base<data_type>>;

  // Define input column names

  template <range_of<std::string_view> R>
  graph_agg &input(R &&col_names) {
    size_t index = 0;
    for (auto const &name : col_names) {
      col_index.emplace(name, index++);
    }
    return *this;
  }

  template <typename... Ts>
  graph_agg &input(Ts &&...col_names) {
    size_t index = 0;
    (col_index.emplace(std::string(std::forward<Ts>(col_names)), index++), ...);
    return *this;
  }

  // Define window

  template <typename Win, typename... Args>
  graph_agg &window(Args &&...cols_and_args) {
    win_cols.clear();
    add_window_impl<Win>(std::forward<Args>(cols_and_args)...);
    return *this;
  }

  template <template <typename> typename Win, typename... Args>
  graph_agg &window(Args &&...cols_and_args) {
    win_cols.clear();
    add_window_impl<Win<data_type>>(std::forward<Args>(cols_and_args)...);
    return *this;
  }

  // Add aggregation

  template <typename Agg, typename... Args>
  graph_agg &add(Args &&...cols_and_args) {
    std::vector<size_t> selected{};
    add_agg_impl<Agg>(selected, std::forward<Args>(cols_and_args)...);
    return *this;
  }

  template <template <typename> typename Agg, typename... Args>
  graph_agg &add(Args &&...args) {
    std::vector<size_t> selected{};
    add_agg_impl<Agg<data_type>>(selected, std::forward<Args>(args)...);
    return *this;
  }

  // Utilities

  size_t size() const noexcept { return aggs.size(); }

  bool empty() const noexcept { return aggs.empty(); }

  void clear() noexcept {
    win.reset();
    aggs.clear();
    cols.clear();
  }

  auto get_window() const noexcept { return win; }

  auto get_nodes() const noexcept { return std::span(aggs); }

  auto window_input_column() const noexcept { return std::span(win_cols); }

  auto input_column(size_t id) const noexcept { return cols[id]; }

private:
  // add window - cols

  template <typename W, detail::string_like T0, typename... Ts>
  void add_window_impl(T0 &&col, Ts &&...args) {
    auto name = std::string(std::forward<T0>(col));
    auto it = col_index.find(name);
    if (it == col_index.end()) {
      throw std::invalid_argument("Column name '" + name + "' not found in input schema.");
    }
    win_cols.push_back(it->second);
    add_window_impl<W>(std::forward<Ts>(args)...);
  }

  template <typename W, range_idx R, typename... Ts>
  void add_window_impl(R &&colidx, Ts &&...args) {
    win_cols.insert(win_cols.end(), std::ranges::begin(colidx), std::ranges::end(colidx));
    add_window_impl<W>(std::forward<Ts>(args)...);
  }

  template <typename W, range_of<std::string_view> R, typename... Ts>
  void add_window_impl(R &&colnames, Ts &&...args) {
    for (auto const &name : colnames) {
      auto it = col_index.find(name);
      if (it == col_index.end()) {
        throw std::invalid_argument("Column name '" + std::string(name) + "' not found in input schema.");
      }
      win_cols.push_back(it->second);
    }
    add_window_impl<W>(std::forward<Ts>(args)...);
  }

  // add window - ctor

  template <typename W, typename... Ts>
  void add_window_impl(Ts &&...args) {
    win = std::make_shared<W>(std::forward<Ts>(args)...);
  }

  template <typename W, typename... Ts>
  void add_window_impl(ctor_args_tag, Ts &&...args) {
    win = std::make_shared<W>(std::forward<Ts>(args)...);
  }

  // add agg - cols

  template <typename A, detail::string_like T0, typename... Ts>
  void add_agg_impl(std::vector<size_t> &selected, T0 &&col, Ts &&...args) {
    auto name = std::string(std::forward<T0>(col));
    auto it = col_index.find(name);
    if (it == col_index.end()) {
      throw std::invalid_argument("Column name '" + name + "' not found in input schema.");
    }
    selected.push_back(it->second);
    add_agg_impl<A>(selected, std::forward<Ts>(args)...);
  }

  template <typename A, range_idx R, typename... Ts>
  void add_agg_impl(std::vector<size_t> &selected, R &&colidx, Ts &&...args) {
    selected.insert(selected.end(), std::ranges::begin(colidx), std::ranges::end(colidx));
    add_agg_impl<A>(selected, std::forward<Ts>(args)...);
  }

  template <typename A, range_of<std::string_view> R, typename... Ts>
  void add_agg_impl(std::vector<size_t> &selected, R &&colnames, Ts &&...args) {
    for (auto const &name : colnames) {
      auto it = col_index.find(name);
      if (it == col_index.end()) {
        throw std::invalid_argument("Column name '" + std::string(name) + "' notfound in input schema.");
      }
      selected.push_back(it->second);
    }
    add_agg_impl<A>(selected, std::forward<Ts>(args)...);
  }

  // add agg - ctor

  template <typename A, typename... Ts>
  void add_agg_impl(std::vector<size_t> &selected, Ts &&...args) {
    auto agg = std::make_shared<A>(std::forward<Ts>(args)...);
    aggs.push_back(agg);
    cols.push_back(std::move(selected));
  }

  template <typename A, typename... Ts>
  void add_agg_impl(std::vector<size_t> &selected, ctor_args_tag, Ts &&...args) {
    auto agg = std::make_shared<A>(std::forward<Ts>(args)...);
    aggs.push_back(agg);
    cols.push_back(std::move(selected));
  }

  win_type win;
  std::vector<size_t> win_cols;
  std::vector<node_type> aggs;
  detail::flat_multivect<size_t> cols;

  std::unordered_map<std::string, size_t, detail::str_hash, std::equal_to<>> col_index;
};
} // namespace opflow
