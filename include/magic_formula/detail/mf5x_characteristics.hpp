#pragma once

#include <Eigen/Core>
#include <cmath>
#include <limits>
#include <magic_formula/detail/dual_lane.hpp>
#include <magic_formula/detail/lanes.hpp>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/detail/stationary_point.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_forms.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <numbers>

/// The MF 5.x tire characteristics (internal, not API): the slopes dFy/dSA and dFx/dSR as exact forward-mode
/// derivatives of the closed-form force kernels (`dual_lane`, no step size), the secant stiffnesses, the peak factors
/// Dy and Dx, and the first stationary points of Fy(SA) and Fx(SR). All functions are total, never throw and never
/// allocate. NaN is a value only in the two peak-slip channels. Equations and policies:
/// docs/research/characteristics.md.
namespace mf::detail {
  inline constexpr double PEAK_SEARCH_ALPHA_RAD = std::numbers::pi / 2.0; ///< default |SA| search extent (policy)
  inline constexpr double PEAK_SEARCH_KAPPA = 1.0;                        ///< default |SR| search extent (policy)

  /// The planar forces of one operating point together with the pure-slip terms they come from.
  template <typename Lane_T> struct planar_forces {
    /// Vertical-load terms.
    load_terms<Lane_T> load;
    /// Pure longitudinal terms (Fx0, Dx, ...).
    longitudinal_terms<Lane_T> lon;
    /// Pure lateral terms (Fy0, Dy, ...).
    lateral_terms<Lane_T> lat;
    /// Fx of the slip mode [N].
    Lane_T fx_n;
    /// Fy of the slip mode [N].
    Lane_T fy_n;
  };

  /// The slip state and the forces of `p` (validated and clamped): Fx0 and Fy0 for `UNCOMBINED`, `combine` for
  /// `COMBINED`. The same functions with the same arguments as `mf5x_evaluate`, so on plain lanes the forces are
  /// bit-identical to its Fx and Fy; no aligning kernel runs, so it is also valid on `dual_lane`.
  template <typename Lane_T>
  [[nodiscard]] planar_forces<Lane_T> mf5x_planar_forces(const mf5x_coefficients &c, combined_method method,
                                                         slip_mode mode, const input_lanes<Lane_T> &p) noexcept {
    slip_state<Lane_T> state{};
    state.load = vertical_load(c, p.fz_n);
    state.lon = fx0(c, state.load, p.fz_n, p.kappa, p.gamma_rad);
    state.lat = fy0(c, state.load, p.fz_n, p.alpha_rad, p.gamma_rad);
    planar_forces<Lane_T> out{
        .load = state.load, .lon = state.lon, .lat = state.lat, .fx_n = state.lon.fx0_n, .fy_n = state.lat.fy0_n};
    if (mode == slip_mode::COMBINED) {
      const combined_forces<Lane_T> forces = combine(c, method, state, p);
      out.fx_n = forces.fx_n;
      out.fy_n = forces.fy_n;
    }
    return out;
  }

  /// `p` as dual lanes with the slip angle `alpha_rad` seeded with `seed` (d alpha/ds = seed) and every other input
  /// constant (derivative 0).
  template <typename Lane_T>
  [[nodiscard]] input_lanes<dual_lane<Lane_T>> seed_alpha(const input_lanes<Lane_T> &p, const Lane_T &alpha_rad,
                                                          const Lane_T &seed) noexcept {
    const Lane_T zero = Lane_T::Zero(p.fz_n.size());
    return input_lanes<dual_lane<Lane_T>>{.fz_n = make_dual(p.fz_n, zero),
                                          .alpha_rad = make_dual(alpha_rad, seed),
                                          .kappa = make_dual(p.kappa, zero),
                                          .gamma_rad = make_dual(p.gamma_rad, zero),
                                          .vx_mps = make_dual(p.vx_mps, zero)};
  }

  /// `p` as dual lanes with the slip ratio `kappa` seeded with `seed` (d kappa/ds = seed) and every other input
  /// constant (derivative 0).
  template <typename Lane_T>
  [[nodiscard]] input_lanes<dual_lane<Lane_T>> seed_kappa(const input_lanes<Lane_T> &p, const Lane_T &kappa,
                                                          const Lane_T &seed) noexcept {
    const Lane_T zero = Lane_T::Zero(p.fz_n.size());
    return input_lanes<dual_lane<Lane_T>>{.fz_n = make_dual(p.fz_n, zero),
                                          .alpha_rad = make_dual(p.alpha_rad, zero),
                                          .kappa = make_dual(kappa, seed),
                                          .gamma_rad = make_dual(p.gamma_rad, zero),
                                          .vx_mps = make_dual(p.vx_mps, zero)};
  }

  /// End of the peak search segment, side*min(max(side*bound, 0), extent), where `side` is +1 or -1 and `bound` is
  /// `upper` for side +1 and `lower` for side -1. An infinite bound gives side*extent; a bound on the other side of 0
  /// gives +-0.
  template <typename Lane_T>
  [[nodiscard]] Lane_T peak_search_end(const Lane_T &side, double lower, double upper, double extent) noexcept {
    const Eigen::Index n = side.size();
    const Lane_T upper_lane = Lane_T::Constant(n, upper);
    const Lane_T lower_lane = Lane_T::Constant(n, lower);
    const Lane_T bound = (side > 0.0).select(upper_lane, lower_lane);
    const Lane_T reach = (side * bound).max(0.0).min(extent);
    return side * reach;
  }

