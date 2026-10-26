#pragma once

#include <Eigen/Core>
#include <cmath>

/// Lane types and branch-free helpers shared by the MF 5.x kernels (internal, not API). A lane is a column of
/// per-point values: `point_lane` holds one point, `block_lane` up to `BLOCK_ROWS` points without heap allocation.
namespace mf::detail {
  /// Rows of one batch block.
  inline constexpr Eigen::Index BLOCK_ROWS = 32; ///< batch block; stack working set ~20 KB; tune with bench

  /// One point: scalar code, std:: math.
  using point_lane = Eigen::Array<double, 1, 1>;

  /// At most BLOCK_ROWS points, no heap.
  using block_lane = Eigen::Array<double, Eigen::Dynamic, 1, Eigen::ColMajor, BLOCK_ROWS, 1>;

  /// Boolean mask with the shape of `Lane_T`.
  template <typename Lane_T>
  using mask_of = Eigen::Array<bool, Lane_T::RowsAtCompileTime, 1, Eigen::ColMajor, Lane_T::MaxRowsAtCompileTime, 1>;

  /// Guard added to denominators that can reach zero.
  inline constexpr double EPSILON = 1e-6;

  /// mask ? if_true : if_false, element-wise: the one select primitive of kernels that must also run on dual lanes.
  /// Call form: pass named concrete lanes, never `Lane_T::Constant(...)` expressions, so that `Lane_T` is deduced; an
  /// unqualified call finds the `dual_lane` overload by argument-dependent lookup.
  template <typename Mask_T, typename Lane_T>
  [[nodiscard]] Lane_T select_lanes(const Mask_T &mask, const Lane_T &if_true, const Lane_T &if_false) noexcept {
    return mask.select(if_true, if_false);
  }

  /// sgn+ : +1 for x >= 0 (including +0.0 and -0.0), -1 otherwise.
  template <typename Lane_T> [[nodiscard]] Lane_T sgn_plus(const Lane_T &x) noexcept {
    return (x >= 0.0).select(Lane_T::Constant(x.size(), 1.0), Lane_T::Constant(x.size(), -1.0));
  }

  /// d + EPSILON*sgn+(d): moves a denominator away from zero without changing its sign.
  template <typename Lane_T> [[nodiscard]] Lane_T add_epsilon(const Lane_T &d) noexcept {
    const Lane_T guarded = d + (EPSILON * sgn_plus(d));
    return guarded;
  }

  /// Limits a curvature factor to E <= 1.
  template <typename Lane_T> [[nodiscard]] Lane_T limit_curvature(const Lane_T &e) noexcept {
    return e.min(1.0);
  }

  /// std::hypot of every element pair.
  template <typename Lane_T> [[nodiscard]] Lane_T elementwise_hypot(const Lane_T &x, const Lane_T &y) noexcept {
    return x.binaryExpr(y, [](double a, double b) { return std::hypot(a, b); });
  }

  /// 1.0 where `mask` is true, 0.0 where it is false.
  template <typename Lane_T> [[nodiscard]] Lane_T to_flag(const mask_of<Lane_T> &mask) noexcept {
    return mask.template cast<double>();
  }

  /// (x < lower).select(lower, (x > upper).select(upper, x)): the scalar clamp, keeping -0.0 and passing NaN through.
  /// `moved` is 1.0 where the value was clamped, 0.0 elsewhere. Infinite bounds mean no range and never flag.
  template <typename Lane_T>
  [[nodiscard]] Lane_T clamp_to_limits(const Lane_T &x, double lower, double upper, Lane_T &moved) noexcept {
    const mask_of<Lane_T> below = x < lower;
    const mask_of<Lane_T> above = x > upper;
    const mask_of<Lane_T> either = below || above;
    moved = to_flag<Lane_T>(either);
    return below.select(Lane_T::Constant(x.size(), lower), above.select(Lane_T::Constant(x.size(), upper), x));
  }
} // namespace mf::detail
