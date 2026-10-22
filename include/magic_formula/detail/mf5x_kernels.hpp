#pragma once

#include <algorithm>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>

/// Pure kernels of the MF 5.x equations shared by PAC2002 and MF 5.2 (internal, not API). Camber always enters as
/// gamma*LGAx; MF 5.2 forces LGA* = 1, so the same code yields the raw camber there. All functions are total and never
/// throw. Equations: docs/research/pac2002.md.
namespace mf::detail {
  /// Guard added to denominators that can reach zero.
  inline constexpr double EPSILON = 1e-6;

  /// sgn+ : +1 for x >= 0 (including +0.0 and -0.0), -1 otherwise.
  [[nodiscard]] constexpr double sgn_plus(double x) noexcept {
    return x >= 0.0 ? 1.0 : -1.0;
  }

  /// d + EPSILON*sgn+(d): moves a denominator away from zero without changing its sign.
  [[nodiscard]] constexpr double add_epsilon(double d) noexcept {
    return d + (EPSILON * sgn_plus(d));
  }

  /// Limits a curvature factor to E <= 1.
  [[nodiscard]] constexpr double limit_curvature(double e) noexcept {
    return std::min(e, 1.0);
  }

  /// D*sin(C*atan(B*x - E*(B*x - atan(B*x)))): the sine-form Magic Formula.
  [[nodiscard]] double sine_magic_formula(double b, double c, double d, double e, double x) noexcept;

  /// cos(C*atan(B*x - E*(B*x - atan(B*x)))): the cosine-form Magic Formula.
  [[nodiscard]] double cosine_magic_formula(double b, double c, double e, double x) noexcept;

  /// Vertical-load terms.
  struct load_terms {
    /// F'z0 = FNOMIN*LFZO [N].
    double fz0_prime_n = 0.0;
    /// dfz = (Fz - F'z0)/F'z0 [-].
    double dfz = 0.0;
  };

  /// Pure longitudinal slip terms.
  struct longitudinal_terms {
    /// kx = kappa + SHx [-].
    double kappa_x = 0.0;
    /// gx = gamma*LGAX [rad].
    double gamma_x = 0.0;
    /// Cx = PCX1*LCX [-].
    double cx = 0.0;
    /// Longitudinal peak friction mu_x [-].
    double mu_x = 0.0;
    /// Dx = mu_x*Fz [N].
    double dx_n = 0.0;
    /// Ex limited to <= 1 [-].
    double ex = 0.0;
    /// Longitudinal slip stiffness Kx [N].
    double kx_n = 0.0;
    /// Bx = Kx/(Cx*Dx + eps) [-].
    double bx = 0.0;
    /// Horizontal shift SHx [-].
    double shx = 0.0;
    /// Vertical shift SVx [N].
    double svx_n = 0.0;
    /// Fx0 with SVx added outside the sine [N].
    double fx0_n = 0.0;
  };

  /// Pure lateral slip terms.
  struct lateral_terms {
    /// ay = alpha + SHy [rad].
    double alpha_y = 0.0;
    /// gy = gamma*LGAY [rad].
    double gamma_y = 0.0;
    /// Cy = PCY1*LCY [-].
    double cy = 0.0;
    /// Lateral peak friction mu_y [-].
    double mu_y = 0.0;
    /// Dy = mu_y*Fz [N].
    double dy_n = 0.0;
    /// Ey limited to <= 1 [-].
    double ey = 0.0;
    /// Ky0 [N/rad].
    double ky0_n = 0.0;
    /// Ky = Ky0*(1 - PKY3*|gy|) [N/rad].
    double ky_n = 0.0;
    /// By = Ky/(Cy*Dy + eps) [1/rad].
    double by = 0.0;
    /// Horizontal shift SHy [rad].
    double shy = 0.0;
    /// Vertical shift SVy [N].
    double svy_n = 0.0;
    /// Fy0 [N].
    double fy0_n = 0.0;
  };