  /// The eight characteristics of one point per lane element.
  template <typename Lane_T> struct characteristic_lanes {
    /// dFy/dSA of the evaluated force [N/rad].
    Lane_T dfy_dsa_n_per_rad;
    /// dFx/dSR of the evaluated force [N].
    Lane_T dfx_dsr_n;
    /// Fy/SA with the raw SA; the tangent slope at SA = +-0 [N/rad].
    Lane_T fy_over_sa_n_per_rad;
    /// Fx/SR with the raw SR; the tangent slope at SR = +-0 [N].
    Lane_T fx_over_sr_n;
    /// Peak lateral factor Dy [N].
    Lane_T dy_n;
    /// Peak longitudinal factor Dx [N].
    Lane_T dx_n;
    /// First stationary point of Fy(SA) from 0 on the side of the row's SA; NaN when there is none [rad].
    Lane_T sa_at_peak_fy_rad;
    /// First stationary point of Fx(SR) from 0 on the side of the row's SR; NaN when there is none [-].
    Lane_T sr_at_peak_fx;
  };

  /// The characteristics of the raw inputs `raw`. The inputs are conditioned like the evaluation (`mf5x_condition`);
  /// a row without contact gives 0 in the six non-peak channels and NaN in the two peak channels. The slopes are the
  /// exact derivatives of the evaluated force at the conditioned point (0 for a slip that was clamped); a secant is
  /// the evaluated force over the raw slip, and the row's own slope where the raw slip is +-0. The peak slips are the
  /// first stationary points of the analytic slope on [0, end], with end the clipped `PEAK_SEARCH_*` extent on the
  /// side of the row's slip, found by a fixed scan and a fixed bisection.
  template <typename Lane_T>
  [[nodiscard]] characteristic_lanes<Lane_T>
  mf5x_characteristics(const mf5x_coefficients &c, const mf5x_forms &forms, slip_mode mode,
                       const Eigen::Ref<const Eigen::VectorXd> &limits, const input_lanes<Lane_T> &raw) noexcept {
    const conditioned_lanes<Lane_T> cond = mf5x_condition<Lane_T>(limits, raw, std::copysign(1.0, c.FNOMIN));
    const input_lanes<Lane_T> &point = cond.point;
    const Eigen::Index n = raw.fz_n.size();
    const Lane_T zero = Lane_T::Zero(n);
    const Lane_T one = Lane_T::Constant(n, 1.0);
    const Lane_T nan_lane = Lane_T::Constant(n, std::numeric_limits<double>::quiet_NaN());

    const planar_forces<Lane_T> at = mf5x_planar_forces(c, forms.combined, mode, point);
    const Lane_T alpha_seed = one - cond.alpha_clamped;
    const Lane_T kappa_seed = one - cond.kappa_clamped;
    const planar_forces<dual_lane<Lane_T>> lat =
        mf5x_planar_forces(c, forms.combined, mode, seed_alpha(point, point.alpha_rad, alpha_seed));
    const planar_forces<dual_lane<Lane_T>> lon =
        mf5x_planar_forces(c, forms.combined, mode, seed_kappa(point, point.kappa, kappa_seed));

    const Lane_T dfy_dsa = cond.contact.select(lat.fy_n.derivative, zero);
    const Lane_T dfx_dsr = cond.contact.select(lon.fx_n.derivative, zero);

    const Lane_T fy = cond.contact.select(at.fy_n, zero);
    const Lane_T fx = cond.contact.select(at.fx_n, zero);
    const mask_of<Lane_T> alpha_nonzero = raw.alpha_rad != 0.0;
    const mask_of<Lane_T> kappa_nonzero = raw.kappa != 0.0;
    const Lane_T fy_secant = fy / raw.alpha_rad;
    const Lane_T fx_secant = fx / raw.kappa;
    const Lane_T fy_ratio = alpha_nonzero.select(fy_secant, dfy_dsa);
    const Lane_T fx_ratio = kappa_nonzero.select(fx_secant, dfx_dsr);

    const Lane_T alpha_side = sgn_plus(raw.alpha_rad);
    const Lane_T alpha_end =
        peak_search_end(alpha_side, limits(mf5x::ALPMIN), limits(mf5x::ALPMAX), PEAK_SEARCH_ALPHA_RAD);
    const auto lateral_slope = [&](const Lane_T &alpha_rad) noexcept -> Lane_T {
      return mf5x_planar_forces(c, forms.combined, mode, seed_alpha(point, alpha_rad, one)).fy_n.derivative;
    };
    const Lane_T sa_peak = first_stationary_point(lateral_slope, alpha_end);

    const Lane_T kappa_side = sgn_plus(raw.kappa);
    const Lane_T kappa_end = peak_search_end(kappa_side, limits(mf5x::KPUMIN), limits(mf5x::KPUMAX), PEAK_SEARCH_KAPPA);
    const auto longitudinal_slope = [&](const Lane_T &slip_ratio) noexcept -> Lane_T {
      return mf5x_planar_forces(c, forms.combined, mode, seed_kappa(point, slip_ratio, one)).fx_n.derivative;
    };
    const Lane_T sr_peak = first_stationary_point(longitudinal_slope, kappa_end);

    characteristic_lanes<Lane_T> out;
    out.dfy_dsa_n_per_rad = dfy_dsa;
    out.dfx_dsr_n = dfx_dsr;
    out.fy_over_sa_n_per_rad = cond.contact.select(fy_ratio, zero);
    out.fx_over_sr_n = cond.contact.select(fx_ratio, zero);
    out.dy_n = cond.contact.select(at.lat.dy_n, zero);
    out.dx_n = cond.contact.select(at.lon.dx_n, zero);
    out.sa_at_peak_fy_rad = cond.contact.select(sa_peak, nan_lane);
    out.sr_at_peak_fx = cond.contact.select(sr_peak, nan_lane);
    return out;
  }
} // namespace mf::detail
