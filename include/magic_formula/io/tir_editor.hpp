#pragma once
#include <expected>
#include <magic_formula/error.hpp>
#include <string>
#include <string_view>

namespace mf::io {
  /// Returns a copy of .tir `text` with the value of the entry (`section`, `key`) replaced by `value`. Every other byte
  /// (comments, blanks, line endings, a leading BOM, other entries, table sections) is unchanged. A quoted entry stays
  /// quoted (`'value'` is written), an unquoted entry gets `value` verbatim. The new text is parsed again and the entry
  /// must read back exactly `value` with the same quoting; the lexical rules therefore live only in
  /// `tir_document::parse`.
  /// @param text .tir text
  /// @param section section of the entry (ASCII case-insensitive)
  /// @param key key of the entry (ASCII case-insensitive)
  /// @param value new value text, without quotes
  /// @return the new text; or the SYNTAX / DUPLICATE_KEY error of parsing `text`; MISSING_KEY (line 0, upper-case
  ///         key) when there is no such entry; INVALID_VALUE (line and key of the entry) when `value` contains '\n' or
  ///         '\r', or the edited text does not parse, or the entry does not read back unchanged (for example a quote in
  ///         a quoted value, '$' or surrounding blanks in an unquoted value, a quoted text for an unquoted entry)
  [[nodiscard]] std::expected<std::string, error> replace_value(std::string_view text, std::string_view section,
                                                                std::string_view key, std::string_view value);
} // namespace mf::io
