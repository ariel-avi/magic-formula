#include <cmath>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <numbers>

namespace mf::detail {
  namespace {
    /// atan(B*x - E*(B*x - atan(B*x))): the argument shape shared by the sine and cosine Magic Formulas.
    double shaped_argument(double b, double e, double x) noexcept {
      const double bx = b * x;
      return std::atan(bx - (e * (bx - std::atan(bx))));
    }
  } // namespace

  double sine_magic_formula(double b, double c, double d, double e, double x) noexcept {
    return d * std::sin(c * shaped_argument(b, e, x));
  }

  double cosine_magic_formula(double b, double c, double e, double x) noexcept {
    return std::cos(c * shaped_argument(b, e, x));
  }

  load_terms vertical_load(const mf5x_coefficients &c, double fz_n) noexcept {
    // Eq (16).
    const double fz0_prime_n = c.FNOMIN * c.LFZO;
    return load_terms{.fz0_prime_n = fz0_prime_n, .dfz = (fz_n - fz0_prime_n) / fz0_prime_n};
  }

  double lateral_friction(const mf5x_coefficients &c, const load_terms &load, double gamma_y) noexcept {
    // Eq (35).
    return (c.PDY1 + (c.PDY2 * load.dfz)) * (1.0 - (c.PDY3 * gamma_y * gamma_y)) * c.LMUY;
  }

  longitudinal_terms pure_longitudinal(const mf5x_coefficients &c, const load_terms &load, double fz_n, double kappa,
                                       double gamma_rad) noexcept {
    const double dfz = load.dfz;
    longitudinal_terms t;
    // Eqs (20), (21), (23) with gamma_x (C3), (22), (25), (27), (28), (19).
    t.gamma_x = gamma_rad * c.LGAX;
    t.cx = c.PCX1 * c.LCX;
    t.mu_x = (c.PDX1 + (c.PDX2 * dfz)) * (1.0 - (c.PDX3 * t.gamma_x * t.gamma_x)) * c.LMUX;
    t.dx_n = t.mu_x * fz_n;
    t.kx_n = fz_n * (c.PKX1 + (c.PKX2 * dfz)) * std::exp(c.PKX3 * dfz) * c.LKX;
    t.shx = (c.PHX1 + (c.PHX2 * dfz)) * c.LHX;
    t.svx_n = fz_n * (c.PVX1 + (c.PVX2 * dfz)) * c.LVX * c.LMUX;
    t.kappa_x = kappa + t.shx;
    // Eq (24) with E <= 1 (D-17).
    const double ex_shape = c.PEX1 + (c.PEX2 * dfz) + (c.PEX3 * dfz * dfz);
    const double ex_sign = 1.0 - (c.PEX4 * sgn_plus(t.kappa_x));
    t.ex = limit_curvature(ex_shape * ex_sign * c.LEX);
    // Eq (26) with the epsilon guard (AD-03) and eq (18) with SVx outside the sine (C2).
    t.bx = t.kx_n / add_epsilon(t.cx * t.dx_n);
    t.fx0_n = sine_magic_formula(t.bx, t.cx, t.dx_n, t.ex, t.kappa_x) + t.svx_n;
    return t;
  }

