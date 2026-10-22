#include <cmath>
#include <expected>
#include <format>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf52_model.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace mf {
  namespace {
    /// Location that names only the VX field. Built through a named local and a move so that the `make_error` call
    /// site carries no exception-cleanup branches of its own.
    location vx_location() {
      std::string key("VX");
      return location{.file = {}, .line = 0, .key = std::move(key)};
    }
  } // namespace

  std::expected<mf52_model, error> mf52_model::create(mf5x_parameters parameters) {
    parameters.coefficients.LGAX = 1.0;
    parameters.coefficients.LGAY = 1.0;
    parameters.coefficients.LGAZ = 1.0;
    if (const std::expected<void, error> valid = validate_parameters(parameters); !valid.has_value()) {
      return std::unexpected<error>(valid.error());
    }
    return mf52_model(parameters);
  }

  mf52_model::mf52_model(const mf5x_parameters &parameters) : mf5x_model(parameters) {}

  std::string_view mf52_model::name() const noexcept {
    return "MF 5.2";
  }

  std::expected<void, error> mf52_model::validate_input(const operating_point &point) const {
    if (const std::expected<void, error> valid = tire_model::validate_input(point); !valid.has_value()) {
      return std::unexpected<error>(valid.error());
    }
    // `<=` also rejects -0.0.
    if (point.vx_mps <= 0.0) {
      return make_error(error_code::INVALID_INPUT, std::format("Vx = {} m/s: MF 5.2 requires Vx > 0", point.vx_mps),
                        vx_location());
    }
    return {};
  }

  double mf52_model::trail_cosine(const operating_point &point) const noexcept {
    // cos'(alpha) = Vx/(Vc + eps), with Vsy = tan(alpha)*Vx.
    const double vx = point.vx_mps;
    return vx / (std::hypot(vx, std::tan(point.alpha_rad) * vx) + detail::EPSILON);
  }

  double mf52_model::fy_prime(const operating_point &point, const detail::slip_state &state,
                              const detail::combined_forces &forces) const noexcept {
    const mf5x_coefficients &c = coefficients();
    const double mu_y_zero_camber = detail::mu_y(c, state.load, 0.0);
    const double svyk0_n =
        detail::svyk(c, state.load, mu_y_zero_camber, point.fz_n, point.kappa, point.alpha_rad, point.gamma_rad);
    return forces.fy_n - svyk0_n;
  }
} // namespace mf
