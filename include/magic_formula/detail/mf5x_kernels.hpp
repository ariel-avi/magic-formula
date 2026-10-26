#pragma once

#include <Eigen/Core>
#include <cmath>
#include <magic_formula/detail/lanes.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_forms.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <numbers>

/// Pure kernels of the MF 5.x equations shared by PAC2002 and MF 5.2 (internal, not API), written once over a lane
/// type (`point_lane` for one point, `block_lane` for a batch block). Camber always enters as gamma*LGAx; MF 5.2
/// forces LGA* = 1, so the same code yields the raw camber there. All functions are total and never throw.
/// Equations: docs/research/pac2002.md.
namespace mf::detail {
  /// atan(B*x - E*(B*x - atan(B*x))): the argument shape shared by the sine and cosine Magic Formulas.
  template <typename Lane_T>
  [[nodiscard]] Lane_T shaped_argument(const Lane_T &b, const Lane_T &e, const Lane_T &x) noexcept {
    const Lane_T bx = b * x;
    return (bx - (e * (bx - bx.atan()))).atan();
  }

  /// D*sin(C*atan(B*x - E*(B*x - atan(B*x)))): the sine-form Magic Formula.
  template <typename Lane_T>
  [[nodiscard]] Lane_T sine_magic_formula(const Lane_T &b, double c, const Lane_T &d, const Lane_T &e,
                                          const Lane_T &x) noexcept {
    const Lane_T shaped = shaped_argument(b, e, x);
    return d * (c * shaped).sin();
  }

  /// cos(C*atan(B*x - E*(B*x - atan(B*x)))): the cosine-form Magic Formula.
  template <typename Lane_T>
  [[nodiscard]] Lane_T cosine_magic_formula(const Lane_T &b, double c, const Lane_T &e, const Lane_T &x) noexcept {
    const Lane_T shaped = shaped_argument(b, e, x);
    return (c * shaped).cos();
  }

  /// Vertical-load terms.
  template <typename Lane_T> struct load_terms {
    /// F'z0 = FNOMIN*LFZO [N].
    double fz0_prime_n = 0.0;
    /// dfz = (Fz - F'z0)/F'z0 [-].
    Lane_T dfz;
  };

  /// Pure longitudinal slip terms.
  template <typename Lane_T> struct longitudinal_terms {
    /// kx = kappa + SHx [-].
    Lane_T kappa_x;
    /// gx = gamma*LGAX [rad].
    Lane_T gamma_x;
    /// Cx = PCX1*LCX [-].
    double cx = 0.0;
    /// Longitudinal peak friction mu_x [-].
    Lane_T mu_x;
    /// Dx = mu_x*Fz [N].
    Lane_T dx_n;
    /// Ex limited to <= 1 [-].
    Lane_T ex;
    /// Longitudinal slip stiffness Kx [N].
    Lane_T kx_n;
    /// Bx = Kx/(Cx*Dx + eps) [-].
    Lane_T bx;
    /// Horizontal shift SHx [-].
    Lane_T shx;
    /// Vertical shift SVx [N].
    Lane_T svx_n;
    /// Fx0 with SVx added outside the sine [N].
    Lane_T fx0_n;
  };

  /// Pure lateral slip terms.
  template <typename Lane_T> struct lateral_terms {
    /// ay = alpha + SHy [rad].
    Lane_T alpha_y;
    /// gy = gamma*LGAY [rad].
    Lane_T gamma_y;
    /// Cy = PCY1*LCY [-].
    double cy = 0.0;
    /// Lateral peak friction mu_y [-].
    Lane_T mu_y;
    /// Dy = mu_y*Fz [N].
    Lane_T dy_n;
    /// Ey limited to <= 1 [-].
    Lane_T ey;
    /// Ky0 [N/rad].
    Lane_T ky0_n;
    /// Ky = Ky0*(1 - PKY3*|gy|) [N/rad].
    Lane_T ky_n;
    /// By = Ky/(Cy*Dy + eps) [1/rad].
    Lane_T by;
    /// Horizontal shift SHy [rad].
    Lane_T shy;
    /// Vertical shift SVy [N].
    Lane_T svy_n;
    /// Fy0 [N].
    Lane_T fy0_n;
  };

  /// Aligning-moment terms.
  template <typename Lane_T> struct aligning_terms {
    /// gz = gamma*LGAZ [rad].
    Lane_T gamma_z;
    /// at = alpha + SHt [rad].
    Lane_T alpha_t;
    /// ar = alpha + SHf [rad].
    Lane_T alpha_r;
    /// Trail horizontal shift SHt [-].
    Lane_T sht;
    /// SHf = SHy + SVy/(Ky + eps) [rad].
    Lane_T shf;
    /// Bt [1/rad].
    Lane_T bt;
    /// Ct = QCZ1 [-].
    double ct = 0.0;
    /// Dt with the R0/FNOMIN normaliser [m].
    Lane_T dt_m;
    /// Et limited to <= 1 [-].
    Lane_T et;
    /// Br [1/rad].
    Lane_T br;
    /// Cr = 1 [-].
    double cr = 0.0;
    /// Dr [N*m].
    Lane_T dr_nm;
  };