  lateral_terms pure_lateral(const mf5x_coefficients &c, const load_terms &load, double fz_n, double alpha_rad,
                             double gamma_rad) noexcept {
    const double dfz = load.dfz;
    lateral_terms t;
    // Eqs (32), (40), (41), (31), (33), (35), (34).
    t.gamma_y = gamma_rad * c.LGAY;
    t.shy = ((c.PHY1 + (c.PHY2 * dfz)) * c.LHY) + (c.PHY3 * t.gamma_y);
    const double svy_pure = (c.PVY1 + (c.PVY2 * dfz)) * c.LVY;
    const double svy_camber = (c.PVY3 + (c.PVY4 * dfz)) * t.gamma_y;
    t.svy_n = fz_n * (svy_pure + svy_camber) * c.LMUY;
    t.alpha_y = alpha_rad + t.shy;
    t.cy = c.PCY1 * c.LCY;
    t.mu_y = lateral_friction(c, load, t.gamma_y);
    t.dy_n = t.mu_y * fz_n;
    // Eq (36) with E <= 1 (C9, D-17).
    const double ey_shape = c.PEY1 + (c.PEY2 * dfz);
    const double ey_sign = 1.0 - ((c.PEY3 + (c.PEY4 * t.gamma_y)) * sgn_plus(t.alpha_y));
    t.ey = limit_curvature(ey_shape * ey_sign * c.LEY);
    // Eqs (37) in the derived form, (38).
    const double load_ratio = fz_n / (c.PKY2 * load.fz0_prime_n);
    t.ky0_n = c.PKY1 * load.fz0_prime_n * std::sin(2.0 * std::atan(load_ratio)) * c.LKY;
    t.ky_n = t.ky0_n * (1.0 - (c.PKY3 * std::abs(t.gamma_y)));
    // Eq (39) with the epsilon guard (AD-03) and eq (30).
    t.by = t.ky_n / add_epsilon(t.cy * t.dy_n);
    t.fy0_n = sine_magic_formula(t.by, t.cy, t.dy_n, t.ey, t.alpha_y) + t.svy_n;
    return t;
  }

  aligning_terms aligning(const mf5x_coefficients &c, const load_terms &load, const lateral_terms &lateral, double fz_n,
                          double alpha_rad, double gamma_rad) noexcept {
    const double dfz = load.dfz;
    aligning_terms t;
    // Eqs (49), (54), (45), (48) with the epsilon guard (AD-03), (47).
    t.gamma_z = gamma_rad * c.LGAZ;
    t.sht = c.QHZ1 + (c.QHZ2 * dfz) + ((c.QHZ3 + (c.QHZ4 * dfz)) * t.gamma_z);
    t.alpha_t = alpha_rad + t.sht;
    t.shf = lateral.shy + (lateral.svy_n / add_epsilon(lateral.ky_n));
    t.alpha_r = alpha_rad + t.shf;
    // Eqs (50), (51).
    const double bt_load = c.QBZ1 + (c.QBZ2 * dfz) + (c.QBZ3 * dfz * dfz);
    const double bt_camber = 1.0 + (c.QBZ4 * t.gamma_z) + (c.QBZ5 * std::abs(t.gamma_z));
    t.bt = bt_load * bt_camber * c.LKY / c.LMUY;
    t.ct = c.QCZ1;
    // Eq (52) with the R0/FNOMIN normaliser (D-23, AD-18).
    const double dt_load = c.QDZ1 + (c.QDZ2 * dfz);
    const double dt_camber = 1.0 + (c.QDZ3 * t.gamma_z) + (c.QDZ4 * t.gamma_z * t.gamma_z);
    t.dt_m = fz_n * dt_load * dt_camber * (c.UNLOADED_RADIUS / c.FNOMIN) * c.LTR;
    // Eq (53) with E <= 1, from the pure alpha_t (D-24).
    const double et_load = c.QEZ1 + (c.QEZ2 * dfz) + (c.QEZ3 * dfz * dfz);
    const double et_camber = c.QEZ4 + (c.QEZ5 * t.gamma_z);
    const double et_atan = std::atan(t.bt * t.ct * t.alpha_t) * (2.0 / std::numbers::pi);
    t.et = limit_curvature(et_load * (1.0 + (et_camber * et_atan)));
    // Eq (55), Cr = 1 (p.24) and eq (56) (C10).
    t.br = (c.QBZ9 * c.LKY / c.LMUY) + (c.QBZ10 * lateral.by * lateral.cy);
    t.cr = 1.0;
    const double dr_pure = (c.QDZ6 + (c.QDZ7 * dfz)) * c.LRES;
    const double dr_camber = (c.QDZ8 + (c.QDZ9 * dfz)) * t.gamma_z;
    t.dr_nm = fz_n * (dr_pure + dr_camber) * c.UNLOADED_RADIUS * c.LMUY;
    return t;
  }

