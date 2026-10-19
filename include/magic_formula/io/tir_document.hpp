#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <magic_formula/error.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mf::io {
  /// One `KEY = value` line of a .tir file (docs/research/tir-format.md section 2, AD-20).
  struct tir_entry {
    /// Upper-case name of the enclosing section (for example "UNITS").
    std::string section;
    /// Upper-case key.
    std::string key;
    /// Value text. A quoted value holds the text between the quotes verbatim; an unquoted value has its trailing `$`
    /// comment cut and its blanks trimmed (it may be empty).
    std::string value;
    /// True when the value was written in single quotes.
    bool quoted = false;
    /// 1-based line of the entry.
    std::size_t line = 0;
  };

  /// A table section such as [SHAPE]: a section whose first content line starts with '{'.
  struct tir_table {
    /// Upper-case section name.
    std::string section;
    /// 1-based line of the section header.
    std::size_t line = 0;
    /// Content lines after the `{...}` column header, trailing `$` comment cut and blanks trimmed, otherwise raw.
    std::vector<std::string> rows;
  };

  /// Keys whose identity is (section, key) instead of the key alone, so they may appear once per section. MASS occurs
  /// in [UNITS] (a unit name) and in [INERTIA] (a number).
  inline constexpr std::array<std::string_view, 1> SECTION_SCOPED_KEYS{"MASS"};

  /// Syntax-level view of a .tir file: sections, `KEY = value` entries and table sections, each with its line number.
  /// Lexical rules: docs/research/tir-format.md section 2 and AD-20 (docs/research/decisions.md). The document knows
  /// nothing about the meaning of the keys; `tir_reader` provides typed access.
  class tir_document {
  public:
    /// Parses `.tir` text.
    ///
    /// Rules: a NUL byte is a SYNTAX error; a leading UTF-8 BOM is skipped; lines end at '\n' (a trailing '\r' is
    /// blank); blank lines and lines starting with '$' or '!' are skipped; `[name]` opens a section (name trimmed and
    /// upper-cased, only blanks or a `$` comment may follow); content before the first section is a SYNTAX error;
    /// the first content line of a section starting with '{' makes it a table section (rows kept raw, '=' is a SYNTAX
    /// error); other lines are `KEY = value` with `[A-Za-z0-9_]+` keys, single-quoted or unquoted values (a `$` ends
    /// an unquoted value; '!' does not). Keys are stored upper-case. A repeated key is DUPLICATE_KEY (naming the first
    /// line), except MASS per section (`SECTION_SCOPED_KEYS`). A repeated table section is a warning and the first
    /// one wins. Empty or comment-only text is an empty document.
    /// @param text file content
    /// @param file_name name used in error and warning locations
    /// @return the document, or the first SYNTAX / DUPLICATE_KEY error with file and 1-based line
    [[nodiscard]] static std::expected<tir_document, error> parse(std::string_view text, std::string file_name = {});

    /// First entry (file order, any section) whose key equals `key`, ignoring ASCII case; nullptr when absent.
    [[nodiscard]] const tir_entry *find(std::string_view key) const noexcept;

    /// Entry whose section and key equal `section` and `key`, ignoring ASCII case; nullptr when absent.
    [[nodiscard]] const tir_entry *find(std::string_view section, std::string_view key) const noexcept;

    /// All `KEY = value` entries in file order.
    [[nodiscard]] std::span<const tir_entry> entries() const noexcept;

    /// All table sections in file order (duplicates excluded).
    [[nodiscard]] std::span<const tir_table> tables() const noexcept;

    /// Parse warnings (duplicate table sections).
    [[nodiscard]] std::span<const warning> warnings() const noexcept;

    /// File name given to `parse` (empty when none).
    [[nodiscard]] const std::string &file_name() const noexcept;

  private:
    tir_document() = default;

    std::string file_name_;
    std::vector<tir_entry> entries_;
    std::vector<tir_table> tables_;
    std::vector<warning> warnings_;
  };

  /// Reads and parses a .tir file (`read_text_file` then `tir_document::parse` with `path.string()` as file name).
  /// @return the document, an IO error, or a SYNTAX / DUPLICATE_KEY error
  [[nodiscard]] std::expected<tir_document, error> read_tir_file(const std::filesystem::path &path);
} // namespace mf::io
