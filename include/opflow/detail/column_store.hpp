#pragma once

#include <cassert>
#include <memory>
#include <span>
#include <vector>

#include "utils.hpp"

namespace opflow::detail {
template <trivial T, typename Alloc = std::allocator<T>>
class column_store {
  /**
   * Layout:
   * | col 0 (m elements) | remaining space | col 1 (m elements) | remaining space | ... | col n-1 (m elements) | ... |
   */
public:
  // Standard allocator-aware type definitions
  using allocator_type = Alloc;
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = T &;
  using const_reference = T const &;
  using pointer = typename std::allocator_traits<Alloc>::pointer;
  using const_pointer = typename std::allocator_traits<Alloc>::const_pointer;

  /**
   * @brief Construct column store with specified number of columns
   *
   * @param num_columns Number of columns (fixed for lifetime of object)
   * @param initial_capacity Initial capacity per column
   * @param alloc Allocator instance
   */
  explicit column_store(size_t num_columns, size_t initial_capacity = 0, const Alloc &alloc = Alloc{})
      : n_col(num_columns), storage(alloc), col_cap(initial_capacity), n_row(0) {
    assert(num_columns > 0 && "Number of columns must be greater than 0");
    if (initial_capacity > 0) {
      storage.resize(n_col * initial_capacity);
    }
  }

  /**
   * @brief Construct column store with allocator only
   *
   * @param num_columns Number of columns (fixed for lifetime of object)
   * @param alloc Allocator instance
   */
  explicit column_store(size_t num_columns, const Alloc &alloc) : column_store(num_columns, 0, alloc) {}

  /**
   * @brief Copy constructor with allocator
   */
  column_store(const column_store &other, const Alloc &alloc)
      : n_col(other.n_col), storage(other.storage, alloc), col_cap(other.col_cap), n_row(other.n_row) {}

  /**
   * @brief Get the allocator instance
   */
  allocator_type get_allocator() const noexcept { return storage.get_allocator(); }

  /**
   * @brief Get span access to a specific column
   *
   * @param col_id Column identifier (0-based)
   * @return Span providing access to all rows in the column
   */
  std::span<T> operator[](size_t col_id) noexcept { return {storage.data() + col_id * col_cap, n_row}; }

  std::span<T const> operator[](size_t col_id) const noexcept { return {storage.data() + col_id * col_cap, n_row}; }

  /**
   * @brief Append a row (one element per column)
   *
   * @param row Span containing exactly ncol() elements
   */
  void append(std::span<T const> row) {
    assert(row.size() == n_col && "[BUG] Row size does not match number of columns.");

    if (n_row >= col_cap) {
      ensure_column_capacity(col_cap == 0 ? 1 : col_cap * 2);
    }

    // Copy each element to its respective column
    for (size_t col = 0; col < n_col; ++col) {
      storage[idx(col, n_row)] = row[col];
    }

    ++n_row;
  }

  void append(T const *row) { append(std::span<T const>(row, n_col)); }

  /**
   * @brief Evict rows from the column store
   *
   * @param n Number of rows to evict
   */
  void evict(size_t n) {
    assert(n <= n_row && "[BUG] Cannot evict more rows than currently stored.");
    for (size_t col = 0; col < n_col; ++col) {
      for (size_t row = n; row < n_row; ++row) {
        storage[idx(col, row - n)] = storage[idx(col, row)];
      }
    }
    n_row -= n;
  }

  /**
   * @brief Get the number of columns
   *
   * @return Number of columns
   */
  size_t ncol() const noexcept { return n_col; }

  /**
   * @brief Get the current number of rows
   *
   * @return Number of rows
   */
  size_t nrow() const noexcept { return n_row; }

  /**
   * @brief Get the current capacity per column
   *
   * @return Column capacity
   */
  size_t column_capacity() const noexcept { return col_cap; }

  /**
   * @brief Get the total number of elements
   *
   * @return Total element count (ncol() * nrow())
   */
  size_t size() const noexcept { return n_col * n_row; }

  /**
   * @brief Check if the store is empty
   *
   * @return True if no rows have been added
   */
  bool empty() const noexcept { return n_row == 0; }

  /**
   * @brief Clear all data (reset to empty state)
   */
  void clear() noexcept { n_row = 0; }

  /**
   * @brief Reserve capacity for at least the specified number of rows per column
   *
   * @param new_capacity Minimum capacity to reserve per column
   */
  void reserve(size_t new_capacity) {
    if (new_capacity > col_cap) {
      ensure_column_capacity(new_capacity);
    }
  }

  /**
   * @brief Get a specific element by column and row
   *
   * @param col_id Column identifier
   * @param row_id Row identifier
   * @return Reference to the element
   */
  T &at(size_t col_id, size_t row_id) noexcept { return storage[idx(col_id, row_id)]; }
  const T &at(size_t col_id, size_t row_id) const noexcept { return storage[idx(col_id, row_id)]; }

private:
  size_t const n_col; // determined on ctor cannot change
  std::vector<T, Alloc> storage;
  size_t col_cap; // current capacity per column
  size_t n_row;   // current number of rows

  void ensure_column_capacity(size_t new_cap) {
    if (new_cap <= col_cap) {
      return; // No change needed
    }

    std::vector<T, Alloc> new_storage(storage.get_allocator());
    new_storage.resize(n_col * new_cap);

    // Copy existing data to new layout
    for (size_t col = 0; col < n_col; ++col) {
      for (size_t row = 0; row < n_row; ++row) {
        new_storage[col * new_cap + row] = storage[col * col_cap + row];
      }
    }

    storage = std::move(new_storage);
    col_cap = new_cap;
  }

  size_t idx(size_t col_id, size_t row_id) const noexcept { return col_id * col_cap + row_id; }
};
} // namespace opflow::detail
