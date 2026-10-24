#pragma once

#include <Eigen/Core>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <magic_formula/error.hpp>
#include <magic_formula/model_layout.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mf::io {
  /// A parsed input table: the columns exactly as the header names them, one matrix row per data row.
  struct input_table {
    /// File name used in errors.
    std::string file;
    /// Upper-case header names in header order (each of FZ SA SR IA VX P).
    std::vector<std::string> columns;
    /// rows x columns, SI units (after the unit-line conversion), unclamped, exactly as read.
    Eigen::MatrixXd values;
    /// 1-based line of every row in the input text.
    std::vector<std::size_t> lines;
    /// 1-based line of the header.
    std::size_t header_line = 0;
  };

  /// A table arranged for one model: the columns in the model's input_index order.
  struct model_inputs {
    /// rows x channels; column k holds the table column named like channel k.
    Eigen::MatrixXd values;
    /// 1-based line of every row in the input text.
    std::vector<std::size_t> lines;
    /// Table columns that no channel uses (for example P for PAC2002), in header order.
    std::vector<std::string> ignored_columns;
  };

  /// Parses the CLI input table. Units are converted to SI once, here.
  ///
  /// Format:
  /// - Text, one row per line. A leading UTF-8 byte order mark is skipped, CR is stripped, blank lines are ignored, and
  ///   a line whose first non-blank character is '#' is a comment. There are no trailing comments. Every other line is
  ///   a content line.
  /// - Fields are separated by any run of ',', ' ' or '\t'; the rule is the same on every line, so the header, the unit
  ///   line and the rows may each use any mix of them. Leading and trailing separators are ignored, so "1,,2" has two
  ///   fields.
  /// - The first content line is the mandatory header: column names only, each one of FZ, SA, SR, IA, VX (all
  ///   required) and P (optional), case-insensitive, in any order, each at most once. A field that contains '[' or ']'
  ///   is the legacy NAME[unit] form and is rejected.
  /// - The next content line is the mandatory unit line: exactly one unit per header column, in header order, compared
  ///   ASCII case-insensitively: FZ N; SA rad|deg; SR -; IA rad|deg; VX m/s; P Pa. Degrees are converted as
  ///   `value * (pi / 180)`, every other unit has the factor 1.
  /// - Every further content line is a data row with exactly as many fields as the header. Each field must be a finite
  ///   number (`mf::io::parse_number`).
  /// - Empty text, or text with only comments and blank lines, has no header. A header and a unit line without rows
  ///   give a 0 x columns matrix.
  ///
  /// Errors carry `file_name`, the 1-based line (0 when there is no header line) and the upper-case column name where
  /// one applies:
  /// - SYNTAX: no header; unknown column; a legacy NAME[unit] header field (the message says to move the unit to the
  ///   unit line); missing unit line (the text ends after the header: located at the header line; the line after the
  ///   header starts with a number: located at that line); a unit count that differs from the column count (an empty
  ///   entry such as "N,,rad" is no unit: separators collapse, so it shows as a missing unit); a row field count.
  /// - DUPLICATE_KEY (column twice), MISSING_KEY (required column absent), UNSUPPORTED_UNIT (a unit that the column
  ///   does not accept, including a unit of another column, such as deg for FZ), INVALID_NUMBER.
  [[nodiscard]] std::expected<input_table, error> parse_input_table(std::string_view text, std::string file_name = {});

  /// Reads `path` (`read_text_file`) and parses it with `parse_input_table`, using `path` as the file name in errors.
  [[nodiscard]] std::expected<input_table, error> read_input_table(const std::filesystem::path &path);

  /// Arranges `table` for a model: column k of the result is the table column named like `channels[k]`; the table
  /// columns that no channel uses are listed in `ignored_columns`.
  /// @return the arranged inputs, or MISSING_KEY (file, header line, channel name; message "missing required column
  ///         <NAME>") for the first channel without a column
  [[nodiscard]] std::expected<model_inputs, error> map_inputs(const input_table &table,
                                                              std::span<const input_channel> channels);

  /// `read_input_table` followed by `map_inputs`.
  [[nodiscard]] std::expected<model_inputs, error> read_model_inputs(const std::filesystem::path &path,
                                                                     std::span<const input_channel> channels);
} // namespace mf::io
