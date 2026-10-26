#pragma once

#include <Eigen/Core>
#include <limits>
#include <magic_formula/detail/lanes.hpp>

/// Generic lane-wise root finder for an analytic slope (internal, not API; see docs/research/characteristics.md).
/// It holds no tire equations.
namespace mf::detail {
  inline constexpr int STATIONARY_SCAN_CELLS = 32; ///< uniform scan cells on [0, end]; tune with bench
  inline constexpr int STATIONARY_BISECTIONS = 52; ///< bisection steps: bracket width cell/2^52 <= ulp(cell)

  /// The first root of `slope` met when moving from 0 to `end` (lane-wise; `end` may be negative or 0), i.e. the first
  /// stationary point of the function whose analytic derivative `slope` returns.
  ///
  /// Scan s_i = end*(i/N), i = 0..N (N = STATIONARY_SCAN_CELLS). The first cell with
  /// (f_i > 0 && f_{i+1} <= 0) || (f_i < 0 && f_{i+1} >= 0) is the bracket [lo, hi] with f_lo = f_i. Then K steps
  /// (K = STATIONARY_BISECTIONS) halve the bracket on the sign of the slope at the midpoint. The result is the middle
  /// of the final bracket, or NaN where no cell changes sign (also for end = 0 and for NaN slopes).
  ///
  /// The trip counts are fixed (N + 1 + K slope evaluations) and every lane runs every iteration, so the function is
  /// branch-free on data. Consequences: f(0) = 0 followed by a constant sign gives NaN (0 is not a sign); a root
  /// exactly on a grid point is found; the first sign change wins, and two roots inside one cell are not resolved.
  /// The scan and the bisection are bit-exactly symmetric under negation of `end` with a mirrored slope.
  ///
  /// `slope` maps a Lane_T to a Lane_T of the same size and must be noexcept.
  template <typename Lane_T, typename Slope_T>
  [[nodiscard]] Lane_T first_stationary_point(const Slope_T &slope, const Lane_T &end) noexcept {
    const Eigen::Index n = end.size();
    const Lane_T nan_lane = Lane_T::Constant(n, std::numeric_limits<double>::quiet_NaN());
    Lane_T lo = Lane_T::Zero(n);
    Lane_T hi = Lane_T::Zero(n);
    Lane_T f_lo = Lane_T::Zero(n);
    mask_of<Lane_T> found = mask_of<Lane_T>::Constant(n, false);

    Lane_T s_prev = end * 0.0;
    Lane_T f_prev = slope(s_prev);
    for (int i = 0; i < STATIONARY_SCAN_CELLS; ++i) {
      const Lane_T s_next = end * (static_cast<double>(i + 1) / STATIONARY_SCAN_CELLS);
      const Lane_T f_next = slope(s_next);
      const mask_of<Lane_T> falls = (f_prev > 0.0) && (f_next <= 0.0);
      const mask_of<Lane_T> rises = (f_prev < 0.0) && (f_next >= 0.0);
      const mask_of<Lane_T> changes = falls || rises;
      const mask_of<Lane_T> bracket = changes && !found;
      lo = bracket.select(s_prev, lo);
      hi = bracket.select(s_next, hi);
      f_lo = bracket.select(f_prev, f_lo);
      found = found || bracket;
      s_prev = s_next;
      f_prev = f_next;
    }

    for (int k = 0; k < STATIONARY_BISECTIONS; ++k) {
      const Lane_T mid = lo + (0.5 * (hi - lo));
      const Lane_T f_mid = slope(mid);
      const mask_of<Lane_T> same_positive = (f_lo > 0.0) && (f_mid > 0.0);
      const mask_of<Lane_T> same_negative = (f_lo < 0.0) && (f_mid < 0.0);
      const mask_of<Lane_T> same = same_positive || same_negative;
      lo = same.select(mid, lo);
      f_lo = same.select(f_mid, f_lo);
      hi = same.select(hi, mid);
    }

    const Lane_T root = lo + (0.5 * (hi - lo));
    return found.select(root, nan_lane);
  }
} // namespace mf::detail
