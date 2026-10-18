#pragma once

#include <algorithm>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>

/// Pure kernels of the MF 5.x equations shared by PAC2002 and MF 5.2 (internal, not API). Equation numbers "(n)" are
/// those of the MSC "Using the PAC2002 Tire Model" manual; the evaluation order and the readings of printed defects
/// (C1-C10, D-xx, AD-xx) are in docs/research/pac2002.md sections 2-5, decisions.md and, for the MF 5.2 differences
/// ([impl] tier), docs/research/mf52.md section 4. Camber always enters as gamma*LGAx; MF 5.2 has LGA* = 1 (AD-19), so
/// the same code yields the raw camber there. All functions are total and never throw.
namespace mf::detail {
  /// Guard added to denominators that can reach zero (conventions.md section 7, AD-03).
  inline constexpr double EPSILON = 1e-6;

  /// sgn+ : +1 for x >= 0 (including +0.0 and -0.0), -1 otherwise (conventions.md section 7, AD-03).
  [[nodiscard]] constexpr double sgn_plus(double x) noexcept {
    return x >= 0.0 ? 1.0 : -1.0;
  }

  /// d + EPSILON*sgn+(d): moves a denominator away from zero without changing its sign (AD-03).
  [[nodiscard]] constexpr double add_epsilon(double d) noexcept {
    return d + (EPSILON * sgn_plus(d));
  }

  /// Curvature factors are limited to E <= 1 (pac2002.md section 3 eqs (24), (36), (53), (64), (73); D-17).
  [[nodiscard]] constexpr double limit_curvature(double e) noexcept {
    return std::min(e, 1.0);
  }

  /// D*sin(C*atan(B*x - E*(B*x - atan(B*x)))): the sine-form Magic Formula of eqs (18) and (30).
  [[nodiscard]] double sine_magic_formula(double b, double c, double d, double e, double x) noexcept;

  /// cos(C*atan(B*x - E*(B*x - atan(B*x)))): the cosine-form Magic Formula of eqs (44), (66) and (77).
  [[nodiscard]] double cosine_magic_formula(double b, double c, double e, double x) noexcept;

  /// Vertical-load terms, eq (16).
  struct load_terms {
    /// F'z0 = FNOMIN*LFZO [N].
    double fz0_prime_n = 0.0;
    /// dfz = (Fz - F'z0)/F'z0 [-].
    double dfz = 0.0;
  };

  /// Pure longitudinal slip terms, eqs (18)-(28).
  struct longitudinal_terms {
    /// kx = kappa + SHx, eq (19) [-].
    double kappa_x = 0.0;
    /// gx = gamma*LGAX, eq (20) [rad].
    double gamma_x = 0.0;
    /// Cx = PCX1*LCX, eq (21) [-].
    double cx = 0.0;
    /// mu_x, eq (23) [-].
    double mu_x = 0.0;
    /// Dx = mu_x*Fz, eq (22) [N].
    double dx_n = 0.0;
    /// Ex limited to <= 1, eq (24) [-].
    double ex = 0.0;
    /// Kx, eq (25) [N].
    double kx_n = 0.0;
    /// Bx = Kx/(Cx*Dx + eps), eq (26) [-].
    double bx = 0.0;
    /// SHx, eq (27) [-].
    double shx = 0.0;
    /// SVx, eq (28) [N].
    double svx_n = 0.0;
    /// Fx0, eq (18) with SVx outside the sine (C2) [N].
    double fx0_n = 0.0;
  };

  /// Pure lateral slip terms, eqs (30)-(41).
  struct lateral_terms {
    /// ay = alpha + SHy, eq (31) [rad].
    double alpha_y = 0.0;
    /// gy = gamma*LGAY, eq (32) [rad].
    double gamma_y = 0.0;
    /// Cy = PCY1*LCY, eq (33) [-].
    double cy = 0.0;
    /// mu_y, eq (35) [-].
    double mu_y = 0.0;
    /// Dy = mu_y*Fz, eq (34) [N].
    double dy_n = 0.0;
    /// Ey limited to <= 1, eq (36) [-].
    double ey = 0.0;
    /// Ky0, eq (37) [N/rad].
    double ky0_n = 0.0;
    /// Ky = Ky0*(1 - PKY3*|gy|), eq (38) [N/rad].
    double ky_n = 0.0;
    /// By = Ky/(Cy*Dy + eps), eq (39) [1/rad].
    double by = 0.0;
    /// SHy, eq (40) [rad].
    double shy = 0.0;
    /// SVy, eq (41) [N].
    double svy_n = 0.0;
    /// Fy0, eq (30) [N].
    double fy0_n = 0.0;
  };