  /// Aligning-moment terms.
  struct aligning_terms {
    /// gz = gamma*LGAZ [rad].
    double gamma_z = 0.0;
    /// at = alpha + SHt [rad].
    double alpha_t = 0.0;
    /// ar = alpha + SHf [rad].
    double alpha_r = 0.0;
    /// Trail horizontal shift SHt [-].
    double sht = 0.0;
    /// SHf = SHy + SVy/(Ky + eps) [rad].
    double shf = 0.0;
    /// Bt [1/rad].
    double bt = 0.0;
    /// Ct = QCZ1 [-].
    double ct = 0.0;
    /// Dt with the R0/FNOMIN normaliser [m].
    double dt_m = 0.0;
    /// Et limited to <= 1 [-].
    double et = 0.0;
    /// Br [1/rad].
    double br = 0.0;
    /// Cr = 1 [-].
    double cr = 0.0;
    /// Dr [N*m].
    double dr_nm = 0.0;
  };

  /// All pure-slip terms of one operating point.
  struct slip_state {
    load_terms load;
    longitudinal_terms lon;
    lateral_terms lat;
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

  /// Friction-ellipse reduction ratios Fx/Fx0 and Fy/Fy0.
  struct ellipse_ratios {
    /// Fx/Fx0, 1 when the ratio is 0/0.
    double rx = 1.0;
    /// Fy/Fy0, 1 when the ratio is 0/0.
    double ry = 1.0;
  };

  /// F'z0 = FNOMIN*LFZO and dfz = (Fz - F'z0)/F'z0.
  [[nodiscard]] load_terms vertical_load(const mf5x_coefficients &c, double fz_n) noexcept;

  /// Lateral peak friction mu_y = (PDY1 + PDY2*dfz)*(1 - PDY3*gy^2)*LMUY. `gamma_y` is the scaled camber gy; MF 5.2
  /// passes 0 for the zero-camber mu_y of SVyk0.
  [[nodiscard]] double mu_y(const mf5x_coefficients &c, const load_terms &load, double gamma_y) noexcept;

  /// Pure longitudinal force Fx0 with all intermediate terms.
  [[nodiscard]] longitudinal_terms fx0(const mf5x_coefficients &c, const load_terms &load, double fz_n, double kappa,
                                       double gamma_rad) noexcept;

  /// Pure lateral force Fy0 with all intermediate terms. Ky0 = PKY1*F'z0*sin(2*atan(Fz/(PKY2*F'z0)))*LKY.
  [[nodiscard]] lateral_terms fy0(const mf5x_coefficients &c, const load_terms &load, double fz_n, double alpha_rad,
                                  double gamma_rad) noexcept;

  /// Aligning terms. SHf = SHy + SVy/add_epsilon(Ky). Et uses the pure alpha_t.
  [[nodiscard]] aligning_terms aligning(const mf5x_coefficients &c, const load_terms &load,
                                        const lateral_terms &lateral, double fz_n, double alpha_rad,
                                        double gamma_rad) noexcept;

  /// Load, pure Fx0, pure Fy0 and the aligning terms of `point`, which must be validated and clamped.
  [[nodiscard]] slip_state evaluate_slip_state(const mf5x_coefficients &c, const operating_point &point) noexcept;

  /// Pneumatic trail t = Dt*cos(Ct*atan(Bt*x - Et*(Bt*x - atan(Bt*x))))*cos_factor. `x` is alpha_t (pure) or the
  /// equivalent slip angle ate (combined); `cos_factor` is cos(alpha) for PAC2002 and cos'(alpha) for MF 5.2.
  [[nodiscard]] double t(const aligning_terms &aln, double x, double cos_factor) noexcept;

  /// Residual moment Mzr = Dr*cos(Cr*atan(Br*x))*cos_alpha. `x` is alpha_r (pure) or the equivalent slip angle are
  /// (combined).
  [[nodiscard]] double mzr(const aligning_terms &aln, double x, double cos_alpha) noexcept;

