#include <expected>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_forms.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/pac2002_model.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <string_view>

namespace mf {
  namespace {
    /// True for FITTYP 5 with QSY1 = QSY2 = 0. Each operand is driven both ways by tests.
    bool selects_my_from_shifts(const mf5x_parameters &parameters) {
      const mf5x_coefficients &c = parameters.coefficients;
      return parameters.metadata.fittyp == 5 && c.QSY1 == 0.0 && c.QSY2 == 0.0;
    }

    /// The PAC2002 forms: cos(alpha) trail, F'y = Fy - SVyk, the given combined method, and the shift form of My when
    /// the parameters select it.
    mf5x_forms pac2002_forms(const mf5x_parameters &parameters, combined_method method) {
      return mf5x_forms{.trail_cosine = trail_cosine_form::COS_ALPHA,
                        .combined = method,
                        .fy_prime = fy_prime_form::SVYK,
                        .rolling_resistance = selects_my_from_shifts(parameters) ? rolling_resistance_form::SHIFTS
                                                                                 : rolling_resistance_form::QSY};
    }
  } // namespace

  std::expected<pac2002_model, error> pac2002_model::create(const mf5x_parameters &parameters, combined_method method,
                                                            const model_options &options) {
    if (const std::expected<void, error> valid = validate_parameters(parameters); !valid.has_value()) {
      return std::unexpected<error>(valid.error());
    }
    return pac2002_model(parameters, pac2002_forms(parameters, method), options);
  }

  pac2002_model::pac2002_model(const mf5x_parameters &parameters, const mf5x_forms &forms, const model_options &options)
      : mf5x_model(parameters, forms, options) {}

  std::unique_ptr<tire_model> pac2002_model::with_parameters(const mf5x_parameters &parameters) const {
    return std::make_unique<pac2002_model>(pac2002_model(parameters, forms(), options()));
  }

  std::string_view pac2002_model::name() const noexcept {
    return "PAC2002";
  }

  const model_layout &pac2002_model::layout() const noexcept {
    return mf5x::PAC2002_LAYOUT;
  }

  combined_method pac2002_model::method() const noexcept {
    return forms().combined;
  }

  bool pac2002_model::uses_my_from_shifts() const noexcept {
    return forms().rolling_resistance == rolling_resistance_form::SHIFTS;
  }
} // namespace mf
