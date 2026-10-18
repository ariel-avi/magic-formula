#pragma once

#include <span>
#include <string>
#include <string_view>

namespace mf::detail {
  /// Returns `text` without leading and trailing blanks (' ', '\t', '\r', '\v', '\f'). The result views `text`.
  [[nodiscard]] std::string_view trim(std::string_view text) noexcept;

  /// Returns a copy of `text` with 'a'..'z' mapped to 'A'..'Z'. Every other byte is unchanged (locale independent).
  [[nodiscard]] std::string to_upper_ascii(std::string_view text);

  /// ASCII case-insensitive equality (locale independent).
  [[nodiscard]] bool iequals_ascii(std::string_view lhs, std::string_view rhs) noexcept;

  /// Concatenates `items`, inserting `separator` between neighbours. No items give an empty string.
  [[nodiscard]] std::string join(std::span<const std::string> items, std::string_view separator);
} // namespace mf::detail
