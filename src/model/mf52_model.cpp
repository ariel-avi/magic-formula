#include <expected>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf52_model.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_forms.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <string_view>

namespace mf {
  namespace {
    /// The MF 5.2 forms: cos'(alpha) trail, cosine-method combined forces, F'y = Fy - SVyk0, QSY form of My.
    constexpr mf5x_forms MF52_FORMS{.trail_cosine = trail_cosine_form::COS_PRIME,
                                    .combined = combined_method::COSINE,
                                    .fy_prime = fy_prime_form::SVYK0,
                                    .rolling_resistance = rolling_resistance_form::QSY};
  } // namespace

  std::expected<mf52_model, error> mf52_model::create(mf5x_parameters parameters, const model_options &options) {
    parameters.coefficients.LGAX = 1.0;
    parameters.coefficients.LGAY = 1.0;
    parameters.coefficients.LGAZ = 1.0;
    if (const std::expected<void, error> valid = validate_parameters(parameters); !valid.has_value()) {
      return std::unexpected<error>(valid.error());
    }
    return mf52_model(parameters, options);
  }

  mf52_model::mf52_model(const mf5x_parameters &parameters, const model_options &options)
      : mf5x_model(parameters, MF52_FORMS, options) {}

  std::unique_ptr<tire_model> mf52_model::with_parameters(const mf5x_parameters &parameters) const {
    return std::make_unique<mf52_model>(mf52_model(parameters, options()));
  }

  std::string_view mf52_model::name() const noexcept {
    return "MF 5.2";
  }

  const model_layout &mf52_model::layout() const noexcept {
    return mf5x::MF52_LAYOUT;
  }
} // namespace mf