  /// All pure-slip terms of one operating point.
  template <typename Lane_T> struct slip_state {
    load_terms<Lane_T> load;
    longitudinal_terms<Lane_T> lon;
    lateral_terms<Lane_T> lat;
    aligning_terms<Lane_T> aln;
  };

  /// Combined-slip forces (and the kappa-induced side force that is part of Fy).
  template <typename Lane_T> struct combined_forces {
    /// Combined Fx [N].
    Lane_T fx_n;
    /// Combined Fy [N].
    Lane_T fy_n;
    /// SVyk contained in `fy_n`; 0 for the friction-ellipse method [N].
    Lane_T svyk_n;
  };

  /// Friction-ellipse reduction ratios Fx/Fx0 and Fy/Fy0.
  template <typename Lane_T> struct ellipse_ratios {
    /// Fx/Fx0, 1 when the ratio is 0/0.
    Lane_T rx;
    /// Fy/Fy0, 1 when the ratio is 0/0.
    Lane_T ry;
  };

  /// The inputs of the evaluation in SI units, one value per point, in input index order.
  template <typename Lane_T> struct input_lanes {
    /// Vertical load Fz [N].
    Lane_T fz_n;
    /// Slip angle alpha [rad].
    Lane_T alpha_rad;
    /// Slip ratio kappa [-].
    Lane_T kappa;
    /// Inclination angle gamma [rad].
    Lane_T gamma_rad;
    /// Longitudinal speed Vx [m/s].
    Lane_T vx_mps;
  };

  /// The forces and moments of the evaluation, one value per point, in output index order.
  template <typename Lane_T> struct force_lanes {
    /// Longitudinal force Fx [N].
    Lane_T fx_n;
    /// Lateral force Fy [N].
    Lane_T fy_n;
    /// Self-aligning moment Mz [N*m].
    Lane_T mz_nm;
    /// Overturning moment Mx [N*m].
    Lane_T mx_nm;
    /// Rolling-resistance moment My [N*m].
    Lane_T my_nm;
  };

  /// F'z0 = FNOMIN*LFZO and dfz = (Fz - F'z0)/F'z0.
  template <typename Lane_T>
  [[nodiscard]] load_terms<Lane_T> vertical_load(const mf5x_coefficients &c, const Lane_T &fz_n) noexcept {
    const double fz0_prime_n = c.FNOMIN * c.LFZO;
    const Lane_T dfz = (fz_n - fz0_prime_n) / fz0_prime_n;
    return load_terms<Lane_T>{.fz0_prime_n = fz0_prime_n, .dfz = dfz};
  }

  /// Lateral peak friction mu_y = (PDY1 + PDY2*dfz)*(1 - PDY3*gy^2)*LMUY. `gamma_y` is the scaled camber gy; MF 5.2
  /// passes 0 for the zero-camber mu_y of SVyk0.
  template <typename Lane_T>
  [[nodiscard]] Lane_T mu_y(const mf5x_coefficients &c, const load_terms<Lane_T> &load,
                            const Lane_T &gamma_y) noexcept {
    return (c.PDY1 + (c.PDY2 * load.dfz)) * (1.0 - (c.PDY3 * gamma_y * gamma_y)) * c.LMUY;
  }

  /// Pure longitudinal force Fx0 with all intermediate terms.
  template <typename Lane_T>
  [[nodiscard]] longitudinal_terms<Lane_T> fx0(const mf5x_coefficients &c, const load_terms<Lane_T> &load,
                                               const Lane_T &fz_n, const Lane_T &kappa,
                                               const Lane_T &gamma_rad) noexcept {
    const Lane_T &dfz = load.dfz;
    longitudinal_terms<Lane_T> terms;
    terms.gamma_x = gamma_rad * c.LGAX;
    terms.cx = c.PCX1 * c.LCX;
    terms.mu_x = (c.PDX1 + (c.PDX2 * dfz)) * (1.0 - (c.PDX3 * terms.gamma_x * terms.gamma_x)) * c.LMUX;
    terms.dx_n = terms.mu_x * fz_n;
    terms.kx_n = fz_n * (c.PKX1 + (c.PKX2 * dfz)) * (c.PKX3 * dfz).exp() * c.LKX;
    terms.shx = (c.PHX1 + (c.PHX2 * dfz)) * c.LHX;
    terms.svx_n = fz_n * (c.PVX1 + (c.PVX2 * dfz)) * c.LVX * c.LMUX;
    terms.kappa_x = kappa + terms.shx;
    const Lane_T ex_shape = c.PEX1 + (c.PEX2 * dfz) + (c.PEX3 * dfz * dfz);
    const Lane_T ex_sign = 1.0 - (c.PEX4 * sgn_plus(terms.kappa_x));
    const Lane_T ex_raw = ex_shape * ex_sign * c.LEX;
    terms.ex = limit_curvature(ex_raw);
    // SVx is added outside the sine.
    const Lane_T cd_product = terms.cx * terms.dx_n;
    terms.bx = terms.kx_n / add_epsilon(cd_product);
    terms.fx0_n = sine_magic_formula(terms.bx, terms.cx, terms.dx_n, terms.ex, terms.kappa_x) + terms.svx_n;
    return terms;
  }

