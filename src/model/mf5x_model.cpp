#include <cmath>
#include <expected>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <magic_formula/tire_model.hpp>

namespace mf {
  mf5x_model::mf5x_model(const mf5x_parameters &parameters)
      : tire_model(parameters.limits, parameters.metadata), coefficients_(parameters.coefficients) {}

  const mf5x_coefficients &mf5x_model::coefficients() const noexcept {
    return coefficients_;
  }

  std::expected<void, error> mf5x_model::validate_parameters(const mf5x_parameters &parameters) {
    if (const std::expected<void, error> valid = validate_coefficients(parameters.coefficients); !valid.has_value()) {
      return std::unexpected<error>(valid.error());
    }
    return validate_limits(parameters.limits);
  }

  detail::combined_forces mf5x_model::combine(const operating_point &point,
                                              const detail::slip_state &state) const noexcept {
    return detail::cosine_combined_forces(coefficients_, state, point);
  }

  double mf5x_model::aligning_lateral_force(const operating_point & /*point*/, const detail::slip_state & /*state*/,
                                            const detail::combined_forces &forces) const noexcept {
    return forces.fy_n - forces.svyk_n;
  }

  double mf5x_model::rolling_resistance(const operating_point &point, const detail::slip_state & /*state*/,
                                        double fx_n) const noexcept {
    return detail::rolling_resistance_moment(coefficients_, point.fz_n, fx_n, point.vx_mps);
  }

  tire_outputs mf5x_model::compute(const operating_point &point, slip_mode mode) const noexcept {
    const mf5x_coefficients &c = coefficients_;
    const detail::slip_state state = detail::evaluate_slip_state(c, point);
    const detail::longitudinal_terms &lon = state.lon;
    const detail::lateral_terms &lat = state.lat;
    const detail::aligning_terms &aln = state.aln;
    const double cos_trail = trail_cosine(point);
    const double cos_alpha = std::cos(point.alpha_rad);

    tire_outputs out;
    if (mode == slip_mode::UNCOMBINED) {
      // D-26: pure-slip forces and Mz0 = -t(alpha_t)*Fy0 + Mzr(alpha_r).
      out.fx_n = lon.fx0_n;
      out.fy_n = lat.fy0_n;
      const double trail_m = detail::trail(aln, aln.alpha_t, cos_trail);
      out.mz_nm = -(trail_m * out.fy_n) + detail::residual_moment(aln, aln.alpha_r, cos_alpha);
    } else {
      // Eq (78) with the equivalent slip angles (84)-(85) (D-05) and the moment arm of Fx (D-08, D-09).
      const detail::combined_forces forces = combine(point, state);
      const double ky_guarded_n = detail::add_epsilon(lat.ky_n);
      const double ate = detail::equivalent_slip_angle(aln.alpha_t, point.kappa, lon.kx_n, ky_guarded_n);
      const double are = detail::equivalent_slip_angle(aln.alpha_r, point.kappa, lon.kx_n, ky_guarded_n);
      const double trail_m = detail::trail(aln, ate, cos_trail);
      const double fy_aligning_n = aligning_lateral_force(point, state, forces);
      const double arm_m = detail::moment_arm(c, state.load, forces.fy_n, point.gamma_rad);
      out.fx_n = forces.fx_n;
      out.fy_n = forces.fy_n;
      out.mz_nm = -(trail_m * fy_aligning_n) + detail::residual_moment(aln, are, cos_alpha) + (arm_m * forces.fx_n);
    }
    // Eq (86) and the rolling-resistance moment of the evaluated mode.
    out.mx_nm = detail::overturning_moment(c, point.fz_n, out.fy_n, point.gamma_rad);
    out.my_nm = rolling_resistance(point, state, out.fx_n);
    return out;
  }
} // namespace mf
