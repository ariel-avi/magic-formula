#include <algorithm>
#include <cstddef>
#include <magic_formula/detail/text.hpp>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

namespace mf::detail {
  namespace {
    /// Maps 'a'..'z' to 'A'..'Z'; every other byte is returned unchanged.
    char upper_char(char ch) noexcept {
      return (ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - 'a' + 'A') : ch;
    }

    /// ASCII case-insensitive character equality.
    bool equal_char(char lhs, char rhs) noexcept {
      return upper_char(lhs) == upper_char(rhs);
    }
  } // namespace

  std::string_view trim(std::string_view text) noexcept {
    constexpr std::string_view blanks = BLANKS;
    const std::size_t first = text.find_first_not_of(blanks);
    if (first == std::string_view::npos) {
      return {};
    }
    const std::size_t last = text.find_last_not_of(blanks);
    text.remove_suffix(text.size() - last - 1);
    text.remove_prefix(first);
    return text;
  }

  std::string to_upper_ascii(std::string_view text) {
    return text | std::views::transform(upper_char) | std::ranges::to<std::string>();
  }

  bool iequals_ascii(std::string_view lhs, std::string_view rhs) noexcept {
    return lhs.size() == rhs.size() && std::ranges::equal(lhs, rhs, equal_char);
  }

  std::string join(std::span<const std::string> items, std::string_view separator) {
    return items | std::views::join_with(separator) | std::ranges::to<std::string>();
  }
} // namespace mf::detail