  /// Pure lateral force Fy0 with all intermediate terms. Ky0 = PKY1*F'z0*sin(2*atan(Fz/(PKY2*F'z0)))*LKY.
  template <typename Lane_T>
  [[nodiscard]] lateral_terms<Lane_T> fy0(const mf5x_coefficients &c, const load_terms<Lane_T> &load,
                                          const Lane_T &fz_n, const Lane_T &alpha_rad,
                                          const Lane_T &gamma_rad) noexcept {
    const Lane_T &dfz = load.dfz;
    lateral_terms<Lane_T> terms;
    terms.gamma_y = gamma_rad * c.LGAY;
    terms.shy = ((c.PHY1 + (c.PHY2 * dfz)) * c.LHY) + (c.PHY3 * terms.gamma_y);
    const Lane_T svy_pure = (c.PVY1 + (c.PVY2 * dfz)) * c.LVY;
    const Lane_T svy_camber = (c.PVY3 + (c.PVY4 * dfz)) * terms.gamma_y;
    terms.svy_n = fz_n * (svy_pure + svy_camber) * c.LMUY;
    terms.alpha_y = alpha_rad + terms.shy;
    terms.cy = c.PCY1 * c.LCY;
    terms.mu_y = mu_y(c, load, terms.gamma_y);
    terms.dy_n = terms.mu_y * fz_n;
    const Lane_T ey_shape = c.PEY1 + (c.PEY2 * dfz);
    const Lane_T ey_sign = 1.0 - ((c.PEY3 + (c.PEY4 * terms.gamma_y)) * sgn_plus(terms.alpha_y));
    const Lane_T ey_raw = ey_shape * ey_sign * c.LEY;
    terms.ey = limit_curvature(ey_raw);
    const Lane_T load_ratio = fz_n / (c.PKY2 * load.fz0_prime_n);
    terms.ky0_n = c.PKY1 * load.fz0_prime_n * (2.0 * load_ratio.atan()).sin() * c.LKY;
    terms.ky_n = terms.ky0_n * (1.0 - (c.PKY3 * terms.gamma_y.abs()));
    const Lane_T cd_product = terms.cy * terms.dy_n;
    terms.by = terms.ky_n / add_epsilon(cd_product);
    terms.fy0_n = sine_magic_formula(terms.by, terms.cy, terms.dy_n, terms.ey, terms.alpha_y) + terms.svy_n;
    return terms;
  }

  /// Aligning terms. SHf = SHy + SVy/add_epsilon(Ky). Et uses the pure alpha_t.
  template <typename Lane_T>
  [[nodiscard]] aligning_terms<Lane_T> aligning(const mf5x_coefficients &c, const load_terms<Lane_T> &load,
                                                const lateral_terms<Lane_T> &lateral, const Lane_T &fz_n,
                                                const Lane_T &alpha_rad, const Lane_T &gamma_rad) noexcept {
    const Lane_T &dfz = load.dfz;
    aligning_terms<Lane_T> terms;
    terms.gamma_z = gamma_rad * c.LGAZ;
    terms.sht = c.QHZ1 + (c.QHZ2 * dfz) + ((c.QHZ3 + (c.QHZ4 * dfz)) * terms.gamma_z);
    terms.alpha_t = alpha_rad + terms.sht;
    terms.shf = lateral.shy + (lateral.svy_n / add_epsilon(lateral.ky_n));
    terms.alpha_r = alpha_rad + terms.shf;
    const Lane_T bt_load = c.QBZ1 + (c.QBZ2 * dfz) + (c.QBZ3 * dfz * dfz);
    const Lane_T bt_camber = 1.0 + (c.QBZ4 * terms.gamma_z) + (c.QBZ5 * terms.gamma_z.abs());
    terms.bt = bt_load * bt_camber * c.LKY / c.LMUY;
    terms.ct = c.QCZ1;
    // Dt carries the R0/FNOMIN normaliser.
    const Lane_T dt_load = c.QDZ1 + (c.QDZ2 * dfz);
    const Lane_T dt_camber = 1.0 + (c.QDZ3 * terms.gamma_z) + (c.QDZ4 * terms.gamma_z * terms.gamma_z);
    terms.dt_m = fz_n * dt_load * dt_camber * (c.UNLOADED_RADIUS / c.FNOMIN) * c.LTR;
    // Et is computed from the pure alpha_t.
    const Lane_T et_load = c.QEZ1 + (c.QEZ2 * dfz) + (c.QEZ3 * dfz * dfz);
    const Lane_T et_camber = c.QEZ4 + (c.QEZ5 * terms.gamma_z);
    const Lane_T et_atan = (terms.bt * terms.ct * terms.alpha_t).atan() * (2.0 / std::numbers::pi);
    const Lane_T et_raw = et_load * (1.0 + (et_camber * et_atan));
    terms.et = limit_curvature(et_raw);
    terms.br = (c.QBZ9 * c.LKY / c.LMUY) + (c.QBZ10 * lateral.by * lateral.cy);
    terms.cr = 1.0;
    const Lane_T dr_pure = (c.QDZ6 + (c.QDZ7 * dfz)) * c.LRES;
    const Lane_T dr_camber = (c.QDZ8 + (c.QDZ9 * dfz)) * terms.gamma_z;
    terms.dr_nm = fz_n * (dr_pure + dr_camber) * c.UNLOADED_RADIUS * c.LMUY;
    return terms;
  }

