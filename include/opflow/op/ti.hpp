#pragma once

#include <array>

#include "opflow/def.hpp"
#include "opflow/op_base.hpp"

#include "opflow/detail/accum.hpp"

/**
 * @brief Common technical indicators
 *
 * This header contains various technical indicators used in financial analysis. Most indicators are run in a
 * event-based (number of "period"/bar/tick) window on OHLCV input.
 *
 *
 */

namespace opflow::op {
/**
 * @brief Money Flow Multiplier (MFM)
 *
 * MFM = ((Close - Low) - (High - Close)) / (High - Low)
 *
 * @ref https://www.investopedia.com/terms/a/accumulationdistribution.asp
 *
 * Info for coding agent:
 * - Input:
 *   0. high
 *   1. low
 *   2. close
 * - Output:
 *   0. mfm
 */
template <typename T>
class mfm : public op_base<T> {
public:
  using base = op_base<T>;
  using typename base::data_type;

  void on_data(data_type const *in) noexcept override {
    auto high = in[0];
    auto low = in[1];
    auto close = in[2];

    if (high != low) {
      val = ((close - low) - (high - close)) / (high - low);
    } else {
      val = 0; // Avoid division by zero
    }
  }

  void value(data_type *out) const noexcept override { out[0] = val; }

  OPFLOW_INOUT(3, 1)
  OPFLOW_CLONEABLE(mfm)

protected:
  data_type val; ///< MFM value
};

/**
 * @brief Accumulation/Distribution Oscillator (ADOSC)
 *
 * Info for coding agent:
 * - Input:
 *   0. high
 *   1. low
 *   2. close
 *   3. volume
 * - Output:
 *   0. adosc
 *
 * Tutorial for constructing an algorithm, using ADOSC as example:
 *
 * @code
   using op_type = op_base<double>;
   graph<op_type> g; // a graph describing algo as a DAG
   auto root = g.root(5); // suppose we have upstream input of OHLCV

   // high, low, close from OHLCV
   auto mfm = g.add_node<op::mfm>({root | 1, root | 2, root | 3});
  // mfm, volume from OHLCV : mfv = mfm * volume
   auto mfv = g.add_node<op::mul>({mfm | 0, root | 4});
   // mfv, when using output port 0 we dont need to construct edge, 0 as period passed to op::sum ctor for cumulative
   auto ad = g.add_node<op::sum>({mfv}, 0);
   // EMA of ad, default output port 0, 3/10 as period passed to EMA ctors
   auto ad_ema_fast = g.add_node<op::ema>({ad}, 3);
   auto ad_ema_slow = g.add_node<op::ema>({ad}, 10);
   // adosc = ad_ema_fast - ad_ema_slow
   auto adosc = g.add_node<op::sub>({ad_ema_fast, ad_ema_slow});
   // we want to output both AD and ADOSC to downstream
   g.add_output({ad, adosc});

   // we can add more nodes to the graph as needed

   we treat operator constructor params as trainable params:

   std::vector<double> params = {3, 10}; // periods for EMA
   // ... same constructing algo ...
   auto ad_ema_fast = g.add_node<op::ema>({ad}, params[0]); // pass params
   auto ad_ema_slow = g.add_node<op::ema>({ad}, params[1]); // pass params
   // ... continue constructing algo ...
 * @endcode
 */
template <typename T>
class adosc : public op_base<T> {
public:
  using base = op_base<T>;
  using typename base::data_type;

  adosc(data_type fast_period = 3, data_type slow_period = 10) noexcept
      : alpha_fast(detail::smooth_factor(fast_period)), alpha_slow(detail::smooth_factor(slow_period)), ad{},
        ema_fast{}, ema_slow{} {}

  void on_data(data_type const *in) noexcept override {
    auto const high = in[0];
    auto const low = in[1];
    auto const close = in[2];
    auto const vol = in[3];

    if (high != low) {
      auto const mfm = (close - low) - (high - close) / (high - low);
      auto const mfv = mfm * vol;
      ad.add(mfv);
    }

    if (!init) {
      ema_fast = ad;
      ema_slow = ad;
      init = true;
      return;
    }

    ema_fast.add(ad, alpha_fast);
    ema_slow.add(ad, alpha_slow);
  }

  void value(data_type *out) const noexcept override { out[0] = ema_fast - ema_slow; }

