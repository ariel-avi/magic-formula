#pragma once

#include <expected>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_forms.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <string_view>

namespace mf {
  /// PAC2002. Its forms differ from MF 5.2: the trail cosine is cos(alpha), the combined forces may use the friction
  /// ellipse, F'y = Fy - SVyk, and My may use the shift form. Accepts Vx = 0. Equations: docs/research/pac2002.md.
  class pac2002_model final : public mf5x_model {
  public:
    /// Builds a model after `validate_parameters` (the first coefficient or limits error is returned unchanged).
    /// My uses the shift form instead of the QSY form when `metadata.fittyp == 5` and QSY1 = QSY2 = 0.
    /// @param parameters coefficients, limits and metadata
    /// @param method combined-slip method (the loader chooses it from the R* keys)
    /// @param options slip mode and the throw-on-failure flag, fixed for the lifetime of the model
    [[nodiscard]] static std::expected<pac2002_model, error> create(const mf5x_parameters &parameters,
                                                                    combined_method method = combined_method::COSINE,
                                                                    const model_options &options = {});

    /// "PAC2002".
    [[nodiscard]] std::string_view name() const noexcept override;

    /// `mf5x::PAC2002_LAYOUT`: Vx >= 0 is the only domain restriction.
    [[nodiscard]] const model_layout &layout() const noexcept override;

    /// The combined-slip method of this model.
    [[nodiscard]] combined_method method() const noexcept;

    /// True when My is computed from the longitudinal shifts instead of the QSY form.
    [[nodiscard]] bool uses_my_from_shifts() const noexcept;

  private:
    /// A PAC2002 model with `parameters`, this model's forms and options.
    [[nodiscard]] std::unique_ptr<tire_model> with_parameters(const mf5x_parameters &parameters) const final;

    pac2002_model(const mf5x_parameters &parameters, const mf5x_forms &forms, const model_options &options);
  };
} // namespace mf
