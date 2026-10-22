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
    const double fz0_prime_n = c.FNOMIN * c.LFZO;
    return load_terms{.fz0_prime_n = fz0_prime_n, .dfz = (fz_n - fz0_prime_n) / fz0_prime_n};
  }

  double mu_y(const mf5x_coefficients &c, const load_terms &load, double gamma_y) noexcept {
    return (c.PDY1 + (c.PDY2 * load.dfz)) * (1.0 - (c.PDY3 * gamma_y * gamma_y)) * c.LMUY;
  }

  longitudinal_terms fx0(const mf5x_coefficients &c, const load_terms &load, double fz_n, double kappa,
                         double gamma_rad) noexcept {
    const double dfz = load.dfz;
    longitudinal_terms terms;
    terms.gamma_x = gamma_rad * c.LGAX;
    terms.cx = c.PCX1 * c.LCX;
    terms.mu_x = (c.PDX1 + (c.PDX2 * dfz)) * (1.0 - (c.PDX3 * terms.gamma_x * terms.gamma_x)) * c.LMUX;
    terms.dx_n = terms.mu_x * fz_n;
    terms.kx_n = fz_n * (c.PKX1 + (c.PKX2 * dfz)) * std::exp(c.PKX3 * dfz) * c.LKX;
    terms.shx = (c.PHX1 + (c.PHX2 * dfz)) * c.LHX;
    terms.svx_n = fz_n * (c.PVX1 + (c.PVX2 * dfz)) * c.LVX * c.LMUX;
    terms.kappa_x = kappa + terms.shx;
    const double ex_shape = c.PEX1 + (c.PEX2 * dfz) + (c.PEX3 * dfz * dfz);
    const double ex_sign = 1.0 - (c.PEX4 * sgn_plus(terms.kappa_x));
    terms.ex = limit_curvature(ex_shape * ex_sign * c.LEX);
    // SVx is added outside the sine.
    terms.bx = terms.kx_n / add_epsilon(terms.cx * terms.dx_n);
    terms.fx0_n = sine_magic_formula(terms.bx, terms.cx, terms.dx_n, terms.ex, terms.kappa_x) + terms.svx_n;
    return terms;
  }

  lateral_terms fy0(const mf5x_coefficients &c, const load_terms &load, double fz_n, double alpha_rad,
                    double gamma_rad) noexcept {
    const double dfz = load.dfz;
    lateral_terms terms;
    terms.gamma_y = gamma_rad * c.LGAY;
    terms.shy = ((c.PHY1 + (c.PHY2 * dfz)) * c.LHY) + (c.PHY3 * terms.gamma_y);
    const double svy_pure = (c.PVY1 + (c.PVY2 * dfz)) * c.LVY;
    const double svy_camber = (c.PVY3 + (c.PVY4 * dfz)) * terms.gamma_y;
    terms.svy_n = fz_n * (svy_pure + svy_camber) * c.LMUY;
    terms.alpha_y = alpha_rad + terms.shy;
    terms.cy = c.PCY1 * c.LCY;
    terms.mu_y = mu_y(c, load, terms.gamma_y);
    terms.dy_n = terms.mu_y * fz_n;
    const double ey_shape = c.PEY1 + (c.PEY2 * dfz);
    const double ey_sign = 1.0 - ((c.PEY3 + (c.PEY4 * terms.gamma_y)) * sgn_plus(terms.alpha_y));
    terms.ey = limit_curvature(ey_shape * ey_sign * c.LEY);
    const double load_ratio = fz_n / (c.PKY2 * load.fz0_prime_n);
    terms.ky0_n = c.PKY1 * load.fz0_prime_n * std::sin(2.0 * std::atan(load_ratio)) * c.LKY;
    terms.ky_n = terms.ky0_n * (1.0 - (c.PKY3 * std::abs(terms.gamma_y)));
    terms.by = terms.ky_n / add_epsilon(terms.cy * terms.dy_n);
    terms.fy0_n = sine_magic_formula(terms.by, terms.cy, terms.dy_n, terms.ey, terms.alpha_y) + terms.svy_n;
    return terms;
  }

  aligning_terms aligning(const mf5x_coefficients &c, const load_terms &load, const lateral_terms &lateral, double fz_n,
                          double alpha_rad, double gamma_rad) noexcept {
    const double dfz = load.dfz;
    aligning_terms terms;
    terms.gamma_z = gamma_rad * c.LGAZ;
    terms.sht = c.QHZ1 + (c.QHZ2 * dfz) + ((c.QHZ3 + (c.QHZ4 * dfz)) * terms.gamma_z);
    terms.alpha_t = alpha_rad + terms.sht;
    terms.shf = lateral.shy + (lateral.svy_n / add_epsilon(lateral.ky_n));
    terms.alpha_r = alpha_rad + terms.shf;
    const double bt_load = c.QBZ1 + (c.QBZ2 * dfz) + (c.QBZ3 * dfz * dfz);
    const double bt_camber = 1.0 + (c.QBZ4 * terms.gamma_z) + (c.QBZ5 * std::abs(terms.gamma_z));
    terms.bt = bt_load * bt_camber * c.LKY / c.LMUY;
    terms.ct = c.QCZ1;
    // Dt carries the R0/FNOMIN normaliser.
    const double dt_load = c.QDZ1 + (c.QDZ2 * dfz);
    const double dt_camber = 1.0 + (c.QDZ3 * terms.gamma_z) + (c.QDZ4 * terms.gamma_z * terms.gamma_z);
    terms.dt_m = fz_n * dt_load * dt_camber * (c.UNLOADED_RADIUS / c.FNOMIN) * c.LTR;
    // Et is computed from the pure alpha_t.
    const double et_load = c.QEZ1 + (c.QEZ2 * dfz) + (c.QEZ3 * dfz * dfz);
    const double et_camber = c.QEZ4 + (c.QEZ5 * terms.gamma_z);
    const double et_atan = std::atan(terms.bt * terms.ct * terms.alpha_t) * (2.0 / std::numbers::pi);
    terms.et = limit_curvature(et_load * (1.0 + (et_camber * et_atan)));
    terms.br = (c.QBZ9 * c.LKY / c.LMUY) + (c.QBZ10 * lateral.by * lateral.cy);
    terms.cr = 1.0;
    const double dr_pure = (c.QDZ6 + (c.QDZ7 * dfz)) * c.LRES;
    const double dr_camber = (c.QDZ8 + (c.QDZ9 * dfz)) * terms.gamma_z;
    terms.dr_nm = fz_n * (dr_pure + dr_camber) * c.UNLOADED_RADIUS * c.LMUY;
    return terms;
  }

  slip_state evaluate_slip_state(const mf5x_coefficients &c, const operating_point &point) noexcept {
    slip_state state;
    state.load = vertical_load(c, point.fz_n);
    state.lon = fx0(c, state.load, point.fz_n, point.kappa, point.gamma_rad);
    state.lat = fy0(c, state.load, point.fz_n, point.alpha_rad, point.gamma_rad);
    state.aln = aligning(c, state.load, state.lat, point.fz_n, point.alpha_rad, point.gamma_rad);
    return state;
  }

  double t(const aligning_terms &aln, double x, double cos_factor) noexcept {
    return aln.dt_m * cosine_magic_formula(aln.bt, aln.ct, aln.et, x) * cos_factor;
  }

  double mzr(const aligning_terms &aln, double x, double cos_alpha) noexcept {
    return aln.dr_nm * std::cos(aln.cr * std::atan(aln.br * x)) * cos_alpha;
  }

  double alpha_eq(double a, double kappa, double kx_n, double ky_guarded_n) noexcept {
    const double tan_a = std::tan(a);
    const double stiffness_ratio = kx_n / ky_guarded_n;
    const double radicand = (tan_a * tan_a) + (stiffness_ratio * stiffness_ratio * kappa * kappa);
    return std::atan(std::sqrt(radicand)) * sgn_plus(a);
  }

  double gxa(const mf5x_coefficients &c, const load_terms &load, double kappa, double alpha_rad) noexcept {
    const double shxa = c.RHX1;
    const double alpha_s = alpha_rad + shxa;
    const double bxa = c.RBX1 * std::cos(std::atan(c.RBX2 * kappa)) * c.LXAL;
    const double cxa = c.RCX1;
    const double exa = limit_curvature(c.REX1 + (c.REX2 * load.dfz));
    return cosine_magic_formula(bxa, cxa, exa, alpha_s) / cosine_magic_formula(bxa, cxa, exa, shxa);
  }

  double gyk(const mf5x_coefficients &c, const load_terms &load, double kappa, double alpha_rad) noexcept {
    const double shyk = c.RHY1 + (c.RHY2 * load.dfz);
    const double kappa_s = kappa + shyk;
    const double byk = c.RBY1 * std::cos(std::atan(c.RBY2 * (alpha_rad - c.RBY3))) * c.LYKA;
    const double cyk = c.RCY1;
    const double eyk = limit_curvature(c.REY1 + (c.REY2 * load.dfz));
    return cosine_magic_formula(byk, cyk, eyk, kappa_s) / cosine_magic_formula(byk, cyk, eyk, shyk);
  }

  double svyk(const mf5x_coefficients &c, const load_terms &load, double mu_y_value, double fz_n, double kappa,
              double alpha_rad, double gamma_rad) noexcept {
    const double load_camber = c.RVY1 + (c.RVY2 * load.dfz) + (c.RVY3 * gamma_rad);
    const double dvyk = mu_y_value * fz_n * load_camber * std::cos(std::atan(c.RVY4 * alpha_rad));
    return dvyk * std::sin(c.RVY5 * std::atan(c.RVY6 * kappa)) * c.LVYKA;
  }

  combined_forces cosine_combined_forces(const mf5x_coefficients &c, const slip_state &state,
                                         const operating_point &p) noexcept {
    const double svyk_n = svyk(c, state.load, state.lat.mu_y, p.fz_n, p.kappa, p.alpha_rad, p.gamma_rad);
    return combined_forces{.fx_n = state.lon.fx0_n * gxa(c, state.load, p.kappa, p.alpha_rad),
                           .fy_n = (state.lat.fy0_n * gyk(c, state.load, p.kappa, p.alpha_rad)) + svyk_n,
                           .svyk_n = svyk_n};
  }

  ellipse_ratios friction_ellipse_ratios(const slip_state &state, double kappa, double alpha_rad) noexcept {
    const double kappa_c = kappa + state.lon.shx + (state.lon.svx_n / add_epsilon(state.lon.kx_n));
    const double alpha_c = alpha_rad + state.lat.shy + (state.lat.svy_n / add_epsilon(state.lat.ky_n));
    const double alpha_star = std::sin(alpha_c);
    // Fz-free magnitudes (the 1/Fz factors of the printed form cancel): mu_act*Fz and mu_max*Fz.
    const double mxa = std::abs(state.lon.fx0_n - state.lon.svx_n);
    const double mya = std::abs(state.lat.fy0_n - state.lat.svy_n);
    const double mxm = std::abs(state.lon.dx_n);
    const double mym = std::abs(state.lat.dy_n);
    const double num_x = std::abs(kappa_c) * mym;
    const double den_x = std::sqrt((kappa_c * kappa_c * mym * mym) + (mxa * mxa * alpha_star * alpha_star));
    const double num_y = std::abs(alpha_star) * mxm;
    const double den_y = std::sqrt((kappa_c * kappa_c * mya * mya) + (alpha_star * alpha_star * mxm * mxm));
    // A 0/0 ratio (both slips at the shifted origin) gives 1.
    return ellipse_ratios{.rx = den_x > 0.0 ? num_x / den_x : 1.0, .ry = den_y > 0.0 ? num_y / den_y : 1.0};
  }

  double s(const mf5x_coefficients &c, const load_terms &load, double fy_n, double gamma_rad) noexcept {
    const double fy_term = c.SSZ2 * fy_n / c.FNOMIN;
    const double camber_term = (c.SSZ3 + (c.SSZ4 * load.dfz)) * gamma_rad;
    return c.UNLOADED_RADIUS * (c.SSZ1 + fy_term + camber_term) * c.LS;
  }

  double mx(const mf5x_coefficients &c, double fz_n, double fy_n, double gamma_rad) noexcept {
    const double bracket = (c.QSX1 * c.LVMX) - (c.QSX2 * gamma_rad) + (c.QSX3 * fy_n / c.FNOMIN);
    return c.UNLOADED_RADIUS * fz_n * bracket * c.LMX;
  }

  double my(const mf5x_coefficients &c, double fz_n, double fx_n, double vx_mps) noexcept {
    const double speed_ratio = vx_mps / c.LONGVL;
    const double speed_squared = speed_ratio * speed_ratio;
    const double fx_term = c.QSY2 * fx_n / c.FNOMIN;
    const double speed_term = (c.QSY3 * std::abs(speed_ratio)) + (c.QSY4 * speed_squared * speed_squared);
    return -(c.UNLOADED_RADIUS * fz_n * (c.QSY1 + fx_term + speed_term) * c.LMY);
  }

  double my_from_shifts(const mf5x_coefficients &c, const longitudinal_terms &lon) noexcept {
    return -(c.UNLOADED_RADIUS * (lon.svx_n + (lon.kx_n * lon.shx)));
  }
} // namespace mf::detail
