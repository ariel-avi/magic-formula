#pragma once

#include <expected>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/tire_model.hpp>

namespace mf {
  /// Abstract MF 5.x-family formulation: the shared steady-state evaluation order of PAC2002
  /// (docs/research/pac2002.md section 2) and MF 5.2 ([impl], docs/research/mf52.md section 4), built from the
  /// kernels of `mf::detail`. The evaluation order is fixed (`compute` is final); the protected hooks sit at exactly
  /// the documented PAC2002 / MF 5.2 differences (docs/research/pac2002-vs-mf52.md). A derived class overrides a hook
  /// only to express one row of that table.
  class mf5x_model : public tire_model {
  public:
    /// The coefficient set this model was built from.
    [[nodiscard]] const mf5x_coefficients &coefficients() const noexcept;

  protected:
    /// Stores the coefficients, the limits and the metadata. `parameters` has already passed `validate_parameters`.
    explicit mf5x_model(const mf5x_parameters &parameters);

    /// `validate_coefficients` followed by `validate_limits` (AD-12, AD-15). The first error is returned unchanged,
    /// including its `where.key`. Used by the `create` functions of the derived classes.
    [[nodiscard]] static std::expected<void, error> validate_parameters(const mf5x_parameters &parameters);

    /// Cosine factor of the pneumatic trail t, eqs (44) and (79): cos(alpha) for PAC2002, cos'(alpha) for MF 5.2
    /// (pac2002-vs-mf52.md row "cos factor in t"; conventions.md section 6).
    [[nodiscard]] virtual double trail_cosine(const operating_point &point) const noexcept = 0;

    /// Combined forces Fx and Fy (and the SVyk contained in Fy). Default: the cosine method, eqs (58)-(77), also the
    /// MF 5.2 form (`detail::cosine_combined_forces`; mf52.md section 4 step 7).
    [[nodiscard]] virtual detail::combined_forces combine(const operating_point &point,
                                                          const detail::slip_state &state) const noexcept;

    /// F'y of eqs (78) and (81), the force that multiplies the trail in Mz. Default: Fy - SVyk (PAC2002 reading A,
    /// D-16; zero SVyk for the friction ellipse). pac2002-vs-mf52.md row "F'y".
    [[nodiscard]] virtual double aligning_lateral_force(const operating_point &point, const detail::slip_state &state,
                                                        const detail::combined_forces &forces) const noexcept;

    /// Output rolling-resistance moment My. Default: the negative of eq (87) (`detail::rolling_resistance_moment`;
    /// AD-17), also the MF 5.2 form (mf52.md section 4 step 9). `fx_n` is the Fx of the evaluated mode.
    [[nodiscard]] virtual double rolling_resistance(const operating_point &point, const detail::slip_state &state,
                                                    double fx_n) const noexcept;

  private:
    /// Evaluation order (pac2002.md section 2 = mf52.md section 4). Pure-slip terms first, then, for the combined mode,
    /// `combine`, the equivalent slip angles (84)-(85) (D-05), Mz of eq (78) with the moment arm of Fx (D-08, D-09),
    /// then Mx (86) and My. Uncombined mode (D-26): Fx0, Fy0 and Mz0 = -t(alpha_t)*Fy0 + Mzr(alpha_r).
    [[nodiscard]] tire_outputs compute(const operating_point &point, slip_mode mode) const noexcept final;

    mf5x_coefficients coefficients_;
  };
} // namespace mf
