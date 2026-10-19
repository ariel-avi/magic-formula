#pragma once

#include <expected>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <string_view>

namespace mf {
  /// MF 5.2 (FITTYP 6). Every equation of this class rests on the implementation-derived [impl] evidence tier
  /// (docs/research/mf52.md section 4, cross-checked against MFeval 4.0.0 `Solver.m`; no primary MF 5.2 equation text
  /// is available; the user accepted this tier, AD-26). It shares the MF 5.x kernels with PAC2002 and overrides only
  /// the documented differences (pac2002-vs-mf52.md): the trail cosine cos'(alpha), F'y = Fy - SVyk0, the Vx > 0
  /// domain, and the fixed camber scaling LGAX = LGAY = LGAZ = 1. The combined forces are always the cosine method.
  class mf52_model final : public mf5x_model {
  public:
    /// Forces LGAX = LGAY = LGAZ = 1 (these are not MF 5.2 parameters, D-14, AD-19), then validates the parameters
    /// (`validate_parameters`; the first error is returned unchanged) and builds the model.
    [[nodiscard]] static std::expected<mf52_model, error> create(mf5x_parameters parameters);

    /// "MF 5.2".
    [[nodiscard]] std::string_view name() const noexcept override;

  private:
    explicit mf52_model(const mf5x_parameters &parameters);

    /// `tire_model::validate_input`, then Vx > 0 (INVALID_INPUT, key VX; 0 and -0.0 are rejected; D-27, AD-02).
    /// pac2002-vs-mf52.md row "Vx".
    [[nodiscard]] std::expected<void, error> validate_input(const operating_point &point) const override;

    /// [impl] cos'(alpha) = Vx/(Vc + eps) with Vc = hypot(Vx, tan(alpha)*Vx) and eps = 1e-6 (conventions.md section 6;
    /// MFeval `Solver.m:647-682`, Q-19). pac2002-vs-mf52.md row "cos factor in t".
    [[nodiscard]] double trail_cosine(const operating_point &point) const noexcept override;

    /// [impl] F'y = Fy - SVyk0, where SVyk0 is SVyk with mu_y at gamma = 0 and every other term at the actual camber
    /// (AD-25; MFeval `Solver.m:1587-1611`). pac2002-vs-mf52.md row "F'y".
    [[nodiscard]] double aligning_lateral_force(const operating_point &point, const detail::slip_state &state,
                                                const detail::combined_forces &forces) const noexcept override;
  };
} // namespace mf
