#pragma once

#include <chrono>
#include <cstdint>
#include <limits>
#include <ratio>
#include <type_traits>

namespace opflow::chrono {
namespace detail {
template <std::integral T>
constexpr T gcd(T a, T b) noexcept {
  while (b != 0) {
    T t = b;
    b = a % b;
    a = t;
  }
  return a;
}

// Choose a wider intermediate type for mixed multiplications to mitigate overflow.
template <typename T>
struct widen_int {
#ifdef __SIZEOF_INT128__
  using type =
      std::conditional_t<(sizeof(T) < 8), std::int64_t, std::conditional_t<(sizeof(T) < 16), __int128_t, long double>>;
#else
  using type = std::conditional_t<(sizeof(T) < 8), std::int64_t, long double>;
#endif
};
template <typename T>
using widen_int_t = typename widen_int<T>::type;
} // namespace detail

// Type-erased std::ratio
template <std::integral Underlying>
struct ratio {
  using rep = Underlying; ///< Type used to represent the numerator and denominator of the ratio
  rep num;                ///< numerator of the ratio
  rep denom;              ///< denominator of the ratio (must be > 0)

  constexpr ratio(rep n = 1, rep d = 1) noexcept : num(n), denom(d) { normalize(); }

  template <auto Num, auto Denom>
  constexpr ratio(std::ratio<Num, Denom>) noexcept : num(static_cast<rep>(Num)), denom(static_cast<rep>(Denom)) {
    normalize();
  }

  constexpr ratio reduce() const noexcept { return *this; }

  constexpr void normalize() noexcept {
    if (denom < 0) { // keep denominator positive
      denom = -denom;
      num = -num;
    }
    auto g = detail::gcd(num < 0 ? -num : num, denom);
    if (g > 1) {
      num /= g;
      denom /= g;
    }
  }

  constexpr auto operator<=>(ratio const &other) const noexcept = default;
};

using period_type = ratio<int32_t>;                              ///< Default period type for durations
constexpr inline period_type nano = std::ratio<1, 1000000000>{}; ///< 1 nanosecond
constexpr inline period_type micro = std::ratio<1, 1000000>{};   ///< 1 microsecond
constexpr inline period_type milli = std::ratio<1, 1000>{};      ///< 1 millisecond
constexpr inline period_type second = std::ratio<1, 1>{};        ///< 1 second
constexpr inline period_type minute = std::ratio<60, 1>{};       ///< 1 minute
constexpr inline period_type hour = std::ratio<3600, 1>{};       ///< 1 hour
constexpr inline period_type day = std::ratio<86400, 1>{};       ///< 1 day
constexpr inline period_type week = std::ratio<604800, 1>{};     ///< 1 week

// Type-erased duration class
template <typename Underlying>
class duration {
public:
  using rep = Underlying;
  using period = period_type;

private:
  rep n;               ///< The number of tick
  period sec_per_tick; ///< The ratio of seconds per tick

  rep convert_tick(duration const &other) const noexcept {
    // Convert the count of ticks from other duration to this duration's tick count
    if (sec_per_tick == other.sec_per_tick) {
      return other.n;
    }
    using wide = detail::widen_int_t<rep>;
    wide num = static_cast<wide>(other.sec_per_tick.num) * static_cast<wide>(sec_per_tick.denom);
    wide denom = static_cast<wide>(other.sec_per_tick.denom) * static_cast<wide>(sec_per_tick.num);
    auto g = detail::gcd(num < 0 ? -num : num, denom < 0 ? -denom : denom);
    if (g > 1) {
      num /= g;
      denom /= g;
    }
    wide scaled = static_cast<wide>(other.n) * num / denom; // truncation toward zero for integral rep
    return static_cast<rep>(scaled);
  }

public:
  // Constructors
  constexpr duration() noexcept = default;
  constexpr explicit duration(rep const &n, period r = second) noexcept : n(n), sec_per_tick(r) {}

  // Converting constructor from std::chrono::duration
  template <typename Period>
  constexpr duration(const std::chrono::duration<rep, Period> &d) noexcept : sec_per_tick(Period::num, Period::den) {
    n = d.count();
  }

