#pragma once

#include <concepts>
#include <limits>
#include <ranges>
#include <type_traits>
#include <utility>

namespace opflow {
namespace detail {
template <typename R, typename Base>
concept range_derived_from =
    std::derived_from<std::remove_cvref_t<std::remove_pointer_t<std::ranges::range_value_t<R>>>, Base>;
} // namespace detail

template <typename T>
using time_delta_t = decltype(std::declval<T>() - std::declval<T>());

template <typename T>
concept time_point_like =
    // can compare
    std::regular<T> && std::totally_ordered<T> &&
    // can default construct
    std::is_nothrow_default_constructible_v<T> &&
    // can be constructed directly from time delta (epoch + delta)
    std::is_nothrow_constructible_v<T, time_delta_t<T>> &&
    // basic arithmetic operations
    requires(T a, time_delta_t<T> b) {
      { a - b } -> std::convertible_to<T>;
      { a + b } -> std::convertible_to<T>;
    };

/// \note an operator should never worry about the time or the current window, unless it has dynamic window size

enum class retention_policy : int {
  cumulative = 0, ///< Cumulative, no data removal
  keep_start,     ///< Data at window start is kept, boundary is calculated as current - window_size
  remove_start    ///< Data at window start is removed
};

// convenience constants
constexpr inline double fnan = std::numeric_limits<double>::quiet_NaN(); ///< NaN constant
constexpr inline double finf = std::numeric_limits<double>::infinity();  ///< Infinity constant
constexpr inline double fmin = std::numeric_limits<double>::min();       ///< Minimum double value
constexpr inline double fmax = std::numeric_limits<double>::max();       ///< Maximum double value

template <time_point_like T>
struct op_base {
  /**
   * @brief Initialize operator state with input data
   *
   * This method is only called in aggregation context to flush and initialise a new aggregate window
   *
   * @param tick Current tick
   * @param in Pointer to input data, where in[parent_id] points to the data from parent operator
   */
  virtual void init(T tick, double const *const *in) noexcept {
    std::ignore = tick; // Unused in base class
    std::ignore = in;   // Unused in base class
  }

  /**
   * @brief Update operator state with new data
   *
   * @param tick Current tick
   * @param in Pointer to input data, where in[parent_id] points to the data from parent operator
   */
  virtual void step(T tick, double const *const *in) noexcept = 0;

  /**
   * @brief Update operator state by removing expired data
   *
   * @param tick Expired tick
   * @param rm Pointer to removal data, where rm[parent_id] points to the data from parent operator
   */
  virtual void inverse(T tick, double const *const *rm) noexcept {
    std::ignore = tick; // Unused in base class
    std::ignore = rm;   // Unused in base class
  };

  /**
   * @brief Write operator's output value to the provided buffer
   *
   * @param out Pointer to output buffer where the operator's value will be written
   * @note The output buffer is allocated as reported num_ouputs()
   */
  virtual void value(double *out) noexcept = 0;

  /**
   * @brief Get the window start (expiry) for this operator
   *
   * @details Data older than window start is considered expired and will be removed by calls to inverse.
   * This method is only called if the operator has a dynamic window size (when registered, retention policy is not
   * cumulative and window size is T{}). Engine consults this method to determine data expiry and cleanup with inverse.
   * If window_start() returns an earlier time point than previous calls, it is undefined behavior.
   *
   * @return T Window start time point
   */
  virtual T window_start() const noexcept { return T{}; }

  /**
   * @brief Returns number of dependencies/parents.
   *
   * @return size_t
   */
  virtual size_t num_depends() const noexcept = 0;

  /**
   * @brief Query number of inputs required from a predecessor operator.
   *
   * @details num_inputs returns the number of inputs this operator expects from the predecessor operator with id pid.
   *
   * @param pid Predecessor id
   * @return size_t
   */
  virtual size_t num_inputs(size_t pid) const noexcept = 0;

  /**
   * @brief Returns number of outputs this operator produces.
   *
   * @return size_t
   */
  virtual size_t num_outputs() const noexcept = 0;

  virtual ~op_base() = default;

  template <std::ranges::forward_range R>
  bool compatible_with(R &&deps) const noexcept
    requires detail::range_derived_from<R, op_base>
  {
    // Check if the number of dependencies matches the expected count
    if (!(num_depends() == std::ranges::size(deps))) {
      return false;
    }
    // Check if each dependency's input count can satisfy the expected input count
    if constexpr (std::is_pointer_v<std::ranges::range_value_t<R>>) {
      return std::ranges::equal(std::views::iota(size_t{0}, num_depends()), deps,
                                [this](size_t pid, auto const &dep) { return num_inputs(pid) <= dep->num_outputs(); });
    } else {
      return std::ranges::equal(std::views::iota(size_t{0}, num_depends()), deps,
                                [this](size_t pid, auto const &dep) { return num_inputs(pid) <= dep.num_outputs(); });
    }
  }
};
} // namespace opflow