  /// Load, pure Fx0, pure Fy0 and the aligning terms of `p`, which must be validated and clamped.
  template <typename Lane_T>
  [[nodiscard]] slip_state<Lane_T> evaluate_slip_state(const mf5x_coefficients &c,
                                                       const input_lanes<Lane_T> &p) noexcept {
    slip_state<Lane_T> state;
    state.load = vertical_load(c, p.fz_n);
    state.lon = fx0(c, state.load, p.fz_n, p.kappa, p.gamma_rad);
    state.lat = fy0(c, state.load, p.fz_n, p.alpha_rad, p.gamma_rad);
    state.aln = aligning(c, state.load, state.lat, p.fz_n, p.alpha_rad, p.gamma_rad);
    return state;
  }

  /// Pneumatic trail t = Dt*cos(Ct*atan(Bt*x - Et*(Bt*x - atan(Bt*x))))*cos_factor. `x` is alpha_t (pure) or the
  /// equivalent slip angle ate (combined); `cos_factor` is cos(alpha) for PAC2002 and cos'(alpha) for MF 5.2.
  template <typename Lane_T>
  [[nodiscard]] Lane_T t(const aligning_terms<Lane_T> &aln, const Lane_T &x, const Lane_T &cos_factor) noexcept {
    const Lane_T shape = cosine_magic_formula(aln.bt, aln.ct, aln.et, x);
    return aln.dt_m * shape * cos_factor;
  }

  /// Residual moment Mzr = Dr*cos(Cr*atan(Br*x))*cos_alpha. `x` is alpha_r (pure) or the equivalent slip angle are
  /// (combined).
  template <typename Lane_T>
  [[nodiscard]] Lane_T mzr(const aligning_terms<Lane_T> &aln, const Lane_T &x, const Lane_T &cos_alpha) noexcept {
    return aln.dr_nm * (aln.cr * (aln.br * x).atan()).cos() * cos_alpha;
  }

  /// Equivalent slip angle atan(sqrt(tan^2(a) + (Kx/Ky)^2*kappa^2))*sgn+(a), with the sign outside the root.
  /// `ky_guarded_n` is add_epsilon(Ky).
  template <typename Lane_T>
  [[nodiscard]] Lane_T alpha_eq(const Lane_T &a, const Lane_T &kappa, const Lane_T &kx_n,
                                const Lane_T &ky_guarded_n) noexcept {
    const Lane_T tan_a = a.tan();
    const Lane_T stiffness_ratio = kx_n / ky_guarded_n;
    const Lane_T radicand = (tan_a * tan_a) + (stiffness_ratio * stiffness_ratio * kappa * kappa);
    return radicand.sqrt().atan() * sgn_plus(a);
  }

  /// Combined-slip weighting function Gxa of Fx. The denominator is not guarded.
  template <typename Lane_T>
  [[nodiscard]] Lane_T gxa(const mf5x_coefficients &c, const load_terms<Lane_T> &load, const Lane_T &kappa,
                           const Lane_T &alpha_rad) noexcept {
    const Eigen::Index n = kappa.size();
    const double shxa = c.RHX1;
    const Lane_T alpha_s = alpha_rad + shxa;
    const Lane_T bxa = c.RBX1 * (c.RBX2 * kappa).atan().cos() * c.LXAL;
    const double cxa = c.RCX1;
    const Lane_T exa_raw = c.REX1 + (c.REX2 * load.dfz);
    const Lane_T exa = limit_curvature(exa_raw);
    const Lane_T shxa_lane = Lane_T::Constant(n, shxa);
    const Lane_T numerator = cosine_magic_formula(bxa, cxa, exa, alpha_s);
    const Lane_T denominator = cosine_magic_formula(bxa, cxa, exa, shxa_lane);
    return numerator / denominator;
  }

  /// Combined-slip weighting function Gyk of Fy. The denominator is not guarded.
  template <typename Lane_T>
  [[nodiscard]] Lane_T gyk(const mf5x_coefficients &c, const load_terms<Lane_T> &load, const Lane_T &kappa,
                           const Lane_T &alpha_rad) noexcept {
    const Lane_T shyk = c.RHY1 + (c.RHY2 * load.dfz);
    const Lane_T kappa_s = kappa + shyk;
    const Lane_T byk = c.RBY1 * (c.RBY2 * (alpha_rad - c.RBY3)).atan().cos() * c.LYKA;
    const double cyk = c.RCY1;
    const Lane_T eyk_raw = c.REY1 + (c.REY2 * load.dfz);
    const Lane_T eyk = limit_curvature(eyk_raw);
    const Lane_T numerator = cosine_magic_formula(byk, cyk, eyk, kappa_s);
    const Lane_T denominator = cosine_magic_formula(byk, cyk, eyk, shyk);
    return numerator / denominator;
  }

