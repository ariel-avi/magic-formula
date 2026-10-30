#include <array>
#include <cmath>
#include <expected>
#include <format>
#include <magic_formula/error.hpp>
#include <magic_formula/mf61_coefficients.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace mf {
  namespace {
    std::unexpected<error> invalid(std::string_view key, std::string_view rule) {
      location where;
      where.key = std::string(key);
      return make_error(error_code::INVALID_VALUE, std::format("{} must be {}", key, rule), std::move(where));
    }
  } // namespace
  std::expected<void, error> validate_coefficients(const mf61_coefficients &coefficients) {
    for (const mf6x::coefficient_entry &entry : mf6x::COEFFICIENTS) {
      if (!std::isfinite(coefficients.*entry.member)) {
        return invalid(entry.key, "finite");
      }
    }
    if (coefficients.FNOMIN == 0.0) {
      return invalid("FNOMIN", "non-zero");
    }
    constexpr std::array<std::pair<std::string_view, double mf61_coefficients::*>, 6> positive{{
        {"UNLOADED_RADIUS", &mf61_coefficients::UNLOADED_RADIUS},
        {"LONGVL", &mf61_coefficients::LONGVL},
        {"NOMPRES", &mf61_coefficients::NOMPRES},
        {"LFZ0", &mf61_coefficients::LFZ0},
        {"LMUX", &mf61_coefficients::LMUX},
        {"LMUY", &mf61_coefficients::LMUY},
    }};
    for (const auto &[key, member] : positive) {
      if (!(coefficients.*member > 0.0)) {
        return invalid(key, "positive");
      }
    }
    if (!(coefficients.VERTICAL_STIFFNESS / coefficients.FNOMIN > 0.0)) {
      return invalid("VERTICAL_STIFFNESS", "on the FNOMIN side");
    }
    if (coefficients.PKY2 == 0.0) {
      return invalid("PKY2", "non-zero");
    }
    if (coefficients.Q_FZ1 == 0.0 && coefficients.Q_FZ2 == 0.0) {
      return invalid("Q_FZ1", "non-zero when Q_FZ2 is zero");
    }
    return {};
  }
} // namespace mf
