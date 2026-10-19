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
  /// How the PAC2002 combined forces are computed (D-07, AD-07; docs/research/pac2002.md sections 3 and 5.4).
  enum class combined_method : std::uint8_t {
    COSINE,          ///< weighting functions Gxa and Gyk plus SVyk, eqs (58)-(77) (R* keys present)
    FRICTION_ELLIPSE ///< MSC friction ellipse, p.33-34, with SVyk = 0 (no R* keys)
  };

  /// PAC2002 (MSC "Using the PAC2002 Tire Model"), docs/research/pac2002.md sections 2-5. Overrides the MF 5.x hooks
  /// that differ from MF 5.2 (pac2002-vs-mf52.md): the trail cosine is cos(alpha), the combined forces may use the
  /// friction ellipse, F'y = Fy - SVyk (reading A), and My may use eq (88). Accepts Vx = 0 (AD-02).
  class pac2002_model final : public mf5x_model {
  public:
    /// Builds a model after `validate_parameters` (the first coefficient or limits error is returned unchanged).
    /// Eq (88) replaces eq (87) for My when `metadata.fittyp == 5` and QSY1 = QSY2 = 0 (MSC p.33, AD-17).
    /// @param parameters coefficients, limits and metadata
    /// @param method combined-slip method (the loader chooses it from the R* keys, AD-07)
    [[nodiscard]] static std::expected<pac2002_model, error> create(const mf5x_parameters &parameters,
                                                                    combined_method method = combined_method::COSINE);

    /// "PAC2002".
    [[nodiscard]] std::string_view name() const noexcept override;

    /// The combined-slip method of this model.
    [[nodiscard]] combined_method method() const noexcept;

    /// True when My is computed with eq (88) instead of eq (87).
    [[nodiscard]] bool uses_eq88_rolling_resistance() const noexcept;

  private:
    pac2002_model(const mf5x_parameters &parameters, combined_method method);

    /// cos(alpha), eq (44) (pac2002-vs-mf52.md row "cos factor in t").
    [[nodiscard]] double trail_cosine(const operating_point &point) const noexcept override;

    /// Friction ellipse for `combined_method::FRICTION_ELLIPSE`: Fx = Fx0*rx, Fy = Fy0*ry, SVyk = 0 (MSC p.33-34,
    /// eqs (76)-(85) with SVyk = 0, so F'y = Fy; pac2002.md section 5.4). Otherwise the cosine method of the base.
    [[nodiscard]] detail::combined_forces combine(const operating_point &point,
                                                  const detail::slip_state &state) const noexcept override;

    /// The negative of eq (88) when `uses_eq88_rolling_resistance()`, otherwise the negative of eq (87) from the base
    /// (pac2002-vs-mf52.md row "My"; AD-17).
    [[nodiscard]] double rolling_resistance(const operating_point &point, const detail::slip_state &state,
                                            double fx_n) const noexcept override;

    combined_method method_;
    bool eq88_;
  };
} // namespace mf