  // Observers
  constexpr rep count() const noexcept { return n; }
  constexpr period get_period() const noexcept { return sec_per_tick; }

  // Arithmetic operators
  constexpr duration operator+() const noexcept { return *this; }
  constexpr duration operator-() const noexcept { return duration(-n, sec_per_tick); }

  constexpr duration &operator++() noexcept {
    ++n;
    return *this;
  }
  constexpr duration operator++(int) noexcept { return duration(n++, sec_per_tick); }
  constexpr duration &operator--() noexcept {
    --n;
    return *this;
  }
  constexpr duration operator--(int) noexcept { return duration(n--, sec_per_tick); }

  constexpr duration &operator+=(duration const &other) noexcept {
    n += convert_tick(other);
    return *this;
  }

  constexpr duration &operator-=(duration const &other) noexcept {
    n -= convert_tick(other);
    return *this;
  }

  constexpr duration &operator*=(rep const &rhs) noexcept {
    n *= rhs;
    return *this;
  }

  constexpr duration &operator/=(rep const &rhs) noexcept {
    n /= rhs;
    return *this;
  }

  constexpr duration &operator%=(rep const &rhs) noexcept {
    n %= rhs;
    return *this;
  }

  constexpr duration &operator%=(duration const &rhs) noexcept {
    n %= convert_tick(rhs);
    return *this;
  }

  // Static member functions
  static constexpr duration zero() noexcept { return duration(rep{0}); }
  static constexpr duration min() noexcept { return duration(std::numeric_limits<rep>::lowest()); }
  static constexpr duration max() noexcept { return duration(std::numeric_limits<rep>::max()); }

  // Comparison operators
  friend constexpr bool operator==(const duration &lhs, const duration &rhs) noexcept {
    if (lhs.sec_per_tick == rhs.sec_per_tick) {
      return lhs.n == rhs.n;
    }
    return lhs.n * lhs.sec_per_tick.num * rhs.sec_per_tick.denom ==
           rhs.n * rhs.sec_per_tick.num * lhs.sec_per_tick.denom;
  }

  friend constexpr bool operator!=(const duration &lhs, const duration &rhs) noexcept { return !(lhs == rhs); }

  friend constexpr bool operator<(const duration &lhs, const duration &rhs) noexcept {
    if (lhs.sec_per_tick == rhs.sec_per_tick) {
      return lhs.n < rhs.n;
    }
    return lhs.n * lhs.sec_per_tick.num * rhs.sec_per_tick.denom <
           rhs.n * rhs.sec_per_tick.num * lhs.sec_per_tick.denom;
  }

  friend constexpr bool operator<=(const duration &lhs, const duration &rhs) noexcept { return !(rhs < lhs); }

  friend constexpr bool operator>(const duration &lhs, const duration &rhs) noexcept { return rhs < lhs; }

  friend constexpr bool operator>=(const duration &lhs, const duration &rhs) noexcept { return !(lhs < rhs); }

  // Binary arithmetic operators
  friend constexpr duration operator+(const duration &lhs, const duration &rhs) noexcept {
    duration result = lhs;
    result += rhs;
    return result;
  }

  friend constexpr duration operator-(const duration &lhs, const duration &rhs) noexcept {
    duration result = lhs;
    result -= rhs;
    return result;
  }

  friend constexpr duration operator*(const duration &lhs, const rep &rhs) noexcept {
    return duration(lhs.n * rhs, lhs.sec_per_tick);
  }

  friend constexpr duration operator*(const rep &lhs, const duration &rhs) noexcept { return rhs * lhs; }

  friend constexpr duration operator/(const duration &lhs, const rep &rhs) noexcept {
    return duration(lhs.n / rhs, lhs.sec_per_tick);
  }

  friend constexpr rep operator/(const duration &lhs, const duration &rhs) noexcept {
    return lhs.n / lhs.convert_tick(rhs);
  }

  friend constexpr duration operator%(const duration &lhs, const rep &rhs) noexcept {
    return duration(lhs.n % rhs, lhs.sec_per_tick);
  }

