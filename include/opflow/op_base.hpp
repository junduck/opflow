#pragma once

#include <cstddef>
#include <tuple>

namespace opflow {

/*
seperating window management from op node sounds cool but it will be a disaster for per op windowing.
*/

enum class window_domain {
  event, ///< Event-based window
  time,  ///< Time-based window
};

struct event_domain_tag {};
struct time_domain_tag {};
constexpr inline event_domain_tag event_domain{};
constexpr inline time_domain_tag time_domain{};

template <typename Data>
struct op_base {
  using data_type = Data;

  /**
   * @brief Update operator state with new data
   *
   * If op node is a root node, output from previous pipeline stage will be passed to in pointer.
   *
   * @param in Pointer to input data.
   */
  virtual void on_data(data_type const *in) noexcept = 0;

  /**
   * @brief Update operator state by removing expired data
   *
   * @param rm Pointer to the data being evicted.
   */
  virtual void on_evict(data_type const *rm) noexcept { std::ignore = rm; }

  /**
   * @brief Write operator's output value to the provided buffer
   *
   * @param out Pointer to the output buffer.
   */
  virtual void value(data_type *out) const noexcept = 0;

  /**
   * @brief Reset operator state.
   *
   */
  virtual void reset() noexcept = 0;

  /**
   * @brief Get the number of input data streams.
   *
   */
  virtual size_t num_inputs() const noexcept = 0;

  /**
   * @brief Get the number of output data streams.
   *
   */
  virtual size_t num_outputs() const noexcept = 0;

  /**
   * @brief Check if the operator is cumulative.
   *
   * This method is called once on pipeline init. If the operator is cumulative, on_evict() won't be called. e.g. EMA,
   * CMA
   */
  virtual bool is_cumulative() const noexcept { return true; } //

  /**
   * @brief Check if the operator has dynamic windowing.
   *
   * This method is called once on pipeline init. If the operator has dynamic windowing, window_size() is called after
   * on_data() to determine size of the window. Overload is chosen based on window domain.
   */
  virtual bool is_dynamic() const noexcept { return false; } // called once on pipeline init

  /**
   * @brief Get the windowing domain of the operator.
   *
   */
  virtual window_domain domain() const noexcept { return window_domain::event; } // called once on pipeline init

  // dynamic: called after on_data()
  // not dynamic: called once on pipeline init

  /**
   * @brief Get the size of dynamic window for event-based windowing.
   *
   * @return number of events (ticks) required
   */
  virtual size_t window_size(event_domain_tag) const noexcept { return 0; }

  /**
   * @brief Get the size of dynamic window for time-based windowing.
   *
   * @return duration required
   */
  virtual data_type window_size(time_domain_tag) const noexcept { return data_type{}; }

  virtual ~op_base() noexcept = default;
};

#if STATIC_COMPOSABLE
namespace detail {
template <typename T, typename U>
concept has_on_evict = requires(T t) {
  { t.on_evict(std::declval<U const *>()) };
};
template <typename T>
concept has_is_cumulative = requires(T t) {
  { t.is_cumulative() };
};
template <typename T>
concept has_is_dynamic = requires(T t) {
  { t.is_dynamic() };
};
template <typename T>
concept has_domain = requires(T t) {
  { t.domain() };
};
template <typename T>
concept has_window_size = requires(T t) {
  { t.window_size(event_domain) };
  { t.window_size(time_domain) };
};
} // namespace detail

template <template <typename> typename Impl, typename Data>
  requires(!std::derived_from<Impl<Data>, op_base<Data>>)
struct op_wrapper : public op_base<Data>, public Impl<Data> {
  using base = op_base<Data>;
  using impl = Impl<Data>;

  using typename base::data_type;

  void on_data(data_type const *in, data_type const *root) noexcept override { impl::on_data(in, root); }
  void value(data_type *out) const noexcept override { impl::value(out); }
  void reset() noexcept override { impl::reset(); }
  size_t num_inputs() const noexcept override { return impl::num_inputs(); }
  size_t num_outputs() const noexcept override { return impl::num_outputs(); }

  void on_evict(data_type const *rm, data_type const *root) noexcept override {
    if constexpr (detail::has_on_evict<impl, data_type>) {
      impl::on_evict(rm, root);
    } else {
      base::on_evict(rm, root); // Default behavior if on_evict is not implemented
    }
  }

  bool is_cumulative() const noexcept override {
    if constexpr (detail::has_is_cumulative<impl>) {
      return impl::is_cumulative();
    } else {
      return base::is_cumulative();
    }
  }

  bool is_dynamic() const noexcept override {
    if constexpr (detail::has_is_dynamic<impl>) {
      return impl::is_dynamic();
    } else {
      return base::is_dynamic();
    }
  }

  window_domain domain() const noexcept override {
    if constexpr (detail::has_domain<impl>) {
      return impl::domain();
    } else {
      return base::domain();
    }
  }

  size_t window_size(event_domain_tag) const noexcept override {
    if constexpr (detail::has_window_size<impl>) {
      return impl::window_size(event_domain);
    } else {
      return base::window_size(event_domain);
    }
  }

  data_type window_size(time_domain_tag) const noexcept override {
    if constexpr (detail::has_window_size<impl>) {
      return impl::window_size(time_domain);
    } else {
      return base::window_size(time_domain);
    }
  }
};
#endif
} // namespace opflow