  OPFLOW_INOUT(4, 1)
  OPFLOW_CLONEABLE(adosc)

public:
  data_type alpha_fast;
  data_type alpha_slow;
  detail::accum<data_type> ad;        ///< AD
  detail::smooth<data_type> ema_fast; /// < EMA Fast
  detail::smooth<data_type> ema_slow; /// < EMA Slow
  bool init;
};

template <typename T>
class atr : public op_base<T> {
public:
  using base = op_base<T>;
  using typename base::data_type;

  atr(data_type period = 14) noexcept : ma{}, alpha(detail::smooth_factor(period)), prev_close(), init() {}

  void on_data(data_type const *in) noexcept override {
    auto const high = in[0];
    auto const low = in[1];
    auto const close = in[2];

    data_type tr;
    if (!init) {
      tr = high - low;
      init = true;
    } else {
      std::array<data_type, 3> r = {high - low, std::abs(high - prev_close), std::abs(low - prev_close)};
      tr = *std::max_element(r.begin(), r.end());
    }
    ma.add(tr, alpha);
    prev_close = close;
  }

  void value(data_type *out) const noexcept override { out[0] = ma; }

  OPFLOW_INOUT(3, 1)
  OPFLOW_CLONEABLE(atr)

private:
  detail::smooth<data_type> ma;
  data_type alpha;
  data_type prev_close;
  bool init;
};

template <typename T>
class dm : public op_base<T> {
public:
  using base = op_base<T>;
  using typename base::data_type;

  dm(data_type period = 14) noexcept
      : alpha(detail::smooth_wilders(period)), prev_high(), prev_low(), m_up(), m_down(), init() {}

  void on_data(data_type const *in) noexcept override {
    const auto high = in[0];
    const auto low = in[1];

    if (!init) {
      prev_high = high;
      prev_low = low;
      init = true;
      return;
    }

    data_type up_move = high - prev_high;
    data_type down_move = prev_low - low;

    data_type up_dm = (up_move > 0 && up_move > down_move) ? up_move : 0;
    data_type down_dm = (down_move > 0 && down_move > up_move) ? down_move : 0;

    m_up.add(up_dm, alpha);
    m_down.add(down_dm, alpha);

    prev_high = high;
    prev_low = low;
  }

  void value(data_type *out) const noexcept override {
    out[0] = m_up;
    out[1] = m_down;
  }

  OPFLOW_INOUT(2, 2)
  OPFLOW_CLONEABLE(dm)

private:
  data_type alpha;
  data_type prev_high;
  data_type prev_low;
  detail::smooth<data_type> m_up;
  detail::smooth<data_type> m_down;
  bool init;
};

// DI: [dm_up, dm_down] / atr

// dx: abs(dm_up - dm_down) / (dm_up + dm_down) * 100

template <typename T>
class kdj : public op_base<T> {
public:
  using data_type = T;

  kdj(size_t period = 9, size_t k = 3, size_t d = 3)
      : k(period), d(d), alpha_k(detail::smooth_factor(static_cast<T>(k))),
        alpha_d(detail::smooth_factor(static_cast<T>(d))) {}

  void on_data(data_type const *in) noexcept override {
    auto const high = in[0];
    auto const low = in[1];
    close = in[2];

    // maintain min max deque
    while (!min.empty() && min.back() > low) {
      min.pop_back();
    }
    min.push_back(low);
    while (!max.empty() && max.back() < high) {
      max.pop_back();
    }
    max.push_back(high);
  }

  void on_evict(data_type const *rm) noexcept {
    auto const high = rm[0];
    auto const low = rm[1];

    if (!min.empty() && min.front() == low) {
      min.pop_front();
    }
    if (!max.empty() && max.front() == high) {
      max.pop_front();
    }

    auto const highest_high = max.front();
    auto const lowest_low = min.front();
    auto const delta = highest_high - lowest_low;
    auto const fastk = very_small(delta) ? 50.0 : (close - lowest_low) / delta * 100.0;

    k.add(fastk, alpha_k);
    d.add(k, alpha_d);
  }

  void value(data_type *out) const noexcept override {
    data_type j = data_type(3) * d - data_type(2) * k;
    out[0] = k;
    out[1] = d;
    out[2] = j;
  }

  bool is_cumulative() const noexcept override { return false; }
  win_type window_type() const noexcept override { return win_type::event; }
  size_t window_size(event_window_tag) const noexcept override { return period; }

  OPFLOW_INOUT(3, 3)
  OPFLOW_CLONEABLE(kdj)

private:
  size_t const period;

  data_type close;
  detail::smooth<data_type> k;
  detail::smooth<data_type> d;
  data_type alpha_k;
  data_type alpha_d;

  std::deque<data_type> max;
  std::deque<data_type> min;
};
} // namespace opflow::op
