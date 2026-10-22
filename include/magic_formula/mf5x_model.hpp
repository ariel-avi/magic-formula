#pragma once

#include <expected>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/tire_model.hpp>

namespace mf {
  /// Abstract MF 5.x-family formulation: the shared steady-state evaluation order of PAC2002 and MF 5.2, built from
  /// the kernels of `mf::detail`. The evaluation order is fixed (`compute` is final); the protected hooks sit at
  /// exactly the PAC2002 / MF 5.2 differences. Equations: docs/research/pac2002-vs-mf52.md.
  class mf5x_model : public tire_model {
  public:
    /// The coefficient set this model was built from.
    [[nodiscard]] const mf5x_coefficients &coefficients() const noexcept;

  protected:
    /// Stores the coefficients, the limits and the metadata. `parameters` has already passed `validate_parameters`.
    explicit mf5x_model(const mf5x_parameters &parameters);

    /// `validate_coefficients` followed by `validate_limits`. The first error is returned unchanged,
    /// including its `where.key`. Used by the `create` functions of the derived classes.
    [[nodiscard]] static std::expected<void, error> validate_parameters(const mf5x_parameters &parameters);

    /// Cosine factor of the pneumatic trail t: cos(alpha) for PAC2002, cos'(alpha) for MF 5.2.
    [[nodiscard]] virtual double trail_cosine(const operating_point &point) const noexcept = 0;

    /// Combined forces Fx and Fy (and the SVyk contained in Fy). Default: the cosine method
    /// (`detail::cosine_combined_forces`), also the MF 5.2 form.
    [[nodiscard]] virtual detail::combined_forces combine(const operating_point &point,
                                                          const detail::slip_state &state) const noexcept;

    /// F'y, the force that multiplies the trail in Mz. Default: Fy - SVyk (zero SVyk for the friction ellipse).
    [[nodiscard]] virtual double fy_prime(const operating_point &point, const detail::slip_state &state,
                                          const detail::combined_forces &forces) const noexcept;

    /// Output rolling-resistance moment My. Default: `detail::my`, also the MF 5.2 form. `fx_n` is the Fx of the
    /// evaluated mode.
    [[nodiscard]] virtual double my(const operating_point &point, const detail::slip_state &state,
                                    double fx_n) const noexcept;

  private:
    /// Evaluation order: pure-slip terms first, then, for the combined mode, `combine`, the equivalent slip angles,
    /// Mz with the moment arm of Fx, then Mx and My. Uncombined mode: Fx0, Fy0 and Mz0 = -t(alpha_t)*Fy0 +
    /// Mzr(alpha_r).
    [[nodiscard]] tire_outputs compute(const operating_point &point, slip_mode mode) const noexcept final;

    mf5x_coefficients coefficients_;
  };
} // namespace mf