  /// Aligning-moment terms, eqs (45)-(56).
  struct aligning_terms {
    /// gz = gamma*LGAZ, eq (49) [rad].
    double gamma_z = 0.0;
    /// at = alpha + SHt, eq (45) [rad].
    double alpha_t = 0.0;
    /// ar = alpha + SHf, eq (47) [rad].
    double alpha_r = 0.0;
    /// SHt, eq (54) [-].
    double sht = 0.0;
    /// SHf = SHy + SVy/(Ky + eps), eq (48) [rad].
    double shf = 0.0;
    /// Bt, eq (50) [1/rad].
    double bt = 0.0;
    /// Ct = QCZ1, eq (51) [-].
    double ct = 0.0;
    /// Dt with the R0/FNOMIN normaliser, eq (52), D-23, AD-18 [m].
    double dt_m = 0.0;
    /// Et limited to <= 1, eq (53) [-].
    double et = 0.0;
    /// Br, eq (55) [1/rad].
    double br = 0.0;
    /// Cr = 1, pac2002.md section 3 [-].
    double cr = 0.0;
    /// Dr, eq (56) [N*m].
    double dr_nm = 0.0;
  };

  /// All pure-slip terms of one operating point.
  struct slip_state {
    /// Vertical-load terms.
    load_terms load;
    /// Longitudinal terms.
    longitudinal_terms lon;
    /// Lateral terms.
    lateral_terms lat;
    /// Aligning terms.
    aligning_terms aln;
  };

  /// Combined-slip forces (and the kappa-induced side force that is part of Fy).
  struct combined_forces {
    /// Combined Fx [N].
    double fx_n = 0.0;
    /// Combined Fy [N].
    double fy_n = 0.0;
    /// SVyk contained in `fy_n`; 0 for the friction-ellipse method [N].
    double svyk_n = 0.0;
  };

  /// Friction-ellipse reduction ratios Fx/Fx0 and Fy/Fy0 (pac2002.md section 5.4).
  struct ellipse_ratios {
    /// Fx/Fx0, 1 when the ratio is 0/0.
    double rx = 1.0;
    /// Fy/Fy0, 1 when the ratio is 0/0.
    double ry = 1.0;
  };

  /// F'z0 = FNOMIN*LFZO and dfz = (Fz - F'z0)/F'z0, eq (16), pac2002.md section 3.
  [[nodiscard]] load_terms vertical_load(const mf5x_coefficients &c, double fz_n) noexcept;

  /// Lateral peak friction mu_y = (PDY1 + PDY2*dfz)*(1 - PDY3*gy^2)*LMUY, eq (35); `gamma_y` is the scaled camber
  /// gy (a different value gives the gamma = 0 form that MF 5.2 uses for SVyk0, AD-25).
  [[nodiscard]] double lateral_friction(const mf5x_coefficients &c, const load_terms &load, double gamma_y) noexcept;

  /// Pure longitudinal force Fx0 with all intermediate terms, eqs (18)-(28), C2 and C3, D-17, AD-03
  /// (pac2002.md section 3).
  [[nodiscard]] longitudinal_terms pure_longitudinal(const mf5x_coefficients &c, const load_terms &load, double fz_n,
                                                     double kappa, double gamma_rad) noexcept;

  /// Pure lateral force Fy0 with all intermediate terms, eqs (30)-(41), C9, D-17, AD-03 (pac2002.md section 3). Ky0
  /// uses the derived form PKY1*F'z0*sin(2*atan(Fz/(PKY2*F'z0)))*LKY of eq (37).
  [[nodiscard]] lateral_terms pure_lateral(const mf5x_coefficients &c, const load_terms &load, double fz_n,
                                           double alpha_rad, double gamma_rad) noexcept;

  /// Aligning terms, eqs (45)-(56), C10, D-23, D-24, AD-03, AD-18 (pac2002.md section 3). SHf = SHy +
  /// SVy/add_epsilon(Ky) (eq (48); the MF 5.2 [impl] reading is identical, mf52.md section 4 step 6). Et uses the pure
  /// alpha_t.
  [[nodiscard]] aligning_terms aligning(const mf5x_coefficients &c, const load_terms &load,
                                        const lateral_terms &lateral, double fz_n, double alpha_rad,
                                        double gamma_rad) noexcept;

  /// Steps 3-6 of the evaluation order (pac2002.md section 2, mf52.md section 4): load, pure Fx0, pure Fy0 and the
  /// aligning terms of `point`, which must be validated and clamped.
  [[nodiscard]] slip_state evaluate_slip_state(const mf5x_coefficients &c, const operating_point &point) noexcept;

  /// Pneumatic trail t = Dt*cos(Ct*atan(Bt*x - Et*(Bt*x - atan(Bt*x))))*cos_factor, eqs (44) and (79). `x` is alpha_t
  /// (pure) or the equivalent slip angle ate (combined); `cos_factor` is cos(alpha) for PAC2002 and cos'(alpha) for
  /// MF 5.2 (conventions.md section 6).
  [[nodiscard]] double trail(const aligning_terms &aln, double x, double cos_factor) noexcept;

