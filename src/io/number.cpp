#include <charconv>
#include <cmath>
#include <limits>
#include <magic_formula/io/number.hpp>
#include <optional>
#include <string_view>
#include <system_error>

namespace mf::io {
  std::optional<double> parse_number(std::string_view text) noexcept {
    const bool had_plus = text.starts_with('+');
    if (had_plus) {
      text.remove_prefix(1);
    }
    if (had_plus && text.starts_with('-')) {
      return std::nullopt;
    }
    double value = 0.0;
    const char *const first = text.data();
    const char *const last = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(first, last, value, std::chars_format::general);
    if (ec == std::errc{} && ptr == last && std::isfinite(value)) {
      return value;
    }
    return std::nullopt;
  }

  std::optional<int> parse_integer(std::string_view text) noexcept {
    const std::optional<double> parsed = parse_number(text);
    if (!parsed.has_value()) {
      return std::nullopt;
    }
    const double value = *parsed;
    if (value != std::trunc(value)) {
      return std::nullopt;
    }
    constexpr auto int_min = static_cast<double>(std::numeric_limits<int>::lowest());
    constexpr auto int_max = static_cast<double>(std::numeric_limits<int>::max());
    if (value < int_min || value > int_max) {
      return std::nullopt;
    }
    return static_cast<int>(value);
  }
} // namespace mf::io