  /// Equivalent slip angle atan(sqrt(tan^2(a) + (Kx/Ky)^2*kappa^2))*sgn+(a), with the sign outside the root.
  /// `ky_guarded_n` is add_epsilon(Ky).
  [[nodiscard]] double alpha_eq(double a, double kappa, double kx_n, double ky_guarded_n) noexcept;

  /// Combined-slip weighting function Gxa of Fx. The denominator is not guarded.
  [[nodiscard]] double gxa(const mf5x_coefficients &c, const load_terms &load, double kappa, double alpha_rad) noexcept;

  /// Combined-slip weighting function Gyk of Fy. The denominator is not guarded.
  [[nodiscard]] double gyk(const mf5x_coefficients &c, const load_terms &load, double kappa, double alpha_rad) noexcept;

  /// Kappa-induced side force SVyk = DVyk*sin(RVY5*atan(RVY6*kappa))*LVYKA with
  /// DVyk = mu_y*Fz*(RVY1 + RVY2*dfz + RVY3*gamma)*cos(atan(RVY4*alpha)), raw camber. `mu_y_value` is a parameter
  /// because MF 5.2 uses the zero-camber mu_y.
  [[nodiscard]] double svyk(const mf5x_coefficients &c, const load_terms &load, double mu_y_value, double fz_n,
                            double kappa, double alpha_rad, double gamma_rad) noexcept;

  /// Cosine-method combined forces Fx = Fx0*Gxa and Fy = Fy0*Gyk + SVyk; shared by PAC2002 and MF 5.2.
  [[nodiscard]] combined_forces cosine_combined_forces(const mf5x_coefficients &c, const slip_state &state,
                                                       const operating_point &p) noexcept;

  /// Friction-ellipse ratios (PAC2002 only) in the Fz-free form with |mu_act| (sign-safe):
  /// rx = |kc|*mym/sqrt(kc^2*mym^2 + mxa^2*a*^2), ry = |a*|*mxm/sqrt(kc^2*mya^2 + a*^2*mxm^2), with
  /// kc = kappa + SHx + SVx/add_epsilon(Kx) and a* = sin(alpha + SHy + SVy/add_epsilon(Ky)). A 0/0 ratio gives 1.
  [[nodiscard]] ellipse_ratios friction_ellipse_ratios(const slip_state &state, double kappa,
                                                       double alpha_rad) noexcept;

  /// Moment arm of Fx in Mz: s = R0*(SSZ1 + SSZ2*Fy/FNOMIN + (SSZ3 + SSZ4*dfz)*gamma)*LS, with the FNOMIN normaliser,
  /// raw camber and the combined Fy.
  [[nodiscard]] double s(const mf5x_coefficients &c, const load_terms &load, double fy_n, double gamma_rad) noexcept;

  /// Overturning moment Mx = R0*Fz*(QSX1*LVMX - QSX2*gamma + QSX3*Fy/FNOMIN)*LMX.
  [[nodiscard]] double mx(const mf5x_coefficients &c, double fz_n, double fy_n, double gamma_rad) noexcept;

  /// Output rolling-resistance moment My = -R0*Fz*(QSY1 + QSY2*Fx/FNOMIN + QSY3*|Vx/LONGVL| + QSY4*(Vx/LONGVL)^4)*LMY
  /// (ISO sign: negative when rolling forward); shared by PAC2002 and MF 5.2.
  [[nodiscard]] double my(const mf5x_coefficients &c, double fz_n, double fx_n, double vx_mps) noexcept;

  /// Output rolling-resistance moment from the longitudinal shifts, My = -R0*(SVx + Kx*SHx), without LMY. PAC2002
  /// uses it when FITTYP = 5 and QSY1 = QSY2 = 0.
  [[nodiscard]] double my_from_shifts(const mf5x_coefficients &c, const longitudinal_terms &lon) noexcept;
} // namespace mf::detail