  /// Kappa-induced side force SVyk = DVyk*sin(RVY5*atan(RVY6*kappa))*LVYKA with
  /// DVyk = mu_y*Fz*(RVY1 + RVY2*dfz + RVY3*gamma)*cos(atan(RVY4*alpha)), raw camber. `mu_y_value` is a parameter
  /// because MF 5.2 uses the zero-camber mu_y.
  template <typename Lane_T>
  [[nodiscard]] Lane_T svyk(const mf5x_coefficients &c, const load_terms<Lane_T> &load, const Lane_T &mu_y_value,
                            const Lane_T &fz_n, const Lane_T &kappa, const Lane_T &alpha_rad,
                            const Lane_T &gamma_rad) noexcept {
    const Lane_T load_camber = c.RVY1 + (c.RVY2 * load.dfz) + (c.RVY3 * gamma_rad);
    const Lane_T dvyk = mu_y_value * fz_n * load_camber * (c.RVY4 * alpha_rad).atan().cos();
    return dvyk * (c.RVY5 * (c.RVY6 * kappa).atan()).sin() * c.LVYKA;
  }

  /// Cosine-method combined forces Fx = Fx0*Gxa and Fy = Fy0*Gyk + SVyk; shared by PAC2002 and MF 5.2.
  template <typename Lane_T>
  [[nodiscard]] combined_forces<Lane_T> cosine_combined_forces(const mf5x_coefficients &c,
                                                               const slip_state<Lane_T> &state,
                                                               const input_lanes<Lane_T> &p) noexcept {
    const Lane_T svyk_n = svyk(c, state.load, state.lat.mu_y, p.fz_n, p.kappa, p.alpha_rad, p.gamma_rad);
    const Lane_T weight_x = gxa(c, state.load, p.kappa, p.alpha_rad);
    const Lane_T weight_y = gyk(c, state.load, p.kappa, p.alpha_rad);
    const Lane_T fx_n = state.lon.fx0_n * weight_x;
    const Lane_T fy_n = (state.lat.fy0_n * weight_y) + svyk_n;
    return combined_forces<Lane_T>{.fx_n = fx_n, .fy_n = fy_n, .svyk_n = svyk_n};
  }

  /// Friction-ellipse ratios (PAC2002 only) in the Fz-free form with |mu_act| (sign-safe):
  /// rx = |kc|*mym/sqrt(kc^2*mym^2 + mxa^2*a*^2), ry = |a*|*mxm/sqrt(kc^2*mya^2 + a*^2*mxm^2), with
  /// kc = kappa + SHx + SVx/add_epsilon(Kx) and a* = sin(alpha + SHy + SVy/add_epsilon(Ky)). A 0/0 ratio gives 1.
  template <typename Lane_T>
  [[nodiscard]] ellipse_ratios<Lane_T> friction_ellipse_ratios(const slip_state<Lane_T> &state, const Lane_T &kappa,
                                                               const Lane_T &alpha_rad) noexcept {
    const Eigen::Index n = kappa.size();
    const Lane_T kappa_c = kappa + state.lon.shx + (state.lon.svx_n / add_epsilon(state.lon.kx_n));
    const Lane_T alpha_c = alpha_rad + state.lat.shy + (state.lat.svy_n / add_epsilon(state.lat.ky_n));
    const Lane_T alpha_star = alpha_c.sin();
    // Fz-free magnitudes (the 1/Fz factors of the printed form cancel): mu_act*Fz and mu_max*Fz.
    const Lane_T mxa = (state.lon.fx0_n - state.lon.svx_n).abs();
    const Lane_T mya = (state.lat.fy0_n - state.lat.svy_n).abs();
    const Lane_T mxm = state.lon.dx_n.abs();
    const Lane_T mym = state.lat.dy_n.abs();
    const Lane_T num_x = kappa_c.abs() * mym;
    const Lane_T den_x = ((kappa_c * kappa_c * mym * mym) + (mxa * mxa * alpha_star * alpha_star)).sqrt();
    const Lane_T num_y = alpha_star.abs() * mxm;
    const Lane_T den_y = ((kappa_c * kappa_c * mya * mya) + (alpha_star * alpha_star * mxm * mxm)).sqrt();
    // A 0/0 ratio (both slips at the shifted origin) gives 1.
    const Lane_T ratio_x = (den_x > 0.0).select(num_x / den_x, Lane_T::Constant(n, 1.0));
    const Lane_T ratio_y = (den_y > 0.0).select(num_y / den_y, Lane_T::Constant(n, 1.0));
    return ellipse_ratios<Lane_T>{.rx = ratio_x, .ry = ratio_y};
  }

