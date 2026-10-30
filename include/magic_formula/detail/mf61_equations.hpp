#pragma once
#include <Eigen/Core>
#include <cmath>
#include <limits>
#include <magic_formula/detail/dual_lane.hpp>
#include <magic_formula/detail/lanes.hpp>
#include <magic_formula/mf61_coefficients.hpp>
#include <numbers>
#include <type_traits>
#include <utility>
namespace mf::detail {
  /// Values of ordinary lanes, used only for piecewise decisions.
  template <typename Lane_T> [[nodiscard]] Lane_T mf61_values(const Lane_T &x) noexcept {
    return x;
  }
  /// Values of dual lanes; decisions do not differentiate their masks.
  template <typename Lane_T> [[nodiscard]] Lane_T mf61_values(const dual_lane<Lane_T> &x) noexcept {
    return x.value;
  }
  /// Square root with zero slope at zero for a norm's piecewise origin convention.
  template <typename Lane_T> [[nodiscard]] Lane_T mf61_sqrt(const Lane_T &x) noexcept {
    return x.sqrt();
  }
  template <typename Lane_T> [[nodiscard]] dual_lane<Lane_T> mf61_sqrt(const dual_lane<Lane_T> &x) noexcept {
    const Lane_T value = x.value.sqrt();
    const Lane_T zero = Lane_T::Zero(x.size());
    const Lane_T denominator = (value > 0.0).select(2.0 * value, Lane_T::Constant(x.size(), 1.0));
    const Lane_T derivative = (value > 0.0).select(x.derivative / denominator, zero);
    return {.value = value, .derivative = derivative};
  }
  /// Euclidean norm; at the origin dual lanes take the right directional derivative.
  template <typename Lane_T> [[nodiscard]] Lane_T mf61_norm(const Lane_T &x, const Lane_T &y) noexcept {
    const Lane_T sum = (x * x) + (y * y);
    return sum.sqrt();
  }
  template <typename Lane_T>
  [[nodiscard]] dual_lane<Lane_T> mf61_norm(const dual_lane<Lane_T> &x, const dual_lane<Lane_T> &y) noexcept {
    const Lane_T sum = (x.value * x.value) + (y.value * y.value);
    const Lane_T value = sum.sqrt();
    const Lane_T direction = ((x.derivative * x.derivative) + (y.derivative * y.derivative)).sqrt();
    const Lane_T guarded = (value > 0.0).select(value, Lane_T::Constant(x.size(), 1.0));
    const Lane_T regular = (x.value * x.derivative + y.value * y.derivative) / guarded;
    const Lane_T derivative = (value > 0.0).select(regular, direction);
    return {.value = value, .derivative = derivative};
  }
  /// Absolute value with the right directional slope at a zero crossing.
  template <typename Lane_T> [[nodiscard]] Lane_T mf61_abs(const Lane_T &x) noexcept {
    return x.abs();
  }
  template <typename Lane_T> [[nodiscard]] dual_lane<Lane_T> mf61_abs(const dual_lane<Lane_T> &x) noexcept {
    const Lane_T value = x.value.abs();
    const Lane_T regular = sgn_plus(x.value) * x.derivative;
    const Lane_T directional = x.derivative.abs();
    const Lane_T derivative = (x.value == 0.0).select(directional, regular);
    return {.value = value, .derivative = derivative};
  }
  /// MF 6.1 equation blocks. Lane_T is point_lane, block_lane, or their dual counterpart.
  /// Equations and source corrections are documented in mf61.md.
  template <typename Lane_T> class mf61_equations {
  public:
    using value_lane = std::remove_cvref_t<decltype(mf61_values(std::declval<Lane_T>()))>;
    mf61_equations(const mf61_coefficients &coefficients, Eigen::Index count, bool turn_slip) noexcept
        : c_(&coefficients), n_(count), turn_slip_(turn_slip) {}
    [[nodiscard]] Lane_T constant(double value) const noexcept {
      return Lane_T::Constant(n_, value);
    }
    /// dfz: MF 6.1 equation block.
    [[nodiscard]] Lane_T dfz(const Lane_T &fz_value) const noexcept {

      return (fz_value - constant(c_->FNOMIN) * constant(c_->LFZ0)) / (constant(c_->FNOMIN) * constant(c_->LFZ0));
    }
    /// dpi: MF 6.1 equation block.
    [[nodiscard]] Lane_T dpi(const Lane_T &pi_value) const noexcept {

      return (pi_value - constant(c_->NOMPRES)) / (constant(c_->NOMPRES));
    }
    /// eg: MF 6.1 equation block.
    [[nodiscard]] Lane_T eg(const Lane_T &dfz_value) const noexcept {

      return constant(c_->PECP1) * (constant(1) + constant(c_->PECP2) * dfz_value);
    }
    /// phi: MF 6.1 equation block.
    [[nodiscard]] Lane_T phi(const Lane_T &alpha_value, const Lane_T &kappa_value, const Lane_T &fz_value,
                             const Lane_T &gamma_value, const Lane_T &vx_value, const Lane_T &p_value,
                             const Lane_T &pt_value) const noexcept {
      if (!turn_slip_) {
        return constant(0.0);
      }
      Lane_T omega_value = this->omega(kappa_value, fz_value, vx_value, p_value);

      Lane_T vsy_value = -tan_lane(alpha_value) * abs_lane(vx_value);

      Lane_T v_value = sqrt_lane((vx_value * vx_value) + (vsy_value * vsy_value));

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T epsilon_value = constant(c_->PECP1) * (constant(1) + constant(c_->PECP2) * dfz_value);

      Lane_T psi_dot_value = -pt_value * v_value;

      Lane_T phi_value = (constant(1) / v_value) *
                         (psi_dot_value - (constant(1) - epsilon_value) * omega_value * sin_lane(gamma_value));
      return phi_value;
    }
    /// zeta1: MF 6.1 equation block.
    [[nodiscard]] Lane_T zeta1(const Lane_T &bxp_value, const Lane_T &pf_value) const noexcept {

      return cos_lane(atan_lane(bxp_value * constant(c_->UNLOADED_RADIUS) * pf_value));
    }
    /// zeta2: MF 6.1 equation block.
    [[nodiscard]] Lane_T zeta2(const Lane_T &byp_value, const Lane_T &pf_value) const noexcept {
      const Lane_T spin = constant(c_->UNLOADED_RADIUS) * abs_lane(pf_value);
      const Lane_T root = sqrt_lane(spin);
      const Lane_T square = (spin * spin) + (2.0 * c_->PDYP4 * spin * root) + ((c_->PDYP4 * c_->PDYP4) * spin);
      return constant(1.0) / sqrt_lane(constant(1.0) + (byp_value * byp_value * square));
    }

    /// zeta3: MF 6.1 equation block.
    [[nodiscard]] Lane_T zeta3(const Lane_T &pf_value) const noexcept {

      return cos_lane(atan_lane(constant(c_->PKYP1) * constant(c_->UNLOADED_RADIUS) * constant(c_->UNLOADED_RADIUS) *
                                pf_value * pf_value));
    }
    /// zeta4: MF 6.1 equation block.
    [[nodiscard]] Lane_T zeta4(const Lane_T &shyp_value, const Lane_T &svyg_value,
                               const Lane_T &kya_value) const noexcept {

      return constant(1) + shyp_value - (svyg_value / add_epsilon(kya_value));
    }
    /// zeta5: MF 6.1 equation block.
    [[nodiscard]] Lane_T zeta5(const Lane_T &pm_value) const noexcept {

      return cos_lane(atan_lane(constant(c_->QDTP1) * constant(c_->UNLOADED_RADIUS) * pm_value));
    }
    /// zeta6: MF 6.1 equation block.
    [[nodiscard]] Lane_T zeta6(const Lane_T &pm_value) const noexcept {

      return cos_lane(atan_lane(constant(c_->QBRP1) * constant(c_->UNLOADED_RADIUS) * pm_value));
    }
    /// zeta8: MF 6.1 equation block.
    [[nodiscard]] Lane_T zeta8(const Lane_T &drp_value) const noexcept {

      return constant(1) + drp_value;
    }
    /// lmux_star: MF 6.1 equation block.
    [[nodiscard]] Lane_T lmux_star(const Lane_T &alpha_value, const Lane_T &kappa_value,
                                   const Lane_T &vx_value) const noexcept {

      Lane_T vsx_value = -kappa_value * abs_lane(vx_value);
      Lane_T vsy_value = tan_lane(alpha_value) * abs_lane(vx_value);
      Lane_T vs_value = mf61_norm(vsx_value, vsy_value);
      return constant(c_->LMUX) / (constant(1) + constant(c_->LMUV) * vs_value / constant(c_->LONGVL));
    }
    /// lmuy_star: MF 6.1 equation block.
    [[nodiscard]] Lane_T lmuy_star(const Lane_T &alpha_value, const Lane_T &kappa_value,
                                   const Lane_T &vx_value) const noexcept {

      Lane_T vsx_value = -kappa_value * abs_lane(vx_value);
      Lane_T vsy_value = tan_lane(alpha_value) * abs_lane(vx_value);
      Lane_T vs_value = mf61_norm(vsx_value, vsy_value);
      return constant(c_->LMUY) / (constant(1) + constant(c_->LMUV) * vs_value / constant(c_->LONGVL));
    }
    /// lmux_prime: MF 6.1 equation block.
    [[nodiscard]] Lane_T lmux_prime(const Lane_T &alpha_value, const Lane_T &kappa_value,
                                    const Lane_T &vx_value) const noexcept {

      Lane_T lmux_star_value = this->lmux_star(alpha_value, kappa_value, vx_value);
      Lane_T amu_value = constant(1);
      return (amu_value * lmux_star_value) / (constant(1) + (amu_value - constant(1)) * lmux_star_value);
    }
    /// lmuy_prime: MF 6.1 equation block.
    [[nodiscard]] Lane_T lmuy_prime(const Lane_T &alpha_value, const Lane_T &kappa_value,
                                    const Lane_T &vx_value) const noexcept {

      Lane_T lmuy_star_value = this->lmuy_star(alpha_value, kappa_value, vx_value);
      Lane_T amu_value = constant(1);
      return (amu_value * lmuy_star_value) / (constant(1) + (amu_value - constant(1)) * lmuy_star_value);
    }
    /// bxp: MF 6.1 equation block.
    [[nodiscard]] Lane_T bxp(const Lane_T &dfz_value, const Lane_T &kappa_value) const noexcept {

      return constant(c_->PDXP1) * (constant(1) + constant(c_->PDXP2) * dfz_value) *
             cos_lane(atan_lane(constant(c_->PDXP3) * kappa_value));
    }
    /// byp: MF 6.1 equation block.
    [[nodiscard]] Lane_T byp(const Lane_T &dfz_value, const Lane_T &alpha_f_value) const noexcept {

      return constant(c_->PDYP1) * (constant(1) + constant(c_->PDYP2) * dfz_value) *
             cos_lane(atan_lane(constant(c_->PDYP3) * tan_lane(alpha_f_value)));
    }
    /// shyp: MF 6.1 equation block.
    [[nodiscard]] Lane_T shyp(const Lane_T &dhyp_value, const Lane_T &chyp_value, const Lane_T &ehyp_value,
                              const Lane_T &bhyp_value, const Lane_T &pf_value) const noexcept {

      return dhyp_value *
             sin_lane(chyp_value *
                      atan_lane((bhyp_value * constant(c_->UNLOADED_RADIUS) * pf_value) -
                                (ehyp_value * (bhyp_value * constant(c_->UNLOADED_RADIUS) * pf_value -
                                               atan_lane(bhyp_value * constant(c_->UNLOADED_RADIUS) * pf_value)))));
    }
    /// chyp: MF 6.1 equation block.
    [[nodiscard]] Lane_T chyp() const noexcept {

      return constant(c_->PHYP1);
    }
    /// dhyp: MF 6.1 equation block.
    [[nodiscard]] Lane_T dhyp(const Lane_T &dfz_value) const noexcept {

      return constant(c_->PHYP2) + (constant(c_->PHYP3) * dfz_value);
    }
    /// ehyp: MF 6.1 equation block.
    [[nodiscard]] Lane_T ehyp() const noexcept {

      return constant(c_->PHYP4);
    }
    /// bhyp: MF 6.1 equation block.
    [[nodiscard]] Lane_T bhyp(const Lane_T &kyrp0_value, const Lane_T &chyp_value, const Lane_T &dhyp_value,
                              const Lane_T &kya0_value) const noexcept {

      return kyrp0_value / add_epsilon(Lane_T(chyp_value * dhyp_value * kya0_value));
    }
    /// kyrp0: MF 6.1 equation block.
    [[nodiscard]] Lane_T kyrp0(const Lane_T &kyg0_value, const Lane_T &eg_value) const noexcept {

      return kyg0_value / add_epsilon(Lane_T(constant(1) - eg_value));
    }
    /// mzpinf: MF 6.1 equation block.
    [[nodiscard]] Lane_T mzpinf(const Lane_T &fz_value, const Lane_T &muy_value) const noexcept {

      Lane_T fz0_value = constant(c_->LFZ0) * constant(c_->FNOMIN);
      return constant(c_->QCRP1) * abs_lane(muy_value) * constant(c_->UNLOADED_RADIUS) * fz_value *
             sqrt_lane(fz_value / fz0_value) * constant(c_->LMP);
    }
    /// mzp90: MF 6.1 equation block.
    [[nodiscard]] Lane_T mzp90(const Lane_T &mzpinf_value, const Lane_T &pt_value,
                               const Lane_T &gyk_value) const noexcept {

      return mzpinf_value * (constant(2) / constant(std::numbers::pi)) *
             atan_lane(constant(c_->QCRP2) * constant(c_->UNLOADED_RADIUS) * abs_lane(pt_value)) * gyk_value;
    }
    /// drp: MF 6.1 equation block.
    [[nodiscard]] Lane_T drp(const Lane_T &ddrp_value, const Lane_T &cdrp_value, const Lane_T &bdrp_value,
                             const Lane_T &pt_value) const noexcept {

      return ddrp_value *
             sin_lane(cdrp_value * atan_lane(bdrp_value * constant(c_->UNLOADED_RADIUS) * abs_lane(pt_value)));
    }
    /// bdrp: MF 6.1 equation block.
    [[nodiscard]] Lane_T bdrp(const Lane_T &kzgr0_value, const Lane_T &cdrp_value, const Lane_T &ddrp_value,
                              const Lane_T &eg_value) const noexcept {

      return kzgr0_value / add_epsilon(Lane_T(cdrp_value * ddrp_value * (constant(1) - eg_value)));
    }
    /// cdrp: MF 6.1 equation block.
    [[nodiscard]] Lane_T cdrp() const noexcept {

      return constant(c_->QDRP1);
    }
    /// ddrp: MF 6.1 equation block.
    [[nodiscard]] Lane_T ddrp(const Lane_T &mzpinf_value, const Lane_T &cdrp_value) const noexcept {

      return mzpinf_value / add_epsilon(sin_lane(constant(0.5) * constant(std::numbers::pi) * cdrp_value));
    }
    /// kzgr0: MF 6.1 equation block.
    [[nodiscard]] Lane_T kzgr0(const Lane_T &fz_value, const Lane_T &gamma_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      return fz_value * constant(c_->UNLOADED_RADIUS) *
             (constant(c_->QDZ8) + constant(c_->QDZ9) * dfz_value +
              (constant(c_->QDZ10) + constant(c_->QDZ11) * dfz_value) * abs_lane(gamma_value)) *
             constant(c_->LKZC);
    }
    /// kappa_x: MF 6.1 equation block.
    [[nodiscard]] Lane_T kappa_x(const Lane_T &kappa_value, const Lane_T &shx_value) const noexcept {

      return kappa_value + shx_value;
    }
    /// cx: MF 6.1 equation block.
    [[nodiscard]] Lane_T cx() const noexcept {

      return (constant(c_->PCX1) * constant(c_->LCX));
    }
    /// dx: MF 6.1 equation block.
    [[nodiscard]] Lane_T dx(const Lane_T &mux_value, const Lane_T &fz_value, const Lane_T &kappa_value,
                            const Lane_T &pf_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T z1_value = constant(0.0);
      if (turn_slip_) {
        Lane_T bxp_value = this->bxp(dfz_value, kappa_value);
        z1_value = this->zeta1(bxp_value, pf_value);
      } else {
        z1_value = constant(1);
      }
      return (mux_value * fz_value * z1_value);
    }
    /// mux: MF 6.1 equation block.
    [[nodiscard]] Lane_T mux(const Lane_T &dfz_value, const Lane_T &gamma_value, const Lane_T &dpi_value,
                             const Lane_T &lmux_star_value) const noexcept {

      return (constant(c_->PDX1) + constant(c_->PDX2) * dfz_value) *
             (constant(1) + (constant(c_->PPX3) * dpi_value) + (constant(c_->PPX4) * dpi_value * dpi_value)) *
             (constant(1) - constant(c_->PDX3) * gamma_value * gamma_value) * lmux_star_value;
    }
    /// ex: MF 6.1 equation block.
    [[nodiscard]] Lane_T ex(const Lane_T &dfz_value, const Lane_T &kappa_x_value) const noexcept {

      Lane_T ex_value =
          (constant(c_->PEX1) + constant(c_->PEX2) * dfz_value + constant(c_->PEX3) * dfz_value * dfz_value) *
          (constant(1) - (constant(c_->PEX4) * sign_lane(kappa_x_value))) * constant(c_->LEX);

      return ex_value.min(1.0);
    }
    /// kxk: MF 6.1 equation block.
    [[nodiscard]] Lane_T kxk(const Lane_T &fz_value, const Lane_T &dfz_value, const Lane_T &dpi_value) const noexcept {

      return fz_value * (constant(c_->PKX1) + constant(c_->PKX2) * dfz_value) *
             exp_lane(constant(c_->PKX3) * dfz_value) *
             (constant(1) + constant(c_->PPX1) * dpi_value + constant(c_->PPX2) * dpi_value * dpi_value) *
             constant(c_->LKX);
    }
    /// bx: MF 6.1 equation block.
    [[nodiscard]] Lane_T bx(const Lane_T &kxk_value, const Lane_T &cx_value, const Lane_T &dx_value) const noexcept {

      Lane_T signdx_value = sign_lane(dx_value);
      return kxk_value / (cx_value * dx_value + (constant(1e-6) * signdx_value));
    }
    /// shx: MF 6.1 equation block.
    [[nodiscard]] Lane_T shx(const Lane_T &dfz_value) const noexcept {

      return (constant(c_->PHX1) + constant(c_->PHX2) * dfz_value) * constant(c_->LHX);
    }
    /// svx: MF 6.1 equation block.
    [[nodiscard]] Lane_T svx(const Lane_T &fz_value, const Lane_T &kappa_value, const Lane_T &lmux_prime_value,
                             const Lane_T &pf_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T z1_value = constant(0);
      if (turn_slip_) {
        Lane_T bxp_value = this->bxp(dfz_value, kappa_value);
        z1_value = this->zeta1(bxp_value, pf_value);
      } else {
        z1_value = constant(1);
      }

      return fz_value * (constant(c_->PVX1) + constant(c_->PVX2) * dfz_value) * constant(c_->LVX) * lmux_prime_value *
             z1_value;
    }
    /// fx_pure: MF 6.1 equation block.
    [[nodiscard]] Lane_T fx_pure(const Lane_T &kappa_value, const Lane_T &gamma_value, const Lane_T &fz_value,
                                 const Lane_T &pi_value, const Lane_T &vx_value,
                                 const Lane_T &pt_value) const noexcept {

      Lane_T lmux_star_value = this->lmux_star(constant(0), kappa_value, vx_value);
      Lane_T lmux_prime_value = this->lmux_prime(constant(0), kappa_value, vx_value);
      Lane_T dpi_value = this->dpi(pi_value);
      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T phi_value = this->phi(constant(0), kappa_value, fz_value, gamma_value, vx_value, pi_value, pt_value);
      Lane_T shx_value = this->shx(dfz_value);
      Lane_T kappa_x_value = this->kappa_x(kappa_value, shx_value);
      Lane_T cx_value = this->cx();
      Lane_T mux_value = this->mux(dfz_value, gamma_value, dpi_value, lmux_star_value);
      Lane_T dx_value = this->dx(mux_value, fz_value, kappa_value, phi_value);
      Lane_T ex_value = this->ex(dfz_value, kappa_x_value);
      Lane_T kx_value = this->kxk(fz_value, dfz_value, dpi_value);
      Lane_T bx_value = this->bx(kx_value, cx_value, dx_value);
      Lane_T svx_value = this->svx(fz_value, kappa_value, lmux_prime_value, phi_value);
      Lane_T fxp_value =
          (dx_value * sin_lane(cx_value * atan_lane((bx_value * kappa_x_value) -
                                                    (ex_value * (bx_value * kappa_x_value -
                                                                 atan_lane(bx_value * kappa_x_value)))))) +
          svx_value;
      return fxp_value;
    }
    /// alpha_y: MF 6.1 equation block.
    [[nodiscard]] Lane_T alpha_y(const Lane_T &alpha_value, const Lane_T &shy_value) const noexcept {

      return alpha_value + shy_value;
    }
    /// cy: MF 6.1 equation block.
    [[nodiscard]] Lane_T cy() const noexcept {

      return (constant(c_->PCY1) * constant(c_->LCY));
    }
    /// dy: MF 6.1 equation block.
    [[nodiscard]] Lane_T dy(const Lane_T &fz_value, const Lane_T &muy_value, const Lane_T &alpha_f_value,
                            const Lane_T &phi_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);

      Lane_T z2_value = constant(0.0);
      if (turn_slip_) {
        Lane_T byp_value = this->byp(dfz_value, alpha_f_value);
        z2_value = this->zeta2(byp_value, phi_value);
      } else {
        z2_value = constant(1);
      }

      return muy_value * fz_value * z2_value;
    }
    /// muy: MF 6.1 equation block.
    [[nodiscard]] Lane_T muy(const Lane_T &dfz_value, const Lane_T &gamma_value, const Lane_T &dpi_value,
                             const Lane_T &lmuy_star_value) const noexcept {

      return (constant(c_->PDY1) + constant(c_->PDY2) * dfz_value) *
             (constant(1) - constant(c_->PDY3) * gamma_value * gamma_value) *
             (constant(1) + constant(c_->PPY3) * dpi_value + constant(c_->PPY4) * dpi_value * dpi_value) *
             lmuy_star_value;
    }
    /// ey: MF 6.1 equation block.
    [[nodiscard]] Lane_T ey(const Lane_T &dfz_value, const Lane_T &gamma_value,
                            const Lane_T &alpha_y_value) const noexcept {

      Lane_T ey_value = ((constant(c_->PEY1) + constant(c_->PEY2) * dfz_value) *
                         (constant(1.) + (constant(c_->PEY5) * gamma_value * gamma_value) -
                          ((constant(c_->PEY3) + constant(c_->PEY4) * gamma_value) * sign_lane(alpha_y_value)))) *
                        constant(c_->LEY);

      return ey_value.min(1.0);
    }
    /// kya: MF 6.1 equation block.
    [[nodiscard]] Lane_T kya(const Lane_T &gamma_value, const Lane_T &fz_value, const Lane_T &dpi_value,
                             const Lane_T &phi_value) const noexcept {

      Lane_T z3_value = constant(0.0);
      if (turn_slip_) {
        z3_value = this->zeta3(phi_value);
      } else {
        z3_value = constant(1);
      }

      return constant(c_->PKY1) * constant(c_->FNOMIN) * constant(c_->LFZ0) *
             (constant(1) + constant(c_->PPY1) * dpi_value) *
             sin_lane(constant(c_->PKY4) *
                      atan_lane((fz_value / (constant(c_->FNOMIN) * constant(c_->LFZ0))) /
                                ((constant(c_->PKY2) + constant(c_->PKY5) * gamma_value * gamma_value) *
                                 (constant(1) + constant(c_->PPY2) * dpi_value)))) *
             constant(c_->LKY) * (constant(1) - constant(c_->PKY3) * abs_lane(gamma_value)) * z3_value;
    }
    /// kya0: MF 6.1 equation block.
    [[nodiscard]] Lane_T kya0(const Lane_T &fz_value, const Lane_T &dpi_value, const Lane_T &phi_value) const noexcept {

      Lane_T z3_value = constant(0.0);
      if (turn_slip_) {
        z3_value = this->zeta3(phi_value);
      } else {
        z3_value = constant(1);
      }

      Lane_T fz0_value = constant(c_->FNOMIN) * constant(c_->LFZ0);
      return constant(c_->PKY1) * fz0_value * (constant(1) + constant(c_->PPY1) * dpi_value) *
             sin_lane(constant(c_->PKY4) *
                      atan_lane((fz_value / fz0_value) /
                                (constant(c_->PKY2) * (constant(1) + constant(c_->PPY2) * dpi_value)))) *
             constant(c_->LKY) * z3_value;
    }
    /// kyg0: MF 6.1 equation block.
    [[nodiscard]] Lane_T kyg0(const Lane_T &fz_value, const Lane_T &p_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T dpi_value = this->dpi(p_value);
      return (constant(c_->PKY6) + constant(c_->PKY7) * dfz_value) * (constant(1) + constant(c_->PPY5) * dpi_value) *
             fz_value * constant(c_->LKYC);
    }
    /// by: MF 6.1 equation block.
    [[nodiscard]] Lane_T by(const Lane_T &kya_value, const Lane_T &cy_value, const Lane_T &dy_value) const noexcept {

      return (kya_value) / (cy_value * dy_value + constant(1e-6) * sign_lane(dy_value));
    }
    /// svy0: MF 6.1 equation block.
    [[nodiscard]] Lane_T svy0(const Lane_T &fz_value, const Lane_T &lmuy_prime_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      return fz_value * (constant(c_->PVY1) + constant(c_->PVY2) * dfz_value) * constant(c_->LVY) * lmuy_prime_value;
    }
    /// svyg: MF 6.1 equation block.
    [[nodiscard]] Lane_T svyg(const Lane_T &fz_value, const Lane_T &gamma_value, const Lane_T &lmuy_prime_value,
                              const Lane_T &alpha_f_value, const Lane_T &phi_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);

