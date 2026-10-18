#include <expected>
#include <format>
#include <magic_formula/error.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace mf {
  namespace {
    /// INVALID_VALUE for coefficient `name` that violates `rule` (AD-12); the key is the member name.
    std::unexpected<error> invalid_coefficient(std::string_view name, std::string_view rule, double value) {
      // The location is built in a named variable: a designated-initialiser temporary inside the call makes GCC emit
      // a cleanup block with two branches that no test can reach.
      location where;
      where.key = std::string{name};
      return make_error(error_code::INVALID_VALUE, std::format("{} must be {} (got {}) (AD-12)", name, rule, value),
                        std::move(where));
    }
  } // namespace

  std::expected<void, error> validate_coefficients(const mf5x_coefficients &coefficients) {
    // The "!(x > 0)" form also rejects NaN.
    if (!(coefficients.FNOMIN > 0.0)) {
      return invalid_coefficient("FNOMIN", "> 0", coefficients.FNOMIN);
    }
    if (!(coefficients.UNLOADED_RADIUS > 0.0)) {
      return invalid_coefficient("UNLOADED_RADIUS", "> 0", coefficients.UNLOADED_RADIUS);
    }
    if (!(coefficients.LFZO > 0.0)) {
      return invalid_coefficient("LFZO", "> 0", coefficients.LFZO);
    }
    if (coefficients.PKY2 == 0.0) {
      return invalid_coefficient("PKY2", "non-zero", coefficients.PKY2);
    }
    if (coefficients.LMUY == 0.0) {
      return invalid_coefficient("LMUY", "non-zero", coefficients.LMUY);
    }
    if (!(coefficients.LONGVL > 0.0)) {
      return invalid_coefficient("LONGVL", "> 0", coefficients.LONGVL);
    }
    return {};
  }
} // namespace mf
