#pragma once

#include <cstdint>
#include <expected>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <string_view>

namespace mf {
  /// How the PAC2002 combined forces are computed.
  enum class combined_method : std::uint8_t {
    COSINE,          ///< weighting functions Gxa and Gyk plus SVyk (R* keys present)
    FRICTION_ELLIPSE ///< friction ellipse with SVyk = 0 (no R* keys)
  };

  /// PAC2002. Overrides the MF 5.x hooks that differ from MF 5.2: the trail cosine is cos(alpha), the combined forces
  /// may use the friction ellipse, F'y = Fy - SVyk, and My may use the shift form. Accepts Vx = 0. Equations:
  /// docs/research/pac2002.md.
  class pac2002_model final : public mf5x_model {
  public:
    /// Builds a model after `validate_parameters` (the first coefficient or limits error is returned unchanged).
    /// My uses the shift form instead of the QSY form when `metadata.fittyp == 5` and QSY1 = QSY2 = 0.
    /// @param parameters coefficients, limits and metadata
    /// @param method combined-slip method (the loader chooses it from the R* keys)
    [[nodiscard]] static std::expected<pac2002_model, error> create(const mf5x_parameters &parameters,
                                                                    combined_method method = combined_method::COSINE);

    /// "PAC2002".
    [[nodiscard]] std::string_view name() const noexcept override;

    /// The combined-slip method of this model.
    [[nodiscard]] combined_method method() const noexcept;

    /// True when My is computed from the longitudinal shifts instead of the QSY form.
    [[nodiscard]] bool uses_my_from_shifts() const noexcept;

  private:
    pac2002_model(const mf5x_parameters &parameters, combined_method method);

    /// cos(alpha).
    [[nodiscard]] double trail_cosine(const operating_point &point) const noexcept override;

    /// Friction ellipse for `combined_method::FRICTION_ELLIPSE`: Fx = Fx0*rx, Fy = Fy0*ry, SVyk = 0 (so F'y = Fy).
    /// Otherwise the cosine method of the base.
    [[nodiscard]] detail::combined_forces combine(const operating_point &point,
                                                  const detail::slip_state &state) const noexcept override;

    /// `detail::my_from_shifts` when `uses_my_from_shifts()`, otherwise the base `detail::my`.
    [[nodiscard]] double my(const operating_point &point, const detail::slip_state &state,
                            double fx_n) const noexcept override;

    combined_method method_;
    bool my_from_shifts_;
  };
} // namespace mf