  /// Residual moment Mzr = Dr*cos(Cr*atan(Br*x))*cos_alpha, eqs (46) and (82), C4. `x` is alpha_r (pure) or the
  /// equivalent slip angle are (combined).
  [[nodiscard]] double residual_moment(const aligning_terms &aln, double x, double cos_alpha) noexcept;

  /// Equivalent slip angle atan(sqrt(tan^2(a) + (kx/ky)^2*kappa^2))*sgn+(a), eqs (84)-(85) with the sign moved outside
  /// the root (C5, D-05). `ky_guarded_n` is add_epsilon(Ky) (AD-03).
  [[nodiscard]] double equivalent_slip_angle(double a, double kappa, double kx_n, double ky_guarded_n) noexcept;

  /// Combined-slip weighting of Fx, Gxa, eqs (60)-(66) (pac2002.md section 3). The denominator is unguarded (AD-23).
  [[nodiscard]] double longitudinal_weighting(const mf5x_coefficients &c, const load_terms &load, double kappa,
                                              double alpha_rad) noexcept;

  /// Combined-slip weighting of Fy, Gyk, eqs (69)-(74) and (77) (pac2002.md section 3). The denominator is unguarded
  /// (AD-23).
  [[nodiscard]] double lateral_weighting(const mf5x_coefficients &c, const load_terms &load, double kappa,
                                         double alpha_rad) noexcept;

  /// Kappa-induced side force SVyk = DVyk*sin(RVY5*atan(RVY6*kappa))*LVYKA with
  /// DVyk = mu_y*Fz*(RVY1 + RVY2*dfz + RVY3*gamma)*cos(atan(RVY4*alpha)), eqs (75)-(76), raw camber. `mu_y` is passed
  /// in because MF 5.2 evaluates SVyk0 with mu_y at gamma = 0 (AD-25).
  [[nodiscard]] double kappa_induced_side_force(const mf5x_coefficients &c, const load_terms &load, double mu_y,
                                                double fz_n, double kappa, double alpha_rad, double gamma_rad) noexcept;

  /// Cosine-method combined forces Fx = Fx0*Gxa (58) and Fy = Fy0*Gyk + SVyk (67), pac2002.md section 3. Also used
  /// by MF 5.2 (mf52.md section 4 step 7).
  [[nodiscard]] combined_forces cosine_combined_forces(const mf5x_coefficients &c, const slip_state &s,
                                                       const operating_point &p) noexcept;

  /// Friction-ellipse ratios (PAC2002 only, MSC p.33-34; pac2002.md section 5.4, D-06, AD-24). The printed form is
  /// rewritten without Fz and with |mu_act| (identical, and sign-safe):
  /// rx = |kc|*mym/sqrt(kc^2*mym^2 + mxa^2*a*^2), ry = |a*|*mxm/sqrt(kc^2*mya^2 + a*^2*mxm^2), with
  /// kc = kappa + SHx + SVx/add_epsilon(Kx) and a* = sin(alpha + SHy + SVy/add_epsilon(Ky)). A 0/0 ratio gives 1.
  [[nodiscard]] ellipse_ratios friction_ellipse_ratios(const slip_state &s, double kappa, double alpha_rad) noexcept;

  /// Moment arm of Fx in Mz: s = R0*(SSZ1 + SSZ2*Fy/FNOMIN + (SSZ3 + SSZ4*dfz)*gamma)*LS (not printed by MSC;
  /// pac2002.md section 5.1, D-09, AD-04: FNOMIN normaliser, raw camber, combined Fy; MF 5.2 mf52.md section 4 step 8).
  [[nodiscard]] double moment_arm(const mf5x_coefficients &c, const load_terms &load, double fy_n,
                                  double gamma_rad) noexcept;

  /// Overturning moment Mx = R0*Fz*(QSX1*LVMX - QSX2*gamma + QSX3*Fy/FNOMIN)*LMX, eq (86); MF 5.2 mf52.md section 4
  /// step 9.
  [[nodiscard]] double overturning_moment(const mf5x_coefficients &c, double fz_n, double fy_n,
                                          double gamma_rad) noexcept;

  /// Output rolling-resistance moment My = -R0*Fz*(QSY1 + QSY2*Fx/FNOMIN + QSY3*|Vx/LONGVL| + QSY4*(Vx/LONGVL)^4)*LMY,
  /// the negative of eq (87) (C6, C7, D-11, AD-17; ISO sign, conventions.md section 4). This is also the MF 5.2 form
  /// (mf52.md section 4 step 9).
  [[nodiscard]] double rolling_resistance_moment(const mf5x_coefficients &c, double fz_n, double fx_n,
                                                 double vx_mps) noexcept;

  /// Output rolling-resistance moment -R0*(SVx + Kx*SHx), the negative of eq (88) as printed, without LMY (PAC2002
  /// with FITTYP 5 and QSY1 = QSY2 = 0; AD-17).
  [[nodiscard]] double rolling_resistance_moment_eq88(const mf5x_coefficients &c,
                                                      const longitudinal_terms &lon) noexcept;
} // namespace mf::detail
