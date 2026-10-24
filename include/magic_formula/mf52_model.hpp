#pragma once

#include <expected>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <magic_formula/model_layout.hpp>
#include <string_view>

namespace mf {
  /// MF 5.2 (FITTYP 6). It shares the MF 5.x kernels with PAC2002 and differs only in its forms: the trail cosine
  /// cos'(alpha) and F'y = Fy - SVyk0, plus the Vx > 0 domain and the fixed camber scaling LGAX = LGAY = LGAZ = 1. The
  /// combined forces are always the cosine method. Equations: docs/research/mf52.md.
  class mf52_model final : public mf5x_model {
  public:
    /// Forces LGAX = LGAY = LGAZ = 1 (these are not MF 5.2 parameters), then validates the parameters
    /// (`validate_parameters`; the first error is returned unchanged) and builds the model.
    /// @param parameters coefficients, limits and metadata
    /// @param options slip mode and the throw-on-failure flag, fixed for the lifetime of the model
    [[nodiscard]] static std::expected<mf52_model, error> create(mf5x_parameters parameters,
                                                                 const model_options &options = {});

    /// "MF 5.2".
    [[nodiscard]] std::string_view name() const noexcept override;

    /// `mf5x::MF52_LAYOUT`: Vx > 0 (0 and -0.0 are outside the domain).
    [[nodiscard]] const model_layout &layout() const noexcept override;

  private:
    mf52_model(const mf5x_parameters &parameters, const model_options &options);
  };
} // namespace mf
