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
using duration_t = decltype(std::declval<T>() - std::declval<T>());

// TODO: REMOVE THIS, retention policy design here is logically flawed
// As a sliding window, we should always keep data in the window (window_start, current_time], LEFT OPEN
enum class retention_policy : int {
  cumulative = 0, ///< Cumulative, no data removal
  keep_start,     ///< Data at window start is kept, boundary is calculated as current - window_size
  remove_start    ///< Data at window start is removed
};

// convenience constants

template <std::floating_point T>
constexpr inline T fnan = std::numeric_limits<T>::quiet_NaN(); ///< NaN constant
template <std::floating_point T>
constexpr inline T finf = std::numeric_limits<T>::infinity(); ///< Infinity constant
template <std::floating_point T>
constexpr inline T fmin = std::numeric_limits<T>::min(); ///< Minimum double value
template <std::floating_point T>
constexpr inline T fmax = std::numeric_limits<T>::max(); ///< Maximum double value

template <typename T, typename U = double>
struct op_base {
  using time_type = T;                 ///< Time type used by this operator
  using duration_type = duration_t<T>; ///< Duration type used by this operator
  using data_type = U;                 ///< Data type produced by this operator

  // size_t id; ///< Unique ID for this operator instance
  // void assign_id(size_t new_id) noexcept { id = new_id; }
  // size_t get_id() const noexcept { return id; }

  /**
   * @brief Initialize operator state with input data
   *
   * This method is only called in aggregation context to flush and initialise a new aggregate window
   *
   * @param timestamp Current timestamp
   * @param in Pointer to input data, where in[parent_id] points to the data from parent operator
   */
  virtual void init(time_type timestamp, data_type const *const *in) noexcept {
    std::ignore = timestamp; // Unused in base class
    std::ignore = in;        // Unused in base class
  }

  /**
   * @brief Update operator state with new data
   *
   * @param timestamp Current timestamp
   * @param in Pointer to input data, where in[parent_id] points to the data from parent operator
   */
  virtual void step(time_type timestamp, data_type const *const *in) noexcept = 0;

  /**
   * @brief Update operator state by removing expired data
   *
   * @param expired Timestamp of the expired data
   * @param rm Pointer to removal data, where rm[parent_id] points to the data from parent operator
   */
  virtual void inverse(time_type expired, data_type const *const *rm) noexcept {
    std::ignore = expired; // Unused in base class
    std::ignore = rm;      // Unused in base class
  };

  /**
   * @brief Write operator's output value to the provided buffer
   *
   * @param out Pointer to output buffer where the operator's value will be written
   * @note The output buffer is allocated as reported num_ouputs()
   */
  virtual void value(data_type *out) noexcept = 0;

  /**
   * @brief Get the window start (expiry) for this operator
   *
   * @details When the operator is run in a streaming pipeline, data older than window start is considered expired and
   * will be removed by calls to inverse. This method is only called if the operator has a dynamic window size (when
   * registered, retention policy is not cumulative and window size is T{}). Engine consults this method to determine
   * data expiry and cleanup with inverse. If window_start() returns an earlier time point than previous calls, it is
   * undefined behavior.
   *
   * @return T Window start time point
   */
  virtual time_type window_start() const noexcept { return time_type{}; }

  /**
   * @brief Get the window period for this operator
   *
   * @details When the operator is run in a tumbling pipeline, and the window period is not provided to the pipeline
   * while initialisation, executor will consult this method ONCE to determine the period of the window. If the operator
   * is run in a streaming pipeline, this method is never called.
   *
   * @return size_t
   */
  virtual size_t window_period() const noexcept { return 0; }

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
    requires(detail::range_derived_from<R, op_base>)
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
