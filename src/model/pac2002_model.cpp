#include <cmath>
#include <expected>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <magic_formula/pac2002_model.hpp>
#include <string_view>

namespace mf {
  namespace {
    /// True for FITTYP 5 with QSY1 = QSY2 = 0. Each operand is driven both ways by tests.
    bool selects_my_from_shifts(const mf5x_parameters &parameters) {
      const mf5x_coefficients &c = parameters.coefficients;
      return parameters.metadata.fittyp == 5 && c.QSY1 == 0.0 && c.QSY2 == 0.0;
    }
  } // namespace

  std::expected<pac2002_model, error> pac2002_model::create(const mf5x_parameters &parameters, combined_method method) {
    if (const std::expected<void, error> valid = validate_parameters(parameters); !valid.has_value()) {
      return std::unexpected<error>(valid.error());
    }
    return pac2002_model(parameters, method);
  }

  pac2002_model::pac2002_model(const mf5x_parameters &parameters, combined_method method)
      : mf5x_model(parameters), method_(method), my_from_shifts_(selects_my_from_shifts(parameters)) {}

  std::string_view pac2002_model::name() const noexcept {
    return "PAC2002";
  }

  combined_method pac2002_model::method() const noexcept {
    return method_;
  }

  bool pac2002_model::uses_my_from_shifts() const noexcept {
    return my_from_shifts_;
  }

  double pac2002_model::trail_cosine(const operating_point &point) const noexcept {
    return std::cos(point.alpha_rad);
  }

  detail::combined_forces pac2002_model::combine(const operating_point &point,
                                                 const detail::slip_state &state) const noexcept {
    if (method_ == combined_method::FRICTION_ELLIPSE) {
      const detail::ellipse_ratios ratios = detail::friction_ellipse_ratios(state, point.kappa, point.alpha_rad);
      const double fx_n = state.lon.fx0_n * ratios.rx;
      const double fy_n = state.lat.fy0_n * ratios.ry;
      return detail::combined_forces{.fx_n = fx_n, .fy_n = fy_n, .svyk_n = 0.0};
    }
    return mf5x_model::combine(point, state);
  }

  double pac2002_model::my(const operating_point &point, const detail::slip_state &state, double fx_n) const noexcept {
    if (my_from_shifts_) {
      return detail::my_from_shifts(coefficients(), state.lon);
    }
    return mf5x_model::my(point, state, fx_n);
  }
} // namespace mf