      Lane_T z2_value = constant(0.0);
      if (turn_slip_) {
        Lane_T byp_value = this->byp(dfz_value, alpha_f_value);
        z2_value = this->zeta2(byp_value, phi_value);
      } else {
        z2_value = constant(1);
      }

      return fz_value * (constant(c_->PVY3) + constant(c_->PVY4) * dfz_value) * gamma_value * constant(c_->LKYC) *
             lmuy_prime_value * z2_value;
    }
    /// svy: MF 6.1 equation block.
    [[nodiscard]] Lane_T svy(const Lane_T &svy0_value, const Lane_T &svyg_value, const Lane_T &fz_value,
                             const Lane_T &alpha_f_value, const Lane_T &phi_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);

      Lane_T z2_value = constant(0.0);
      if (turn_slip_) {
        Lane_T byp_value = this->byp(dfz_value, alpha_f_value);
        z2_value = this->zeta2(byp_value, phi_value);
      } else {
        z2_value = constant(1);
      }

      return (svy0_value * z2_value) + svyg_value;
    }
    /// shyg: MF 6.1 equation block.
    [[nodiscard]] Lane_T shyg(const Lane_T &gamma_value, const Lane_T &kyg0_value, const Lane_T &kya_value,
                              const Lane_T &svyg_value, const Lane_T &fz_value, const Lane_T &p_value,
                              const Lane_T &phi_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T dpi_value = this->dpi(p_value);
      Lane_T eg_value = this->eg(dfz_value);

      Lane_T z0_value = constant(0.0);
      if (turn_slip_) {
        z0_value = constant(0);
      } else {
        z0_value = constant(1);
      }

      Lane_T z4_value = constant(0.0);
      if (turn_slip_) {
        Lane_T ehyp_value = this->ehyp();
        Lane_T chyp_value = this->chyp();
        Lane_T kyrp0_value = this->kyrp0(kyg0_value, eg_value);
        Lane_T dhyp_value = this->dhyp(dfz_value);
        Lane_T kya0_value = this->kya0(fz_value, dpi_value, phi_value);
        Lane_T bhyp_value = this->bhyp(kyrp0_value, chyp_value, dhyp_value, kya0_value);
        Lane_T shyp_value = this->shyp(dhyp_value, chyp_value, ehyp_value, bhyp_value, phi_value);
        z4_value = this->zeta4(shyp_value, svyg_value, kya_value);
      } else {
        z4_value = constant(1);
      }

      return (((kyg0_value * gamma_value) - svyg_value) / (kya_value + constant(1e-6) * sign_lane(kya_value)) *
              z0_value) +
             z4_value - constant(1);
    }
    /// shy0: MF 6.1 equation block.
    [[nodiscard]] Lane_T shy0(const Lane_T &dfz_value) const noexcept {

      return (constant(c_->PHY1) + constant(c_->PHY2) * dfz_value) * constant(c_->LHY);
    }
    /// shy: MF 6.1 equation block.
    [[nodiscard]] Lane_T shy(const Lane_T &shy0_value, const Lane_T &shyg_value) const noexcept {

      return shy0_value + shyg_value;
    }
    /// fy_pure: MF 6.1 equation block.
    [[nodiscard]] Lane_T fy_pure(const Lane_T &alpha_value, const Lane_T &gamma_value, const Lane_T &fz_value,
                                 const Lane_T &pi_value, const Lane_T &vx_value,
                                 const Lane_T &pt_value) const noexcept {

      Lane_T lmuy_star_value = this->lmuy_star(alpha_value, constant(0), vx_value);
      Lane_T lmuy_prime_value = this->lmuy_prime(alpha_value, constant(0), vx_value);
      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T dpi_value = this->dpi(pi_value);
      Lane_T phi_value = this->phi(alpha_value, constant(0), fz_value, gamma_value, vx_value, pi_value, pt_value);
      Lane_T muy_value = this->muy(dfz_value, gamma_value, dpi_value, lmuy_star_value);
      Lane_T dy_value = this->dy(fz_value, muy_value, alpha_value, phi_value);
      Lane_T cy_value = this->cy();
      Lane_T kya_value = this->kya(gamma_value, fz_value, dpi_value, phi_value);
      Lane_T by_value = this->by(kya_value, cy_value, dy_value);
      Lane_T svyg_value = this->svyg(fz_value, gamma_value, lmuy_prime_value, alpha_value, phi_value);
      Lane_T kyg0_value = this->kyg0(fz_value, pi_value);
      Lane_T shy0_value = this->shy0(dfz_value);
      Lane_T shyg_value = this->shyg(gamma_value, kyg0_value, kya_value, svyg_value, fz_value, pi_value, phi_value);
      Lane_T shy_value = this->shy(shy0_value, shyg_value);
      Lane_T alpha_y_value = this->alpha_y(alpha_value, shy_value);
      Lane_T ey_value = this->ey(dfz_value, gamma_value, alpha_y_value);
      Lane_T svy0_value = this->svy0(fz_value, lmuy_prime_value);
      Lane_T svsy_value = this->svy(svy0_value, svyg_value, fz_value, alpha_value, phi_value);
      return (dy_value * sin_lane(cy_value * atan_lane((by_value * alpha_y_value) -
                                                       (ey_value * (by_value * alpha_y_value -
                                                                    atan_lane(by_value * alpha_y_value)))))) +
             svsy_value;
    }
    /// fy0: MF 6.1 equation block.
    [[nodiscard]] Lane_T fy0(const Lane_T &alpha_value, const Lane_T &kappa_value, const Lane_T &gamma_value,
                             const Lane_T &fz_value, const Lane_T &vx_value, const Lane_T &p_value,
                             const Lane_T &pt_value) const noexcept {

      Lane_T lmuy_star_value = this->lmuy_star(alpha_value, constant(0), vx_value);
      Lane_T lmuy_prime_value = this->lmuy_prime(alpha_value, constant(0), vx_value);
      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T dpi_value = this->dpi(p_value);
      Lane_T phi_value = this->phi(alpha_value, kappa_value, fz_value, gamma_value, vx_value, p_value, pt_value);
      Lane_T muy_value = this->muy(dfz_value, constant(0) * gamma_value, dpi_value, lmuy_star_value);
      Lane_T dy_value = this->dy(fz_value, muy_value, alpha_value, phi_value);
      Lane_T cy_value = this->cy();
      Lane_T kya_value = this->kya(gamma_value, fz_value, dpi_value, phi_value);
      Lane_T by_value = this->by(kya_value, cy_value, dy_value);
      Lane_T svyg_value = this->svyg(fz_value, constant(0) * gamma_value, lmuy_prime_value, alpha_value, phi_value);
      Lane_T kyg0_value = this->kyg0(fz_value, p_value);
      Lane_T shy0_value = this->shy0(dfz_value);
      Lane_T shyg_value =
          this->shyg(constant(0) * gamma_value, kyg0_value, kya_value, svyg_value, fz_value, p_value, phi_value);
      Lane_T shy_value = this->shy(shy0_value, shyg_value);
      Lane_T alpha_y_value = this->alpha_y(alpha_value, shy_value);
      Lane_T ey_value = this->ey(dfz_value, constant(0) * gamma_value, alpha_y_value);
      Lane_T svy0_value = this->svy0(fz_value, lmuy_prime_value);
      Lane_T svsy_value = this->svy(svy0_value, svyg_value, fz_value, alpha_value, phi_value);

      return (dy_value * sin_lane(cy_value * atan_lane((by_value * alpha_y_value) -
                                                       (ey_value * (by_value * alpha_y_value -
                                                                    atan_lane(by_value * alpha_y_value)))))) +
             svsy_value;
    }
    /// shxa: MF 6.1 equation block.
    [[nodiscard]] Lane_T shxa() const noexcept {

      return constant(c_->RHX1);
    }
    /// exa: MF 6.1 equation block.
    [[nodiscard]] Lane_T exa(const Lane_T &dfz_value) const noexcept {

      Lane_T exa_value = constant(c_->REX1) + (constant(c_->REX2) * dfz_value);

      return exa_value.min(1.0);
    }
    /// cxa: MF 6.1 equation block.
    [[nodiscard]] Lane_T cxa() const noexcept {

      return constant(c_->RCX1);
    }
    /// bxa: MF 6.1 equation block.
    [[nodiscard]] Lane_T bxa(const Lane_T &kappa_value, const Lane_T &gamma_value) const noexcept {

      Lane_T bxa_value = (constant(c_->RBX1) + constant(c_->RBX3) * gamma_value * gamma_value) *
                         cos_lane(atan_lane(constant(c_->RBX2) * kappa_value)) * constant(c_->LXAL);
      return (bxa_value);
    }
    /// alpha_s: MF 6.1 equation block.
    [[nodiscard]] Lane_T alpha_s(const Lane_T &alpha_value, const Lane_T &shxa_value) const noexcept {

      return alpha_value + shxa_value;
    }
    /// gxa: MF 6.1 equation block.
    [[nodiscard]] Lane_T gxa(const Lane_T &fz_value, const Lane_T &alpha_value, const Lane_T &kappa_value,
                             const Lane_T &gamma_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T cxa_value = this->cxa();
      Lane_T bxa_value = this->bxa(kappa_value, gamma_value);
      Lane_T shxa_value = this->shxa();
      Lane_T alpha_s_value = this->alpha_s(alpha_value, shxa_value);
      Lane_T exa_value = this->exa(dfz_value);
      Lane_T gxa_value =
          (cos_lane(cxa_value *
                    atan_lane((bxa_value * alpha_s_value) -
                              (exa_value * (bxa_value * alpha_s_value - atan_lane(bxa_value * alpha_s_value)))))) /
          (cos_lane(cxa_value * atan_lane((bxa_value * shxa_value) -
                                          (exa_value * (bxa_value * shxa_value - atan_lane(bxa_value * shxa_value))))));

      return gxa_value;
    }
    /// fx: MF 6.1 equation block.
    [[nodiscard]] Lane_T fx(const Lane_T &alpha_value, const Lane_T &kappa_value, const Lane_T &gamma_value,
                            const Lane_T &fz_value, const Lane_T &pi_value, const Lane_T &vx_value,
                            const Lane_T &pt_value) const noexcept {

      Lane_T fx0_value = this->fx_pure(kappa_value, gamma_value, fz_value, pi_value, vx_value, pt_value);
      Lane_T gxa_value = this->gxa(fz_value, alpha_value, kappa_value, gamma_value);
      return fx0_value * gxa_value;
    }
    /// shyk: MF 6.1 equation block.
    [[nodiscard]] Lane_T shyk(const Lane_T &dfz_value) const noexcept {

      return constant(c_->RHY1) + (constant(c_->RHY2) * dfz_value);
    }
    /// eyk: MF 6.1 equation block.
    [[nodiscard]] Lane_T eyk(const Lane_T &dfz_value) const noexcept {

      Lane_T eyk_value = constant(c_->REY1) + (constant(c_->REY2) * dfz_value);

      return eyk_value.min(1.0);
    }
    /// cyk: MF 6.1 equation block.
    [[nodiscard]] Lane_T cyk() const noexcept {

      return constant(c_->RCY1);
    }
    /// byk: MF 6.1 equation block.
    [[nodiscard]] Lane_T byk(const Lane_T &alpha_value, const Lane_T &gamma_value) const noexcept {

      return (constant(c_->RBY1) + constant(c_->RBY4) * gamma_value * gamma_value) *
             cos_lane(atan_lane(constant(c_->RBY2) * (alpha_value - constant(c_->RBY3)))) * constant(c_->LYKA);
    }
    /// kappa_s: MF 6.1 equation block.
    [[nodiscard]] Lane_T kappa_s(const Lane_T &kappa_value, const Lane_T &shyk_value) const noexcept {

      return kappa_value + shyk_value;
    }
    /// gyk0: MF 6.1 equation block.
    [[nodiscard]] Lane_T gyk0(const Lane_T &alpha_value, const Lane_T &gamma_value,
                              const Lane_T &fz_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T shyk_value = this->shyk(dfz_value);
      Lane_T byk_value = this->byk(alpha_value, gamma_value);
      Lane_T cyk_value = this->cyk();
      Lane_T eyk_value = this->eyk(dfz_value);
      return cos_lane(cyk_value *
                      atan_lane((byk_value * shyk_value) -
                                (eyk_value * (byk_value * shyk_value - atan_lane(byk_value * shyk_value)))));
    }
    /// gyk: MF 6.1 equation block.
    [[nodiscard]] Lane_T gyk(const Lane_T &alpha_value, const Lane_T &gamma_value, const Lane_T &kappa_value,
                             const Lane_T &fz_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T shyk_value = this->shyk(dfz_value);
      Lane_T kappa_s_value = this->kappa_s(kappa_value, shyk_value);
      Lane_T byk_value = this->byk(alpha_value, gamma_value);
      Lane_T cyk_value = this->cyk();
      Lane_T eyk_value = this->eyk(dfz_value);
      Lane_T gyk0_value = this->gyk0(alpha_value, gamma_value, fz_value);
      Lane_T gyk_value =
          (cos_lane(cyk_value *
                    atan_lane((byk_value * kappa_s_value) -
                              (eyk_value * (byk_value * kappa_s_value - atan_lane(byk_value * kappa_s_value))))) /
           gyk0_value);

      return (gyk_value);
    }
    /// dvyk: MF 6.1 equation block.
    [[nodiscard]] Lane_T dvyk(const Lane_T &alpha_value, const Lane_T &gamma_value, const Lane_T &fz_value,
                              const Lane_T &dfz_value, const Lane_T &muy_value, const Lane_T &pf_value) const noexcept {

      Lane_T z2_value = constant(0.0);
      if (turn_slip_) {
        Lane_T byp_value = this->byp(dfz_value, alpha_value);
        z2_value = this->zeta2(byp_value, pf_value);
      } else {
        z2_value = constant(1);
      }

      return (muy_value * fz_value *
              (constant(c_->RVY1) + (constant(c_->RVY2) * dfz_value) + (constant(c_->RVY3) * gamma_value)) *
              cos_lane(atan_lane(constant(c_->RVY4) * alpha_value)) * z2_value);
    }
    /// svyk: MF 6.1 equation block.
    [[nodiscard]] Lane_T svyk(const Lane_T &dvyk_value, const Lane_T &kappa_value) const noexcept {

      return dvyk_value * sin_lane(constant(c_->RVY5) * atan_lane(constant(c_->RVY6) * kappa_value)) *
             constant(c_->LVYKA);
    }
    /// fy: MF 6.1 equation block.
    [[nodiscard]] Lane_T fy(const Lane_T &alpha_value, const Lane_T &kappa_value, const Lane_T &gamma_value,
                            const Lane_T &fz_value, const Lane_T &pi_value, const Lane_T &vx_value,
                            const Lane_T &pt_value) const noexcept {

      Lane_T lmuy_star_value = this->lmuy_star(alpha_value, kappa_value, vx_value);
      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T dpi_value = this->dpi(pi_value);
      Lane_T phi_value = this->phi(alpha_value, kappa_value, fz_value, gamma_value, vx_value, pi_value, pt_value);
      Lane_T fy0_value = this->fy_pure(alpha_value, gamma_value, fz_value, pi_value, vx_value, pt_value);
      Lane_T gyk_value = this->gyk(alpha_value, gamma_value, kappa_value, fz_value);
      Lane_T muy_value = this->muy(dfz_value, gamma_value, dpi_value, lmuy_star_value);
      Lane_T dvyk_value = this->dvyk(alpha_value, gamma_value, fz_value, dfz_value, muy_value, phi_value);
      Lane_T svyk_value = this->svyk(dvyk_value, kappa_value);
      return (fy0_value * gyk_value) + svyk_value;
    }
    /// shf: MF 6.1 equation block.
    [[nodiscard]] Lane_T shf(const Lane_T &shy_value, const Lane_T &svsy_value,
                             const Lane_T &kya_value) const noexcept {

      Lane_T k_prime_ya_value = add_epsilon(kya_value);
      return shy_value + (svsy_value / k_prime_ya_value);
    }
    /// alpha_r: MF 6.1 equation block.
    [[nodiscard]] Lane_T alpha_r(const Lane_T &alpha_m_value, const Lane_T &shf_value) const noexcept {

      return alpha_m_value + shf_value;
    }
    /// alpha_req: MF 6.1 equation block.
    [[nodiscard]] Lane_T alpha_req(const Lane_T &kappa_value, const Lane_T &alpha_r_value, const Lane_T &kxk_value,
                                   const Lane_T &kya_value) const noexcept {

      return atan_lane(sqrt_lane((tan_lane(alpha_r_value) * tan_lane(alpha_r_value)) +
                                 ((kxk_value / add_epsilon(kya_value)) * (kxk_value / add_epsilon(kya_value)) *
                                  kappa_value * kappa_value))) *
             sign_lane(alpha_r_value);
    }
    /// dr: MF 6.1 equation block.
    [[nodiscard]] Lane_T dr(const Lane_T &fz_value, const Lane_T &gamma_value, const Lane_T &vx_value,
                            const Lane_T &alpha_m_value, const Lane_T &kappa_value, const Lane_T &p_value,
                            const Lane_T &pt_value, const Lane_T &lmuy_star_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T dpi_value = this->dpi(p_value);
      Lane_T phi_value = this->phi(alpha_m_value, kappa_value, fz_value, gamma_value, vx_value, p_value, pt_value);
      Lane_T eg_value = this->eg(dfz_value);

      Lane_T z0_value = constant(0.0);
      if (turn_slip_) {
        z0_value = constant(0);
      } else {
        z0_value = constant(1);
      }

      Lane_T z2_value = constant(0.0);
      if (turn_slip_) {
        Lane_T byp_value = this->byp(dfz_value, alpha_m_value);
        z2_value = this->zeta2(byp_value, phi_value);
      } else {
        z2_value = constant(1);
      }

      Lane_T z8_value = constant(0.0);
      if (turn_slip_) {
        Lane_T parking_friction = this->lmuy_star(alpha_m_value, kappa_value, vx_value);
        Lane_T muy_value = this->muy(dfz_value, gamma_value, dpi_value, parking_friction);
        Lane_T mzpinf_value = this->mzpinf(fz_value, muy_value);
        Lane_T kzgr0_value = this->kzgr0(fz_value, gamma_value);
        Lane_T cdrp_value = this->cdrp();
        Lane_T ddrp_value = this->ddrp(mzpinf_value, cdrp_value);
        Lane_T bdrp_value = this->bdrp(kzgr0_value, cdrp_value, ddrp_value, eg_value);
        Lane_T drp_value = this->drp(ddrp_value, cdrp_value, bdrp_value, pt_value);
        z8_value = this->zeta8(drp_value);
      } else {
        z8_value = constant(1);
      }

      Lane_T cos_alpha_star_value = cos_lane(alpha_m_value);
      return (fz_value * constant(c_->UNLOADED_RADIUS) *
              ((constant(c_->QDZ6) + (constant(c_->QDZ7) * dfz_value)) * constant(c_->LRES) * z2_value +
               ((constant(c_->QDZ8) + constant(c_->QDZ9) * dfz_value) * (constant(1) + constant(c_->PPZ2) * dpi_value) +
                (constant(c_->QDZ10) + constant(c_->QDZ11) * dfz_value) * abs_lane(gamma_value)) *
                   gamma_value * constant(c_->LKZC) * z0_value) *
              lmuy_star_value * sign_lane(vx_value) * cos_alpha_star_value) +
             z8_value - constant(1);
    }
    /// br: MF 6.1 equation block.
    [[nodiscard]] Lane_T br(const Lane_T &by_value, const Lane_T &cy_value, const Lane_T &pm_value,
                            const Lane_T &lmuy_star_value) const noexcept {

      Lane_T z6_value = constant(0.0);
      if (turn_slip_) {
        z6_value = this->zeta6(pm_value);
      } else {
        z6_value = constant(1);
      }

      return (constant(c_->QBZ9) * (constant(c_->LKY) / lmuy_star_value)) +
             (constant(c_->QBZ10) * by_value * cy_value * z6_value);
    }
    /// mzr0: MF 6.1 equation block.
    [[nodiscard]] Lane_T mzr0(const Lane_T &fz_value, const Lane_T &alpha_m_value, const Lane_T &gamma_value,
                              const Lane_T &kappa_value, const Lane_T &alpha_req_value, const Lane_T &vx_value,
                              const Lane_T &p_value, const Lane_T &pt_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T dpi_value = this->dpi(p_value);
      Lane_T phi_value = this->phi(alpha_m_value, kappa_value, fz_value, gamma_value, vx_value, p_value, pt_value);
      Lane_T eg_value = this->eg(dfz_value);
      Lane_T lmuy_star_value = this->lmuy_star(alpha_m_value, constant(0), vx_value);

      Lane_T z7_value = constant(0.0);
      if (turn_slip_) {

        Lane_T gyk_value = this->gyk(alpha_m_value, gamma_value, kappa_value, fz_value);
        Lane_T muy_value = this->muy(dfz_value, gamma_value, dpi_value, lmuy_star_value);
        Lane_T mzpinf_value = this->mzpinf(fz_value, muy_value);
        Lane_T mzp90_value = this->mzp90(mzpinf_value, pt_value, gyk_value);

        Lane_T kzgr0_value = this->kzgr0(fz_value, gamma_value);
        Lane_T cdrp_value = this->cdrp();
        Lane_T ddrp_value = this->ddrp(mzpinf_value, cdrp_value);
        Lane_T bdrp_value = this->bdrp(kzgr0_value, cdrp_value, ddrp_value, eg_value);
        Lane_T drp_value = this->drp(ddrp_value, cdrp_value, bdrp_value, pt_value);

        z7_value = this->zeta7(mzp90_value, drp_value, mzpinf_value, gyk_value, kzgr0_value, eg_value);
      } else {
        z7_value = constant(1);
      }

      Lane_T vsy_value = tan_lane(alpha_m_value) * abs_lane(vx_value);
      Lane_T vc_value = sqrt_lane((vx_value * vx_value) + (vsy_value * vsy_value));
      lmuy_star_value = this->lmuy_star(alpha_m_value, kappa_value, vx_value);
      Lane_T kya_value = this->kya(gamma_value, fz_value, dpi_value, phi_value);
      Lane_T muy_value = this->muy(dfz_value, gamma_value, dpi_value, lmuy_star_value);
      Lane_T dy_value = this->dy(fz_value, muy_value, alpha_m_value, phi_value);
      Lane_T cy_value = this->cy();
      Lane_T by_value = this->by(kya_value, cy_value, dy_value);
      Lane_T br_value = this->br(by_value, cy_value, phi_value, lmuy_star_value);
      Lane_T dr_value =
          this->dr(fz_value, gamma_value, vx_value, alpha_m_value, kappa_value, p_value, pt_value, lmuy_star_value);
      Lane_T cos_alpha_prime_value = vx_value / (vc_value + constant(1e-6) * sign_lane(vc_value));
      return dr_value * cos_lane(z7_value * atan_lane(br_value * alpha_req_value)) * cos_alpha_prime_value;
    }
    /// mzr: MF 6.1 equation block.
    [[nodiscard]] Lane_T mzr(const Lane_T &fz_value, const Lane_T &alpha_m_value, const Lane_T &gamma_value,
                             const Lane_T &kappa_value, const Lane_T &alpha_req_value, const Lane_T &vx_value,
                             const Lane_T &p_value, const Lane_T &pt_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T dpi_value = this->dpi(p_value);
      Lane_T phi_value = this->phi(alpha_m_value, kappa_value, fz_value, gamma_value, vx_value, p_value, pt_value);
      Lane_T eg_value = this->eg(dfz_value);
      Lane_T lmuy_star_value = this->lmuy_star(alpha_m_value, constant(0), vx_value);

      Lane_T z7_value = constant(0.0);
      if (turn_slip_) {

        Lane_T gyk_value = this->gyk(alpha_m_value, gamma_value, kappa_value, fz_value);
        Lane_T muy_value = this->muy(dfz_value, gamma_value, dpi_value, lmuy_star_value);
        Lane_T mzpinf_value = this->mzpinf(fz_value, muy_value);
        Lane_T mzp90_value = this->mzp90(mzpinf_value, pt_value, gyk_value);

        Lane_T kzgr0_value = this->kzgr0(fz_value, gamma_value);
        Lane_T cdrp_value = this->cdrp();
        Lane_T ddrp_value = this->ddrp(mzpinf_value, cdrp_value);
        Lane_T bdrp_value = this->bdrp(kzgr0_value, cdrp_value, ddrp_value, eg_value);
        Lane_T drp_value = this->drp(ddrp_value, cdrp_value, bdrp_value, pt_value);

        z7_value = this->zeta7(mzp90_value, drp_value, mzpinf_value, gyk_value, kzgr0_value, eg_value);
      } else {
        z7_value = constant(1);
      }

      lmuy_star_value = this->lmuy_star(alpha_m_value, kappa_value, vx_value);
      Lane_T kya_value = this->kya(gamma_value, fz_value, dpi_value, phi_value);
      Lane_T muy_value = this->muy(dfz_value, gamma_value, dpi_value, lmuy_star_value);
      Lane_T dy_value = this->dy(fz_value, muy_value, alpha_m_value, phi_value);
      Lane_T cy_value = this->cy();
      Lane_T by_value = this->by(kya_value, cy_value, dy_value);
      Lane_T br_value = this->br(by_value, cy_value, phi_value, lmuy_star_value);
      Lane_T dr_value =
          this->dr(fz_value, gamma_value, vx_value, alpha_m_value, kappa_value, p_value, pt_value, lmuy_star_value);
      return dr_value * cos_lane(z7_value * atan_lane(br_value * alpha_req_value));
    }
    /// bt: MF 6.1 equation block.
    [[nodiscard]] Lane_T bt(const Lane_T &fz_value, const Lane_T &gamma_value,
                            const Lane_T &lmuy_star_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T bt_value =
          (constant(c_->QBZ1) + constant(c_->QBZ2) * dfz_value + constant(c_->QBZ3) * dfz_value * dfz_value) *
          (constant(1) + constant(c_->QBZ4) * gamma_value + constant(c_->QBZ5) * abs_lane(gamma_value)) *
          (constant(c_->LKY) / lmuy_star_value);
      return (bt_value);
    }
    /// ct: MF 6.1 equation block.
    [[nodiscard]] Lane_T ct() const noexcept {

      return (constant(c_->QCZ1));
    }
    /// dt: MF 6.1 equation block.
    [[nodiscard]] Lane_T dt(const Lane_T &fz_value, const Lane_T &gamma_value, const Lane_T &p_value,
                            const Lane_T &pm_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T dpi_value = this->dpi(p_value);

      Lane_T z5_value = constant(0.0);
      if (turn_slip_) {
        z5_value = this->zeta5(pm_value);
      } else {
        z5_value = constant(1);
      }

      Lane_T dt_value =
          fz_value * (constant(c_->QDZ1) + constant(c_->QDZ2) * dfz_value) *
          (constant(1) - constant(c_->PPZ1) * dpi_value) *
          (constant(1) + constant(c_->QDZ3) * gamma_value + constant(c_->QDZ4) * gamma_value * gamma_value) *
          (constant(c_->UNLOADED_RADIUS) / (constant(c_->FNOMIN) * constant(c_->LFZ0))) * constant(c_->LTR) * z5_value;
      return dt_value;
    }
    /// sht: MF 6.1 equation block.
    [[nodiscard]] Lane_T sht(const Lane_T &fz_value, const Lane_T &gamma_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      return constant(c_->QHZ1) + (constant(c_->QHZ2) * dfz_value) +
             ((constant(c_->QHZ3) + constant(c_->QHZ4) * dfz_value) * gamma_value);
    }
    /// alpha_t: MF 6.1 equation block.
    [[nodiscard]] Lane_T alpha_t(const Lane_T &alpha_m_value, const Lane_T &fz_value,
                                 const Lane_T &gamma_value) const noexcept {

      Lane_T sht_value = this->sht(fz_value, gamma_value);
      return alpha_m_value + sht_value;
    }
    /// alpha_teq: MF 6.1 equation block.
    [[nodiscard]] Lane_T alpha_teq(const Lane_T &alpha_m_value, const Lane_T &fz_value, const Lane_T &gamma_value,
                                   const Lane_T &kappa_value, const Lane_T &dpi_value,
                                   const Lane_T &pf_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T kya_value = this->kya(gamma_value, fz_value, dpi_value, pf_value);
      Lane_T kxk_value = this->kxk(fz_value, dfz_value, dpi_value);
      Lane_T alpha_t_value = this->alpha_t(alpha_m_value, fz_value, gamma_value);
      return atan_lane(sqrt_lane((tan_lane(alpha_t_value) * tan_lane(alpha_t_value)) +
                                 ((kxk_value / add_epsilon(kya_value)) * (kxk_value / add_epsilon(kya_value)) *
                                  kappa_value * kappa_value))) *
             sign_lane(alpha_t_value);
    }
    /// et: MF 6.1 equation block.
    [[nodiscard]] Lane_T et(const Lane_T &fz_value, const Lane_T &alpha_m_value, const Lane_T &gamma_value,
                            const Lane_T &lmuy_star_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T bt_value = this->bt(fz_value, gamma_value, lmuy_star_value);
      Lane_T ct_value = this->ct();
      Lane_T alpha_t_value = this->alpha_t(alpha_m_value, fz_value, gamma_value);
      Lane_T et_value =
          (constant(c_->QEZ1) + (constant(c_->QEZ2) * dfz_value) + (constant(c_->QEZ3) * dfz_value * dfz_value)) *
          (constant(1) + (constant(c_->QEZ4) + constant(c_->QEZ5) * gamma_value) *
                             (constant(2) / constant(std::numbers::pi)) *
                             atan_lane(bt_value * ct_value * alpha_t_value));

      return et_value.min(1.0);
    }
    /// t: MF 6.1 equation block.
    [[nodiscard]] Lane_T t(const Lane_T &fz_value, const Lane_T &alpha_m_value, const Lane_T &gamma_value,
                           const Lane_T &alpha_teq_value, const Lane_T &vx_value, const Lane_T &p_value,
                           const Lane_T &pm_value, const Lane_T &lmuy_star_value) const noexcept {

      Lane_T vsy_value = tan_lane(alpha_m_value) * abs_lane(vx_value);
      Lane_T vc_value = sqrt_lane((vx_value * vx_value) + (vsy_value * vsy_value));
      Lane_T cos_alpha_prime_value = vx_value / (vc_value + constant(1e-6) * sign_lane(vc_value));
      Lane_T dt_value = this->dt(fz_value, gamma_value, p_value, pm_value);
      Lane_T ct_value = this->ct();
      Lane_T bt_value = this->bt(fz_value, gamma_value, lmuy_star_value);
      Lane_T et_value = this->et(fz_value, alpha_m_value, gamma_value, lmuy_star_value);

      return dt_value *
             cos_lane(ct_value *
                      atan_lane((bt_value * alpha_teq_value) -
                                (et_value * ((bt_value * alpha_teq_value) - atan_lane(bt_value * alpha_teq_value))))) *
             cos_alpha_prime_value;
    }
    /// s: MF 6.1 equation block.
    [[nodiscard]] Lane_T s(const Lane_T &fz_value, const Lane_T &alpha_value, const Lane_T &gamma_value,
                           const Lane_T &kappa_value, const Lane_T &pi_value, const Lane_T &vx_value,
                           const Lane_T &pt_value) const noexcept {

      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T fy_value = this->fy(alpha_value, kappa_value, gamma_value, fz_value, pi_value, vx_value, pt_value);
      return (constant(c_->SSZ1) + (constant(c_->SSZ2) * (fy_value / constant(c_->FNOMIN))) +
              ((constant(c_->SSZ3) + constant(c_->SSZ4) * dfz_value) * gamma_value)) *
             constant(c_->UNLOADED_RADIUS) * constant(c_->LS);
    }
    /// mz_pure: MF 6.1 equation block.
    [[nodiscard]] Lane_T mz_pure(const Lane_T &alpha_m_value, const Lane_T &gamma_value, const Lane_T &fz_value,
                                 const Lane_T &pi_value, const Lane_T &vx_value,
                                 const Lane_T &pt_value) const noexcept {

      Lane_T lmuy_star_value = this->lmuy_star(alpha_m_value, constant(0), vx_value);
      Lane_T lmuy_prime_value = this->lmuy_prime(alpha_m_value, constant(0), vx_value);
      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T dpi_value = this->dpi(pi_value);
      Lane_T phi_value = this->phi(alpha_m_value, constant(0), fz_value, gamma_value, vx_value, pi_value, pt_value);
      Lane_T kya_value = this->kya(gamma_value, fz_value, dpi_value, phi_value);
      Lane_T shy0_value = this->shy0(dfz_value);
      Lane_T svyg_value = this->svyg(fz_value, gamma_value, lmuy_prime_value, alpha_m_value, phi_value);
      Lane_T kyg0_value = this->kyg0(fz_value, pi_value);
      Lane_T shyg_value = this->shyg(gamma_value, kyg0_value, kya_value, svyg_value, fz_value, pi_value, phi_value);
      Lane_T shy_value = this->shy(shy0_value, shyg_value);
      Lane_T svy0_value = this->svy0(fz_value, lmuy_prime_value);
      Lane_T svsy_value = this->svy(svy0_value, svyg_value, fz_value, alpha_m_value, phi_value);
      Lane_T shf_value = this->shf(shy_value, svsy_value, kya_value);
      Lane_T alpha_req_value = this->alpha_r(alpha_m_value, shf_value);
      Lane_T mzr_value =
          this->mzr0(fz_value, alpha_m_value, gamma_value, constant(0), alpha_req_value, vx_value, pi_value, pt_value);
      Lane_T alpha_teq_value = this->alpha_t(alpha_m_value, fz_value, gamma_value);
      Lane_T t_value = this->t(fz_value, alpha_m_value, gamma_value, alpha_teq_value, vx_value, pi_value, phi_value,
                               lmuy_star_value);
      Lane_T fyp0_value = this->fy_pure(alpha_m_value, constant(0), fz_value, pi_value, vx_value, pt_value);
      return (-t_value * fyp0_value) + mzr_value;
    }
    /// mz: MF 6.1 equation block.
    [[nodiscard]] Lane_T mz(const Lane_T &alpha_m_value, const Lane_T &gamma_value, const Lane_T &kappa_value,
                            const Lane_T &fz_value, const Lane_T &p_value, const Lane_T &vx_value,
                            const Lane_T &pt_value) const noexcept {

      Lane_T lmuy_star_value = this->lmuy_star(alpha_m_value, constant(0), vx_value);
      Lane_T lmuy_prime_value = this->lmuy_prime(alpha_m_value, constant(0), vx_value);
      Lane_T dfz_value = this->dfz(fz_value);
      Lane_T dpi_value = this->dpi(p_value);
      Lane_T phi_value = this->phi(alpha_m_value, kappa_value, fz_value, gamma_value, vx_value, p_value, pt_value);
      Lane_T kxk_value = this->kxk(fz_value, dfz_value, dpi_value);
      Lane_T kya_value = this->kya(gamma_value, fz_value, dpi_value, phi_value);
      Lane_T shy0_value = this->shy0(dfz_value);
      Lane_T svyg_value = this->svyg(fz_value, gamma_value, lmuy_prime_value, alpha_m_value, phi_value);
      Lane_T kyg0_value = this->kyg0(fz_value, p_value);
      Lane_T shyg_value = this->shyg(gamma_value, kyg0_value, kya_value, svyg_value, fz_value, p_value, phi_value);
      Lane_T shy_value = this->shy(shy0_value, shyg_value);
      Lane_T svy0_value = this->svy0(fz_value, lmuy_prime_value);
      Lane_T svsy_value = this->svy(svy0_value, svyg_value, fz_value, alpha_m_value, phi_value);
      Lane_T shf_value = this->shf(shy_value, svsy_value, kya_value);
      Lane_T alpha_r_value = this->alpha_r(alpha_m_value, shf_value);
      Lane_T alpha_req_value = this->alpha_req(kappa_value, alpha_r_value, kxk_value, kya_value);
      Lane_T mzr_value =
          this->mzr(fz_value, alpha_m_value, gamma_value, kappa_value, alpha_req_value, vx_value, p_value, pt_value);
      Lane_T alpha_teq_value = this->alpha_teq(alpha_m_value, fz_value, gamma_value, kappa_value, dpi_value, phi_value);
      Lane_T t_value =
          this->t(fz_value, alpha_m_value, gamma_value, alpha_teq_value, vx_value, p_value, phi_value, lmuy_star_value);
      Lane_T fyp_value = this->fy0(alpha_m_value, kappa_value, gamma_value, fz_value, vx_value, p_value, pt_value);
      Lane_T gyk0_value = this->gyk(alpha_m_value, gamma_value, kappa_value, fz_value);
      Lane_T fx_value = this->fx(alpha_m_value, kappa_value, gamma_value, fz_value, p_value, vx_value, pt_value);
      Lane_T s_value = this->s(fz_value, alpha_m_value, gamma_value, kappa_value, p_value, vx_value, pt_value);
      Lane_T mz_value = (-t_value * fyp_value * gyk0_value) + mzr_value + (s_value * fx_value);
      return mz_value;
    }
    /// mx: MF 6.1 equation block.
    [[nodiscard]] Lane_T mx(const Lane_T &gamma_value, const Lane_T &fz_value, const Lane_T &p_value,
                            const Lane_T &fy_value) const noexcept {
      Lane_T dpi_value = this->dpi(p_value);

      Lane_T mx_value =
          (constant(c_->UNLOADED_RADIUS) * fz_value * constant(c_->LMX) *
           (constant(c_->QSX1) * constant(c_->LVMX) -
            constant(c_->QSX2) * gamma_value * (constant(1) + constant(c_->PPMX1) * dpi_value) +
            constant(c_->QSX3) * (fy_value / constant(c_->FNOMIN)) +
            constant(c_->QSX4) *
                cos_lane(constant(c_->QSX5) * atan_lane((constant(c_->QSX6) * fz_value / constant(c_->FNOMIN)) *
                                                        (constant(c_->QSX6) * fz_value / constant(c_->FNOMIN)))) *
                sin_lane((constant(c_->QSX7) * gamma_value) +
                         (constant(c_->QSX8) * atan_lane(constant(c_->QSX9) * fy_value / constant(c_->FNOMIN)))) +
            constant(c_->QSX10) * atan_lane(constant(c_->QSX11) * fz_value / constant(c_->FNOMIN)) * gamma_value)) +
          (constant(c_->UNLOADED_RADIUS) * constant(c_->LMX) *
           (fy_value * (constant(c_->QSX13) + constant(c_->QSX14) * abs_lane(gamma_value)) -
            fz_value * constant(c_->QSX12) * gamma_value * abs_lane(gamma_value)));
      return mx_value;
    }
    /// my: MF 6.1 equation block.
    [[nodiscard]] Lane_T my(const Lane_T &gamma_value, const Lane_T &fz_value, const Lane_T &vx_value,
                            const Lane_T &p_value, const Lane_T &fx_value) const noexcept {
      Lane_T my_value =
          (-constant(1) * constant(c_->UNLOADED_RADIUS)) * constant(c_->FNOMIN) * constant(c_->LMY) *
          (constant(c_->QSY1) + constant(c_->QSY2) * (fx_value / constant(c_->FNOMIN)) +
           constant(c_->QSY3) * abs_lane(vx_value / constant(c_->LONGVL)) +
           constant(c_->QSY4) * (((vx_value / constant(c_->LONGVL)) * (vx_value / constant(c_->LONGVL)) *
                                  (vx_value / constant(c_->LONGVL)) * (vx_value / constant(c_->LONGVL)))) +
           constant(c_->QSY5) * gamma_value * gamma_value +
           constant(c_->QSY6) * fz_value / constant(c_->FNOMIN) * gamma_value * gamma_value) *
          (pow_lane(fz_value / constant(c_->FNOMIN), constant(c_->QSY7)) *
           pow_lane(p_value / constant(c_->NOMPRES), constant(c_->QSY8)));
      return my_value;
    }

    /// Turn-slip residual-angle factor, with the continuous zero-turn-slip ratio.
    [[nodiscard]] Lane_T zeta7(const Lane_T &moment90, const Lane_T &parking, const Lane_T &moment_infinite,
                               const Lane_T &weight, const Lane_T &stiffness,
                               const Lane_T &camber_reduction) const noexcept {
      const Lane_T zero = constant(0.0);
      const Lane_T one = constant(1.0);
      const Lane_T amplitude = abs_lane(parking);
      const Lane_T normal = amplitude + select_lanes(mf61_values(amplitude) == 0.0, one, zero);
      const Lane_T numerator = moment_infinite * (2.0 / std::numbers::pi) * c_->QCRP2 * weight;
      const Lane_T denominator = abs_lane(stiffness / add_epsilon(Lane_T(one - camber_reduction)));
      const Lane_T guarded = denominator + select_lanes(mf61_values(denominator) == 0.0, one, zero);
      const Lane_T finite_limit = numerator / guarded;
      const Lane_T infinite_limit = sgn_plus(numerator) * std::numeric_limits<double>::infinity();
      const Lane_T neutral_limit = select_lanes(mf61_values(numerator) == 0.0, zero, infinite_limit);
      const Lane_T missing_limit = select_lanes(mf61_values(numerator).isNaN(), numerator, neutral_limit);
      const Lane_T ratio_limit = select_lanes(mf61_values(denominator) == 0.0, missing_limit, finite_limit);
      const Lane_T regular_ratio = moment90 / normal;
      const Lane_T ratio = select_lanes(mf61_values(amplitude) == 0.0, ratio_limit, regular_ratio);
      const Lane_T signed_ratio = ratio * std::copysign(1.0, c_->FNOMIN);
      return (2.0 / std::numbers::pi) * acos_lane(signed_ratio);
    }
    /// Effective rolling radius, fixed-point updates with independently frozen converged lanes.
    [[nodiscard]] Lane_T re(const Lane_T &kappa, const Lane_T &fz, const Lane_T &vx,
                            const Lane_T &pressure) const noexcept {
      const Lane_T one = constant(1.0);
      Lane_T radius = constant(c_->UNLOADED_RADIUS * 0.965);
      const Lane_T stiffness = constant(c_->VERTICAL_STIFFNESS) * (one + constant(c_->PFZ1) * dpi(pressure));
      const Lane_T deflection = constant(c_->FNOMIN) / stiffness *
                                (constant(c_->DREFF) * atan_lane(constant(c_->BREFF) * fz / c_->FNOMIN) +
                                 constant(c_->FREFF) * fz / c_->FNOMIN);
      mask_of<value_lane> active = mf61_values(radius) > -std::numeric_limits<double>::infinity();
      for (int iteration = 0; iteration < 100; ++iteration) {
        const Lane_T wheel_speed = (one + kappa) * vx / radius;
        const Lane_T speed_ratio = wheel_speed * c_->UNLOADED_RADIUS / c_->LONGVL;
        const Lane_T next =
            (constant(c_->UNLOADED_RADIUS) * (constant(c_->Q_RE0) + constant(c_->Q_V1) * speed_ratio * speed_ratio)) -
            deflection;
        const mask_of<value_lane> pending = (mf61_values(abs_lane(next - radius)) > 1e-6);
        radius = select_lanes(active, next, radius);
        active = active && pending;
        if (!active.any()) {
          break;
        }
      }
      const Lane_T failed = constant(std::numeric_limits<double>::quiet_NaN());
      const mask_of<value_lane> positive = mf61_values(radius) > 0.0;
      const mask_of<value_lane> valid = positive && !active;
      return select_lanes(valid, radius, failed);
    }
    /// Wheel angular speed from the converged effective radius.
    [[nodiscard]] Lane_T omega(const Lane_T &kappa, const Lane_T &fz, const Lane_T &vx,
                               const Lane_T &pressure) const noexcept {
      return (constant(1.0) + kappa) * vx / re(kappa, fz, vx, pressure);
    }
    /// Loaded radius from the positive vertical-deflection root; zero quadratic curvature uses its linear limit.
    [[nodiscard]] Lane_T rl(const Lane_T &alpha, const Lane_T &kappa, const Lane_T &gamma, const Lane_T &fz,
                            const Lane_T &pressure, const Lane_T &vx, const Lane_T &radius) const noexcept {
      const Lane_T zero = constant(0.0);
      const Lane_T one = constant(1.0);
      const Lane_T wheel_speed = (one + kappa) * vx / radius;
      const Lane_T speed_ratio = wheel_speed * c_->UNLOADED_RADIUS / c_->LONGVL;
      const Lane_T unloaded =
          constant(c_->UNLOADED_RADIUS) * (constant(c_->Q_RE0) + constant(c_->Q_V1) * speed_ratio * speed_ratio);
      const Lane_T force_x = fx(alpha, kappa, gamma, fz, pressure, vx, zero);
      const Lane_T force_y = fy(alpha, kappa, gamma, fz, pressure, vx, zero);
      const Lane_T x_effect = constant(c_->Q_FCX) * force_x / c_->FNOMIN;
      const Lane_T y_effect = constant(c_->Q_FCY) * force_y / c_->FNOMIN;
      const Lane_T external = (one + constant(c_->Q_V2 * c_->UNLOADED_RADIUS / c_->LONGVL) * abs_lane(wheel_speed) -
                               x_effect * x_effect - y_effect * y_effect) *
                              (one + constant(c_->PFZ1) * dpi(pressure)) * c_->FNOMIN;
      const Lane_T load = fz / external;
      const Lane_T discriminant = constant(c_->Q_FZ1 * c_->Q_FZ1) + (constant(4.0 * c_->Q_FZ2) * load);
      const Lane_T root = sqrt_lane(select_lanes(mf61_values(discriminant) >= 0.0, discriminant, zero));
      const Lane_T denominator = constant(c_->Q_FZ1) + root;
      const Lane_T safe_denominator = denominator + select_lanes(mf61_values(abs_lane(denominator)) > 0.0, zero, one);
      const Lane_T deflection = 2.0 * load / safe_denominator;
      const Lane_T rho = select_lanes(mf61_values(deflection) > 0.0, deflection, zero) * c_->UNLOADED_RADIUS;
      const Lane_T loaded = unloaded - rho;
      const Lane_T positive = select_lanes(mf61_values(loaded) > 0.0, loaded, zero);
      const Lane_T physical = select_lanes(mf61_values(discriminant) >= 0.0, positive, zero);
      const mask_of<value_lane> valid_radius = mf61_values(radius).isFinite() && (mf61_values(radius) > 0.0);
      const mask_of<value_lane> valid_external =
          mf61_values(external).isFinite() && (mf61_values(external) * std::copysign(1.0, c_->FNOMIN) > 0.0);
      const mask_of<value_lane> valid_denominator =
          (mf61_values(discriminant) < 0.0) || (mf61_values(abs_lane(denominator)) > 0.0);
      const mask_of<value_lane> valid =
          valid_radius && valid_external && valid_denominator && mf61_values(discriminant).isFinite();
      const Lane_T failed = constant(std::numeric_limits<double>::quiet_NaN());
      return select_lanes(valid, physical, failed);
    }

  private:
    [[nodiscard]] Lane_T sin_lane(const Lane_T &x) const noexcept {
      return x.sin();
    }
    [[nodiscard]] Lane_T cos_lane(const Lane_T &x) const noexcept {
      return x.cos();
    }
    [[nodiscard]] Lane_T atan_lane(const Lane_T &x) const noexcept {
      return x.atan();
    }
    [[nodiscard]] Lane_T tan_lane(const Lane_T &x) const noexcept {
      return x.sin() / x.cos();
    }
    [[nodiscard]] Lane_T exp_lane(const Lane_T &x) const noexcept {
      return x.exp();
    }
    [[nodiscard]] Lane_T sqrt_lane(const Lane_T &x) const noexcept {
      return mf61_sqrt(x);
    }
    [[nodiscard]] Lane_T abs_lane(const Lane_T &x) const noexcept {
      return mf61_abs(x);
    }
    [[nodiscard]] Lane_T sign_lane(const Lane_T &x) const noexcept {
      return sgn_plus(x);
    }
    [[nodiscard]] Lane_T pow_lane(const Lane_T &x, const Lane_T &exponent) const noexcept {
      return (x.abs().log() * exponent).exp();
    }
    [[nodiscard]] Lane_T acos_lane(const Lane_T &x) const noexcept {
      const Lane_T minus = constant(-1.0);
      const Lane_T plus = constant(1.0);
      const Lane_T lower = select_lanes(mf61_values(x) < -1.0, minus, x);
      const Lane_T limited = select_lanes(mf61_values(lower) > 1.0, plus, lower);
      return limited.acos();
    }
    const mf61_coefficients *c_;
    Eigen::Index n_;
    bool turn_slip_;
  };
} // namespace mf::detail
