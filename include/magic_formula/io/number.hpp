#pragma once

#include <optional>
#include <string_view>

namespace mf::io {
  /// Parses a finite decimal number. Grammar: `std::from_chars` general format
  /// (C locale, no whitespace, no hexadecimal) with one optional leading '+' that must not be followed by '-'.
  /// The whole text must match. Rejects "", "+", "+-1", "++1", inf/nan, "1,5", "1e", ".", "1.0D-3", blanks, and values
  /// that overflow or underflow a double ("1e999", "1e-400"). Examples: "2.1e+005", "-0.0", "+.5", "1.".
  [[nodiscard]] std::optional<double> parse_number(std::string_view text) noexcept;

  /// `parse_number` restricted to values that are integral and within the range of `int` ("6" and "6.0" give 6;
  /// "5.5" and "1e10" give no value).
  [[nodiscard]] std::optional<int> parse_integer(std::string_view text) noexcept;
} // namespace mf::io
