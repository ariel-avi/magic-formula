#pragma once

#include <expected>
#include <magic_formula/error.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mf::io {
  /// Typed access to the entries of a `tir_document` that remembers which entries were read, so the caller can list
  /// the keys nobody asked for (the "ignored keys"). Lookups are case-insensitive. Every accessor marks the
  /// entry it finds as consumed, also when the value then fails to convert.
  /// The reader keeps a pointer to the document: the document must outlive the reader.
  class tir_reader {
  public:
    /// Starts with every entry unconsumed.
    explicit tir_reader(const tir_document &document);

    /// The document this reader was built on.
    [[nodiscard]] const tir_document &document() const noexcept;

    /// True when an entry with this key exists in any section. Marks it consumed.
    [[nodiscard]] bool contains(std::string_view key);

    /// Value text (quoted or bare) of the first entry with this key in any section; nullopt when absent. Marks it
    /// consumed. The view stays valid as long as the document.
    [[nodiscard]] std::optional<std::string_view> text(std::string_view key);

    /// Like `text(key)` but only looks into `section` (for MASS in [UNITS] versus [INERTIA]).
    [[nodiscard]] std::optional<std::string_view> text(std::string_view section, std::string_view key);

    /// Numeric value of `key` (`parse_number`).
    /// @return no value when the key is absent; INVALID_VALUE when the value is quoted; INVALID_NUMBER when the text
    ///         is not a finite number (both with file, line and key)
    [[nodiscard]] std::expected<std::optional<double>, error> number(std::string_view key);

    /// Integral value of `key` (`parse_integer`: 6 and 6.0 give 6).
    /// @return no value when the key is absent; the errors of `number`; INVALID_VALUE when the number is not an
    ///         integer within the range of int
    [[nodiscard]] std::expected<std::optional<int>, error> integer(std::string_view key);

    /// Location of `key` for diagnostics: the document's file name, the line of the first entry with that key (0 when
    /// there is none) and the upper-cased key. Does not mark anything consumed.
    [[nodiscard]] location locate(std::string_view key) const;

    /// Locations (file, line, upper-case key) of all entries whose section equals `section` (ASCII case-insensitive),
    /// in file order. Marks them consumed. Empty when the section is absent or has no entries.
    [[nodiscard]] std::vector<location> take_section(std::string_view section);

    /// Keys of all entries that were never consumed, in file order.
    [[nodiscard]] std::vector<std::string> unconsumed_keys() const;

  private:
    const tir_document *document_;
    std::vector<bool> consumed_;
  };
} // namespace mf::io