  /// Moment arm of Fx in Mz: s = R0*(SSZ1 + SSZ2*Fy/FNOMIN + (SSZ3 + SSZ4*dfz)*gamma)*LS, with the FNOMIN normaliser,
  /// raw camber and the combined Fy.
  template <typename Lane_T>
  [[nodiscard]] Lane_T s(const mf5x_coefficients &c, const load_terms<Lane_T> &load, const Lane_T &fy_n,
                         const Lane_T &gamma_rad) noexcept {
    const Lane_T fy_term = c.SSZ2 * fy_n / c.FNOMIN;
    const Lane_T camber_term = (c.SSZ3 + (c.SSZ4 * load.dfz)) * gamma_rad;
    return c.UNLOADED_RADIUS * (c.SSZ1 + fy_term + camber_term) * c.LS;
  }

  /// Overturning moment Mx = R0*Fz*(QSX1*LVMX - QSX2*gamma + QSX3*Fy/FNOMIN)*LMX.
  template <typename Lane_T>
  [[nodiscard]] Lane_T mx(const mf5x_coefficients &c, const Lane_T &fz_n, const Lane_T &fy_n,
                          const Lane_T &gamma_rad) noexcept {
    const Lane_T bracket = (c.QSX1 * c.LVMX) - (c.QSX2 * gamma_rad) + (c.QSX3 * fy_n / c.FNOMIN);
    return c.UNLOADED_RADIUS * fz_n * bracket * c.LMX;
  }

  /// Output rolling-resistance moment My = -R0*Fz*(QSY1 + QSY2*Fx/FNOMIN + QSY3*|Vx/LONGVL| + QSY4*(Vx/LONGVL)^4)*LMY
  /// (ISO sign: negative when rolling forward); shared by PAC2002 and MF 5.2.
  template <typename Lane_T>
  [[nodiscard]] Lane_T my(const mf5x_coefficients &c, const Lane_T &fz_n, const Lane_T &fx_n,
                          const Lane_T &vx_mps) noexcept {
    const Lane_T speed_ratio = vx_mps / c.LONGVL;
    const Lane_T speed_squared = speed_ratio * speed_ratio;
    const Lane_T fx_term = c.QSY2 * fx_n / c.FNOMIN;
    const Lane_T speed_term = (c.QSY3 * speed_ratio.abs()) + (c.QSY4 * speed_squared * speed_squared);
    return -(c.UNLOADED_RADIUS * fz_n * (c.QSY1 + fx_term + speed_term) * c.LMY);
  }

  /// Output rolling-resistance moment from the longitudinal shifts, My = -sgn(FNOMIN)*R0*(SVx + Kx*SHx), without LMY.
  /// PAC2002 uses it when FITTYP = 5 and QSY1 = QSY2 = 0.
  template <typename Lane_T>
  [[nodiscard]] Lane_T my_from_shifts(const mf5x_coefficients &c, const longitudinal_terms<Lane_T> &lon) noexcept {
    return -(std::copysign(c.UNLOADED_RADIUS, c.FNOMIN) * (lon.svx_n + (lon.kx_n * lon.shx)));
  }

  /// Cosine factor of the trail: cos(alpha) for `COS_ALPHA`, cos'(alpha) = Vx/(hypot(Vx, tan(alpha)*Vx) + eps) for
  /// `COS_PRIME`.
  template <typename Lane_T>
  [[nodiscard]] Lane_T trail_cosine(trail_cosine_form form, const input_lanes<Lane_T> &p) noexcept {
    if (form == trail_cosine_form::COS_ALPHA) {
      return p.alpha_rad.cos();
    }
    const Lane_T tan_alpha_vx = p.alpha_rad.tan() * p.vx_mps;
    const Lane_T vc = elementwise_hypot(p.vx_mps, tan_alpha_vx);
    return p.vx_mps / (vc + EPSILON);
  }

  /// Combined forces: the friction ellipse (Fx = Fx0*rx, Fy = Fy0*ry, SVyk = 0) or the cosine method.
  template <typename Lane_T>
  [[nodiscard]] combined_forces<Lane_T> combine(const mf5x_coefficients &c, combined_method method,
                                                const slip_state<Lane_T> &state,
                                                const input_lanes<Lane_T> &p) noexcept {
    if (method == combined_method::FRICTION_ELLIPSE) {
      const ellipse_ratios<Lane_T> ratios = friction_ellipse_ratios(state, p.kappa, p.alpha_rad);
      const Lane_T fx_n = state.lon.fx0_n * ratios.rx;
      const Lane_T fy_n = state.lat.fy0_n * ratios.ry;
      const Lane_T zero_svyk = Lane_T::Zero(p.fz_n.size());
      return combined_forces<Lane_T>{.fx_n = fx_n, .fy_n = fy_n, .svyk_n = zero_svyk};
    }
    return cosine_combined_forces(c, state, p);
  }