  slip_state evaluate_slip_state(const mf5x_coefficients &c, const operating_point &point) noexcept {
    slip_state s;
    s.load = vertical_load(c, point.fz_n);
    s.lon = pure_longitudinal(c, s.load, point.fz_n, point.kappa, point.gamma_rad);
    s.lat = pure_lateral(c, s.load, point.fz_n, point.alpha_rad, point.gamma_rad);
    s.aln = aligning(c, s.load, s.lat, point.fz_n, point.alpha_rad, point.gamma_rad);
    return s;
  }

  double trail(const aligning_terms &aln, double x, double cos_factor) noexcept {
    // Eqs (44), (79).
    return aln.dt_m * cosine_magic_formula(aln.bt, aln.ct, aln.et, x) * cos_factor;
  }

  double residual_moment(const aligning_terms &aln, double x, double cos_alpha) noexcept {
    // Eqs (46), (82) (C4).
    return aln.dr_nm * std::cos(aln.cr * std::atan(aln.br * x)) * cos_alpha;
  }

  double equivalent_slip_angle(double a, double kappa, double kx_n, double ky_guarded_n) noexcept {
    // Eqs (84), (85) with the sign outside the root (C5, D-05).
    const double tan_a = std::tan(a);
    const double stiffness_ratio = kx_n / ky_guarded_n;
    const double radicand = (tan_a * tan_a) + (stiffness_ratio * stiffness_ratio * kappa * kappa);
    return std::atan(std::sqrt(radicand)) * sgn_plus(a);
  }

  double longitudinal_weighting(const mf5x_coefficients &c, const load_terms &load, double kappa,
                                double alpha_rad) noexcept {
    // Eqs (65), (60), (61), (62), (64) with E <= 1 (D-17), then (66).
    const double shxa = c.RHX1;
    const double alpha_s = alpha_rad + shxa;
    const double bxa = c.RBX1 * std::cos(std::atan(c.RBX2 * kappa)) * c.LXAL;
    const double cxa = c.RCX1;
    const double exa = limit_curvature(c.REX1 + (c.REX2 * load.dfz));
    return cosine_magic_formula(bxa, cxa, exa, alpha_s) / cosine_magic_formula(bxa, cxa, exa, shxa);
  }

  double lateral_weighting(const mf5x_coefficients &c, const load_terms &load, double kappa,
                           double alpha_rad) noexcept {
    // Eqs (74), (69), (70), (71), (73) with E <= 1 (D-17), then (77).
    const double shyk = c.RHY1 + (c.RHY2 * load.dfz);
    const double kappa_s = kappa + shyk;
    const double byk = c.RBY1 * std::cos(std::atan(c.RBY2 * (alpha_rad - c.RBY3))) * c.LYKA;
    const double cyk = c.RCY1;
    const double eyk = limit_curvature(c.REY1 + (c.REY2 * load.dfz));
    return cosine_magic_formula(byk, cyk, eyk, kappa_s) / cosine_magic_formula(byk, cyk, eyk, shyk);
  }

  double kappa_induced_side_force(const mf5x_coefficients &c, const load_terms &load, double mu_y, double fz_n,
                                  double kappa, double alpha_rad, double gamma_rad) noexcept {
    // Eq (76), then eq (75).
    const double load_camber = c.RVY1 + (c.RVY2 * load.dfz) + (c.RVY3 * gamma_rad);
    const double dvyk = mu_y * fz_n * load_camber * std::cos(std::atan(c.RVY4 * alpha_rad));
    return dvyk * std::sin(c.RVY5 * std::atan(c.RVY6 * kappa)) * c.LVYKA;
  }