  friend constexpr duration operator%(const duration &lhs, const duration &rhs) noexcept {
    auto rhs_converted = lhs.convert_tick(rhs);
    return duration(lhs.n % rhs_converted, lhs.sec_per_tick);
  }
};

// Type-erased time point class
template <typename Clock>
class time_point {
public:
  using rep = typename Clock::rep;
  using period = typename Clock::period;
  using duration = opflow::chrono::duration<rep>;
  using clock = Clock;

private:
  duration d; ///< Duration since epoch

public:
  // Constructors
  constexpr time_point() noexcept = default;
  constexpr explicit time_point(const duration &d) noexcept : d(d) {}

  // Converting constructor from std::chrono::time_point
  template <typename StdClock, typename StdDuration>
  constexpr time_point(const std::chrono::time_point<StdClock, StdDuration> &tp) noexcept : d(tp.time_since_epoch()) {}

  // Observers
  constexpr duration time_since_epoch() const noexcept { return d; }

  // Arithmetic operators
  constexpr time_point &operator+=(const duration &rhs) noexcept {
    d += rhs;
    return *this;
  }

  constexpr time_point &operator-=(const duration &rhs) noexcept {
    d -= rhs;
    return *this;
  }

  // Comparison operators
  friend constexpr bool operator==(const time_point &lhs, const time_point &rhs) noexcept { return lhs.d == rhs.d; }

  friend constexpr bool operator!=(const time_point &lhs, const time_point &rhs) noexcept { return !(lhs == rhs); }

  friend constexpr bool operator<(const time_point &lhs, const time_point &rhs) noexcept { return lhs.d < rhs.d; }

  friend constexpr bool operator<=(const time_point &lhs, const time_point &rhs) noexcept { return !(rhs < lhs); }

  friend constexpr bool operator>(const time_point &lhs, const time_point &rhs) noexcept { return rhs < lhs; }

  friend constexpr bool operator>=(const time_point &lhs, const time_point &rhs) noexcept { return !(lhs < rhs); }

  // Binary arithmetic operators
  friend constexpr time_point operator+(const time_point &lhs, const duration &rhs) noexcept {
    return time_point(lhs.d + rhs);
  }

  friend constexpr time_point operator+(const duration &lhs, const time_point &rhs) noexcept { return rhs + lhs; }

  friend constexpr time_point operator-(const time_point &lhs, const duration &rhs) noexcept {
    return time_point(lhs.d - rhs);
  }

  friend constexpr duration operator-(const time_point &lhs, const time_point &rhs) noexcept { return lhs.d - rhs.d; }

  // Static member functions
  static constexpr time_point min() noexcept { return time_point(duration::min()); }
  static constexpr time_point max() noexcept { return time_point(duration::max()); }
};

// Clock concepts and implementations
template <typename Rep>
struct steady_clock {
  using rep = Rep;
  using period = ratio<int32_t>;
  using duration = opflow::chrono::duration<rep>;
  using time_point = opflow::chrono::time_point<steady_clock>;

  static constexpr bool is_steady = true;

  static time_point now() noexcept {
    auto std_now = std::chrono::steady_clock::now();
    auto std_duration = std_now.time_since_epoch();
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(std_duration).count();
    return time_point(duration(static_cast<rep>(nanoseconds), nano));
  }
};

template <typename Rep>
struct system_clock {
  using rep = Rep;
  using period = ratio<int32_t>;
  using duration = opflow::chrono::duration<rep>;
  using time_point = opflow::chrono::time_point<system_clock>;

  static constexpr bool is_steady = false;

  static time_point now() noexcept {
    auto std_now = std::chrono::system_clock::now();
    auto std_duration = std_now.time_since_epoch();
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(std_duration).count();
    return time_point(duration(static_cast<rep>(nanoseconds), nano));
  }
};

template <typename Rep>
struct high_resolution_clock {
  using rep = Rep;
  using period = ratio<int32_t>;
  using duration = opflow::chrono::duration<rep>;
  using time_point = opflow::chrono::time_point<high_resolution_clock>;

  static constexpr bool is_steady = steady_clock<rep>::is_steady;