  /// F'y, the force that multiplies the trail in Mz: Fy - SVyk, or Fy - SVyk0 with SVyk0 using mu_y at gamma = 0.
  template <typename Lane_T>
  [[nodiscard]] Lane_T fy_prime(const mf5x_coefficients &c, fy_prime_form form, const slip_state<Lane_T> &state,
                                const combined_forces<Lane_T> &forces, const input_lanes<Lane_T> &p) noexcept {
    if (form == fy_prime_form::SVYK) {
      return forces.fy_n - forces.svyk_n;
    }
    const Lane_T zero_camber = Lane_T::Zero(p.fz_n.size());
    const Lane_T mu_zero_camber = mu_y(c, state.load, zero_camber);
    const Lane_T svyk0_n = svyk(c, state.load, mu_zero_camber, p.fz_n, p.kappa, p.alpha_rad, p.gamma_rad);
    return forces.fy_n - svyk0_n;
  }

  /// Output rolling-resistance moment: from the longitudinal shifts for `SHIFTS`, otherwise the QSY form.
  template <typename Lane_T>
  [[nodiscard]] Lane_T rolling_resistance(const mf5x_coefficients &c, rolling_resistance_form form,
                                          const slip_state<Lane_T> &state, const Lane_T &fx_n,
                                          const input_lanes<Lane_T> &p) noexcept {
    if (form == rolling_resistance_form::SHIFTS) {
      return my_from_shifts(c, state.lon);
    }
    return my(c, p.fz_n, fx_n, p.vx_mps);
  }

  /// slip state, then UNCOMBINED (Fx0, Fy0, Mz0) or COMBINED (combine, alpha_eq, Mz with s*Fx), then Mx, My.
  template <typename Lane_T>
  [[nodiscard]] force_lanes<Lane_T> mf5x_evaluate(const mf5x_coefficients &c, const mf5x_forms &forms, slip_mode mode,
                                                  const input_lanes<Lane_T> &p) noexcept {
    const slip_state<Lane_T> state = evaluate_slip_state(c, p);
    const longitudinal_terms<Lane_T> &lon = state.lon;
    const lateral_terms<Lane_T> &lat = state.lat;
    const aligning_terms<Lane_T> &aln = state.aln;
    const Lane_T cos_trail = trail_cosine(forms.trail_cosine, p);
    const Lane_T cos_alpha = p.alpha_rad.cos();

    force_lanes<Lane_T> out;
    if (mode == slip_mode::UNCOMBINED) {
      // Pure-slip forces and Mz0 = -t(alpha_t)*Fy0 + Mzr(alpha_r).
      out.fx_n = lon.fx0_n;
      out.fy_n = lat.fy0_n;
      const Lane_T trail_m = t(aln, aln.alpha_t, cos_trail);
      const Lane_T residual_nm = mzr(aln, aln.alpha_r, cos_alpha);
      out.mz_nm = -(trail_m * out.fy_n) + residual_nm;
    } else {
      const combined_forces<Lane_T> forces = combine(c, forms.combined, state, p);
      const Lane_T ky_guarded_n = add_epsilon(lat.ky_n);
      const Lane_T ate = alpha_eq(aln.alpha_t, p.kappa, lon.kx_n, ky_guarded_n);
      const Lane_T are = alpha_eq(aln.alpha_r, p.kappa, lon.kx_n, ky_guarded_n);
      const Lane_T trail_m = t(aln, ate, cos_trail);
      const Lane_T fy_prime_n = fy_prime(c, forms.fy_prime, state, forces, p);
      const Lane_T arm_m = s(c, state.load, forces.fy_n, p.gamma_rad);
      const Lane_T residual_nm = mzr(aln, are, cos_alpha);
      out.fx_n = forces.fx_n;
      out.fy_n = forces.fy_n;
      out.mz_nm = -(trail_m * fy_prime_n) + residual_nm + (arm_m * forces.fx_n);
    }
    out.mx_nm = mx(c, p.fz_n, out.fy_n, p.gamma_rad);
    out.my_nm = rolling_resistance(c, forms.rolling_resistance, state, out.fx_n, p);
    return out;
  }

  /// The inputs after range clamping, with the contact mask and the 0.0/1.0 clamp flags.
  template <typename Lane_T> struct conditioned_lanes {
    /// Clamped Fz, SA, SR, IA and the untouched Vx.
    input_lanes<Lane_T> point;
    /// True where the point has a vertical load on the side of FNOMIN: raw Fz and clamped Fz both on that side.
    mask_of<Lane_T> contact;
    /// 1.0 where Fz was moved to FZMIN/FZMAX (0.0 for a point without raw contact).
    Lane_T fz_clamped;
    /// 1.0 where SR was moved to KPUMIN/KPUMAX (0.0 for a point without raw contact).
    Lane_T kappa_clamped;
    /// 1.0 where SA was moved to ALPMIN/ALPMAX (0.0 for a point without raw contact).
    Lane_T alpha_clamped;
    /// 1.0 where IA was moved to CAMMIN/CAMMAX (0.0 for a point without raw contact).
    Lane_T gamma_clamped;
    /// 1.0 where Vx < VXLOW.
    Lane_T below_vxlow;
  };