  combined_forces cosine_combined_forces(const mf5x_coefficients &c, const slip_state &s,
                                         const operating_point &p) noexcept {
    // Eqs (58) and (67).
    const double gxa = longitudinal_weighting(c, s.load, p.kappa, p.alpha_rad);
    const double gyk = lateral_weighting(c, s.load, p.kappa, p.alpha_rad);
    const double svyk = kappa_induced_side_force(c, s.load, s.lat.mu_y, p.fz_n, p.kappa, p.alpha_rad, p.gamma_rad);
    return combined_forces{.fx_n = s.lon.fx0_n * gxa, .fy_n = (s.lat.fy0_n * gyk) + svyk, .svyk_n = svyk};
  }

  ellipse_ratios friction_ellipse_ratios(const slip_state &s, double kappa, double alpha_rad) noexcept {
    const double kappa_c = kappa + s.lon.shx + (s.lon.svx_n / add_epsilon(s.lon.kx_n));
    const double alpha_c = alpha_rad + s.lat.shy + (s.lat.svy_n / add_epsilon(s.lat.ky_n));
    const double alpha_star = std::sin(alpha_c);
    // Fz-free magnitudes (the 1/Fz factors of the printed form cancel): mu_act*Fz and mu_max*Fz.
    const double mxa = std::abs(s.lon.fx0_n - s.lon.svx_n);
    const double mya = std::abs(s.lat.fy0_n - s.lat.svy_n);
    const double mxm = std::abs(s.lon.dx_n);
    const double mym = std::abs(s.lat.dy_n);
    const double num_x = std::abs(kappa_c) * mym;
    const double den_x = std::sqrt((kappa_c * kappa_c * mym * mym) + (mxa * mxa * alpha_star * alpha_star));
    const double num_y = std::abs(alpha_star) * mxm;
    const double den_y = std::sqrt((kappa_c * kappa_c * mya * mya) + (alpha_star * alpha_star * mxm * mxm));
    // A 0/0 ratio (both slips at the shifted origin) gives 1 (AD-24).
    return ellipse_ratios{.rx = den_x > 0.0 ? num_x / den_x : 1.0, .ry = den_y > 0.0 ? num_y / den_y : 1.0};
  }

  double moment_arm(const mf5x_coefficients &c, const load_terms &load, double fy_n, double gamma_rad) noexcept {
    const double fy_term = c.SSZ2 * fy_n / c.FNOMIN;
    const double camber_term = (c.SSZ3 + (c.SSZ4 * load.dfz)) * gamma_rad;
    return c.UNLOADED_RADIUS * (c.SSZ1 + fy_term + camber_term) * c.LS;
  }

  double overturning_moment(const mf5x_coefficients &c, double fz_n, double fy_n, double gamma_rad) noexcept {
    // Eq (86).
    const double bracket = (c.QSX1 * c.LVMX) - (c.QSX2 * gamma_rad) + (c.QSX3 * fy_n / c.FNOMIN);
    return c.UNLOADED_RADIUS * fz_n * bracket * c.LMX;
  }

  double rolling_resistance_moment(const mf5x_coefficients &c, double fz_n, double fx_n, double vx_mps) noexcept {
    // Negative of eq (87) (C6, C7, D-11).
    const double speed_ratio = vx_mps / c.LONGVL;
    const double speed_squared = speed_ratio * speed_ratio;
    const double fx_term = c.QSY2 * fx_n / c.FNOMIN;
    const double speed_term = (c.QSY3 * std::abs(speed_ratio)) + (c.QSY4 * speed_squared * speed_squared);
    return -(c.UNLOADED_RADIUS * fz_n * (c.QSY1 + fx_term + speed_term) * c.LMY);
  }

  double rolling_resistance_moment_eq88(const mf5x_coefficients &c, const longitudinal_terms &lon) noexcept {
    // Negative of eq (88) as printed, without LMY (AD-17).
    return -(c.UNLOADED_RADIUS * (lon.svx_n + (lon.kx_n * lon.shx)));
  }
} // namespace mf::detail