  static time_point now() noexcept {
    auto std_now = std::chrono::high_resolution_clock::now();
    auto std_duration = std_now.time_since_epoch();
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(std_duration).count();
    return time_point(duration(static_cast<rep>(nanoseconds), nano));
  }
};

// Convenience duration type aliases with default periods
class nanoseconds : public duration<int64_t> {
public:
  constexpr nanoseconds() : duration<int64_t>(0, nano) {}
  constexpr explicit nanoseconds(int64_t count) : duration<int64_t>(count, nano) {}
  constexpr nanoseconds(int64_t count, period p) : duration<int64_t>(count, p) {}
  constexpr nanoseconds(const duration<int64_t> &d) : duration<int64_t>(d) {}
};

class microseconds : public duration<int64_t> {
public:
  constexpr microseconds() : duration<int64_t>(0, micro) {}
  constexpr explicit microseconds(int64_t count) : duration<int64_t>(count, micro) {}
  constexpr microseconds(int64_t count, period p) : duration<int64_t>(count, p) {}
  constexpr microseconds(const duration<int64_t> &d) : duration<int64_t>(d) {}
};

class milliseconds : public duration<int64_t> {
public:
  constexpr milliseconds() : duration<int64_t>(0, milli) {}
  constexpr explicit milliseconds(int64_t count) : duration<int64_t>(count, milli) {}
  constexpr milliseconds(int64_t count, period p) : duration<int64_t>(count, p) {}
  constexpr milliseconds(const duration<int64_t> &d) : duration<int64_t>(d) {}
};

class seconds : public duration<int64_t> {
public:
  constexpr seconds() : duration<int64_t>(0, second) {}
  constexpr explicit seconds(int64_t count) : duration<int64_t>(count, second) {}
  constexpr seconds(int64_t count, period p) : duration<int64_t>(count, p) {}
  constexpr seconds(const duration<int64_t> &d) : duration<int64_t>(d) {}
};

class minutes : public duration<int64_t> {
public:
  constexpr minutes() : duration<int64_t>(0, minute) {}
  constexpr explicit minutes(int64_t count) : duration<int64_t>(count, minute) {}
  constexpr minutes(int64_t count, period p) : duration<int64_t>(count, p) {}
  constexpr minutes(const duration<int64_t> &d) : duration<int64_t>(d) {}
};

class hours : public duration<int64_t> {
public:
  constexpr hours() : duration<int64_t>(0, hour) {}
  constexpr explicit hours(int64_t count) : duration<int64_t>(count, hour) {}
  constexpr hours(int64_t count, period p) : duration<int64_t>(count, p) {}
  constexpr hours(const duration<int64_t> &d) : duration<int64_t>(d) {}
};

class days : public duration<int64_t> {
public:
  constexpr days() : duration<int64_t>(0, day) {}
  constexpr explicit days(int64_t count) : duration<int64_t>(count, day) {}
  constexpr days(int64_t count, period p) : duration<int64_t>(count, p) {}
  constexpr days(const duration<int64_t> &d) : duration<int64_t>(d) {}
};

class weeks : public duration<int64_t> {
public:
  constexpr weeks() : duration<int64_t>(0, week) {}
  constexpr explicit weeks(int64_t count) : duration<int64_t>(count, week) {}
  constexpr weeks(int64_t count, period p) : duration<int64_t>(count, p) {}
  constexpr weeks(const duration<int64_t> &d) : duration<int64_t>(d) {}
};

// R POSIXct, Python datetime compatible (seconds since epoch, double rep)
class POSIXct : public duration<double> {
public:
  constexpr POSIXct() : duration<double>(0, second) {}
  constexpr explicit POSIXct(double count) : duration<double>(count, second) {}
  constexpr POSIXct(double count, period p) : duration<double>(count, p) {}
  constexpr POSIXct(const duration<double> &d) : duration<double>(d) {}
};

namespace literals {
constexpr nanoseconds operator""_ns(unsigned long long ns) { return nanoseconds(static_cast<int64_t>(ns)); }
constexpr microseconds operator""_us(unsigned long long us) { return microseconds(static_cast<int64_t>(us)); }
constexpr milliseconds operator""_ms(unsigned long long ms) { return milliseconds(static_cast<int64_t>(ms)); }
constexpr seconds operator""_s(unsigned long long s) { return seconds(static_cast<int64_t>(s)); }
constexpr minutes operator""_min(unsigned long long min) { return minutes(static_cast<int64_t>(min)); }
constexpr hours operator""_h(unsigned long long h) { return hours(static_cast<int64_t>(h)); }
constexpr days operator""_d(unsigned long long d) { return days(static_cast<int64_t>(d)); }

constexpr POSIXct operator""_us(long double us) { return POSIXct(static_cast<double>(us) / 1'000'000.0); }
constexpr POSIXct operator""_ms(long double ms) { return POSIXct(static_cast<double>(ms) / 1'000.0); }
constexpr POSIXct operator""_s(long double s) { return POSIXct(static_cast<double>(s)); }
constexpr POSIXct operator""_min(long double min) { return POSIXct(static_cast<double>(min) * 60.0); }
constexpr POSIXct operator""_h(long double h) { return POSIXct(static_cast<double>(h) * 3600.0); }
constexpr POSIXct operator""_d(long double d) { return POSIXct(static_cast<double>(d) * 86400.0); }
} // namespace literals

// Duration casting utility
template <typename ToDuration, typename Rep>
constexpr ToDuration duration_cast(const duration<Rep> &d) noexcept {
  using to_rep = typename ToDuration::rep;

  // Get the default period for the target duration type
  // Create a default instance and use its period
  ToDuration default_target{};
  auto target_period = default_target.get_period();

  // Convert to target period
  auto src_period = d.get_period();
  if (src_period.num == target_period.num && src_period.denom == target_period.denom) {
    return ToDuration(d.count(), target_period);
  }

  // Prevent overflow in the conversion calculation
  using wide = detail::widen_int_t<to_rep>;
  wide num = static_cast<wide>(src_period.num) * static_cast<wide>(target_period.denom);
  wide denom = static_cast<wide>(src_period.denom) * static_cast<wide>(target_period.num);
  auto g = detail::gcd(num < 0 ? -num : num, denom < 0 ? -denom : denom);
  if (g > 1) {
    num /= g;
    denom /= g;
  }
  if constexpr (std::floating_point<to_rep>) {
    return ToDuration(static_cast<to_rep>(static_cast<long double>(d.count()) * static_cast<long double>(num) /
                                          static_cast<long double>(denom)),
                      target_period);
  } else {
    wide converted = static_cast<wide>(d.count()) * num / denom;
    return ToDuration(static_cast<to_rep>(converted), target_period);
  }
}

// Time point casting utility
template <typename ToTimePoint, typename Clock>
constexpr ToTimePoint time_point_cast(const time_point<Clock> &tp) noexcept {
  using to_duration = typename ToTimePoint::duration;
  return ToTimePoint(duration_cast<to_duration>(tp.time_since_epoch()));
}

// Common duration operations
template <typename Rep>
constexpr duration<Rep> abs(const duration<Rep> &d) noexcept {
  return d >= duration<Rep>::zero() ? d : -d;
}

// Floor operation for durations
template <typename ToDuration, typename Rep>
constexpr ToDuration floor(const duration<Rep> &d) noexcept {
  auto result = duration_cast<ToDuration>(d);
  if (result > d) {
    return ToDuration(result.count() - 1);
  }
  return result;
}

// Ceiling operation for durations
template <typename ToDuration, typename Rep>
constexpr ToDuration ceil(const duration<Rep> &d) noexcept {
  auto result = duration_cast<ToDuration>(d);
  if (result < d) {
    return ToDuration(result.count() + 1);
  }
  return result;
}

// Round operation for durations
template <typename ToDuration, typename Rep>
constexpr ToDuration round(const duration<Rep> &d) noexcept {
  auto result = duration_cast<ToDuration>(d);
  auto diff1 = abs(d - result);
  auto diff2 = abs(d - (result + ToDuration(1)));

  if (diff1 < diff2) {
    return result;
  } else if (diff1 > diff2) {
    return ToDuration(result.count() + 1);
  } else {
    // Tie-breaking: round to even
    return (result.count() % 2 == 0) ? result : ToDuration(result.count() + 1);
  }
}

} // namespace opflow::chrono