  /// The outputs of the evaluation, one value per point, in output index order.
  template <typename Lane_T> struct output_lanes {
    /// Longitudinal force Fx [N].
    Lane_T fx_n;
    /// Lateral force Fy [N].
    Lane_T fy_n;
    /// Self-aligning moment Mz [N*m].
    Lane_T mz_nm;
    /// Overturning moment Mx [N*m].
    Lane_T mx_nm;
    /// Rolling-resistance moment My [N*m].
    Lane_T my_nm;
    /// 1.0 where Fz was clamped.
    Lane_T fz_clamped;
    /// 1.0 where SR was clamped.
    Lane_T kappa_clamped;
    /// 1.0 where SA was clamped.
    Lane_T alpha_clamped;
    /// 1.0 where IA was clamped.
    Lane_T gamma_clamped;
    /// 1.0 where Vx < VXLOW.
    Lane_T below_vxlow;
  };

  /// Branch-free input conditioning: below_vxlow = to_flag(vx < VXLOW); raw_contact = fz*load_sign > 0; Fz, SR, SA and
  /// IA are clamped to their limits (infinite bounds never move a value) and each clamp flag is masked by
  /// raw_contact; contact additionally needs the clamped fz*load_sign > 0; load_sign = +1 or -1 is the sign of
  /// FNOMIN. Vx is never modified.
  template <typename Lane_T>
  [[nodiscard]] conditioned_lanes<Lane_T> mf5x_condition(const Eigen::Ref<const Eigen::VectorXd> &limits,
                                                         const input_lanes<Lane_T> &raw, double load_sign) noexcept {
    const Lane_T zero = Lane_T::Zero(raw.fz_n.size());
    const mask_of<Lane_T> raw_contact = (raw.fz_n * load_sign) > 0.0;
    Lane_T fz_moved;
    Lane_T kappa_moved;
    Lane_T alpha_moved;
    Lane_T gamma_moved;
    const Lane_T fz = clamp_to_limits(raw.fz_n, limits(mf5x::FZMIN), limits(mf5x::FZMAX), fz_moved);
    const Lane_T kappa = clamp_to_limits(raw.kappa, limits(mf5x::KPUMIN), limits(mf5x::KPUMAX), kappa_moved);
    const Lane_T alpha = clamp_to_limits(raw.alpha_rad, limits(mf5x::ALPMIN), limits(mf5x::ALPMAX), alpha_moved);
    const Lane_T gamma = clamp_to_limits(raw.gamma_rad, limits(mf5x::CAMMIN), limits(mf5x::CAMMAX), gamma_moved);
    const mask_of<Lane_T> clamped_contact = (fz * load_sign) > 0.0;
    conditioned_lanes<Lane_T> out;
    out.point.fz_n = fz;
    out.point.alpha_rad = alpha;
    out.point.kappa = kappa;
    out.point.gamma_rad = gamma;
    out.point.vx_mps = raw.vx_mps;
    out.contact = raw_contact && clamped_contact;
    out.fz_clamped = raw_contact.select(fz_moved, zero);
    out.kappa_clamped = raw_contact.select(kappa_moved, zero);
    out.alpha_clamped = raw_contact.select(alpha_moved, zero);
    out.gamma_clamped = raw_contact.select(gamma_moved, zero);
    out.below_vxlow = to_flag<Lane_T>(raw.vx_mps < limits(mf5x::VXLOW));
    return out;
  }

  /// `mf5x_condition`, then `mf5x_evaluate` on the conditioned point; the forces are `contact.select(force, 0.0)`
  /// (exact +0.0 for points without contact) and the flags are copied from the conditioning. Inputs are never
  /// modified.
  template <typename Lane_T>
  [[nodiscard]] output_lanes<Lane_T> mf5x_pipeline(const mf5x_coefficients &c, const mf5x_forms &forms, slip_mode mode,
                                                   const Eigen::Ref<const Eigen::VectorXd> &limits,
                                                   const input_lanes<Lane_T> &raw) noexcept {
    const conditioned_lanes<Lane_T> cond = mf5x_condition<Lane_T>(limits, raw, std::copysign(1.0, c.FNOMIN));
    const force_lanes<Lane_T> f = mf5x_evaluate(c, forms, mode, cond.point);
    const Lane_T zero = Lane_T::Zero(raw.fz_n.size());
    output_lanes<Lane_T> out;
    out.fx_n = cond.contact.select(f.fx_n, zero);
    out.fy_n = cond.contact.select(f.fy_n, zero);
    out.mz_nm = cond.contact.select(f.mz_nm, zero);
    out.mx_nm = cond.contact.select(f.mx_nm, zero);
    out.my_nm = cond.contact.select(f.my_nm, zero);
    out.fz_clamped = cond.fz_clamped;
    out.kappa_clamped = cond.kappa_clamped;
    out.alpha_clamped = cond.alpha_clamped;
    out.gamma_clamped = cond.gamma_clamped;
    out.below_vxlow = cond.below_vxlow;
    return out;
  }
} // namespace mf::detail
