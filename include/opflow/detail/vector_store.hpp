#pragma once

#include <array>
#include <cassert>
#include <memory>
#include <span>
#include <vector>

#include "../common.hpp"

namespace opflow::detail {

/**
 * @brief Cache-line aligned matrix-like storage for preventing false sharing between groups.
 *
 * This storage provides m * n slots of type T, organized as n groups of m elements each.
 * Each group is aligned to cache line boundaries to prevent false sharing when accessed
 * concurrently from different threads.
 *
 * Layout:
 * | group 0 (m elements)    | padding | group 1 (m elements)    | padding | ... | group n-1 (m elements) |
 * |<-- cacheline aligned -->|         |<-- cacheline aligned -->|
 *
 * @tparam T The element type to store
 * @tparam Allocator The allocator type for the underlying storage
 */
template <typename T, typename Allocator = std::allocator<T>>
  requires(std::is_trivial_v<T>) // Requires trivial type so we can default copy/move storage
class vector_store {
public:
  using value_type = T;
  using size_type = std::size_t;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;

private:
  struct alignas(cacheline_size) cacheline_chunk {
    std::array<std::byte, cacheline_size> data;
  };

  // Rebind allocator to work with cacheline_chunk instead of std::byte
  using chunk_allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<cacheline_chunk>;

  std::vector<cacheline_chunk, chunk_allocator_type> storage;
  size_type grp_size;
  size_type grp_num;
  size_type grp_stride; // Bytes between start of consecutive groups

  /**
   * @brief Calculate the stride between groups to ensure cache-line alignment
   *
   * @param group_size Number of elements per group
   * @return Stride in bytes between consecutive groups
   */
  static constexpr size_type calculate_group_stride(size_type group_size) noexcept {
    size_type group_bytes = group_size * sizeof(T);
    return aligned_size(group_bytes, cacheline_size);
  }

  /**
   * @brief Get pointer to the start of a specific group
   *
   * @param grp_id Group identifier (0-based)
   * @return Pointer to the first element of the group
   */
  pointer group_data(size_type grp_id) noexcept {
    assert(grp_id < grp_num && "Group ID out of bounds");
    auto total_byte_offset = grp_id * grp_stride;
    auto chunk_offset = total_byte_offset >> cacheline_shift;
    auto byte_offset = total_byte_offset & cacheline_mask;
    return reinterpret_cast<pointer>(storage[chunk_offset].data.data() + byte_offset);
  }

  /**
   * @brief Get const pointer to the start of a specific group
   *
   * @param grp_id Group identifier (0-based)
   * @return Const pointer to the first element of the group
   */
  const_pointer group_data(size_type grp_id) const noexcept {
    assert(grp_id < grp_num && "Group ID out of bounds");
    auto total_byte_offset = grp_id * grp_stride;
    auto chunk_offset = total_byte_offset >> cacheline_shift;
    auto byte_offset = total_byte_offset & cacheline_mask;
    return reinterpret_cast<const_pointer>(storage[chunk_offset].data.data() + byte_offset);
  }

public:
  /**
   * @brief Construct aligned matrix storage
   *
   * @param group_size Number of elements per group (m)
   * @param num_groups Number of groups (n)
   * @param alloc Allocator instance for the underlying storage
   */
  explicit vector_store(size_type group_size, size_type num_groups, const Allocator &alloc = Allocator{})
      : storage(chunk_allocator_type{alloc}), grp_size(group_size), grp_num(num_groups),
        grp_stride(calculate_group_stride(group_size)) {

    assert(group_size > 0 && "Group size must be greater than 0");
    assert(num_groups > 0 && "Number of groups must be greater than 0");

    // Calculate how many cacheline chunks we need
    size_type total_bytes = grp_num * grp_stride;
    size_type num_chunks = (total_bytes + cacheline_size - 1) / cacheline_size;
    storage.resize(num_chunks);

    // Initialize all elements using placement new
    for (size_type grp = 0; grp < grp_num; ++grp) {
      pointer group_start = group_data(grp);
      for (size_type i = 0; i < grp_size; ++i) {
        new (group_start + i) T{};
      }
    }
  }

  /**
   * @brief Get a span view of a specific group
   *
   * @param grp_id Group identifier (0-based)
   * @return Span providing access to group_size elements
   */
  std::span<T> get(size_type grp_id) noexcept { return {group_data(grp_id), grp_size}; }
  std::span<const T> get(size_type grp_id) const noexcept { return {group_data(grp_id), grp_size}; }

  std::span<T> operator[](size_type grp_id) noexcept { return get(grp_id); }
  std::span<const T> operator[](size_type grp_id) const noexcept { return get(grp_id); }

  /**
   * @brief Get the number of elements per group
   *
   * @return Group size (m)
   */
  size_type group_size() const noexcept { return grp_size; }

  /**
   * @brief Get the number of groups
   *
   * @return Number of groups (n)
   */
  size_type num_groups() const noexcept { return grp_num; }

  /**
   * @brief Get the total number of elements (m * n)
   *
   * @return Total element count
   */
  size_type size() const noexcept { return grp_size * grp_num; }

  /**
   * @brief Get the stride between groups in bytes
   *
   * @return Byte stride between consecutive groups
   */
  size_type group_stride() const noexcept { return grp_stride; }
};

} // namespace opflow::detail
