#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mf::io {
  /// One operating point of the CLI input table, in SI units (docs/research/decisions.md, AD-27).
  struct input_row {
    /// Fz [N], SR [-], SA [rad], IA [rad], Vx [m/s]; unclamped, exactly as read (after unit conversion).
    operating_point point;
    /// Inflation pressure [Pa] when the table has a P column. The models have no pressure input (AD-09).
    std::optional<double> pressure_pa;
    /// 1-based line of this row in the input text.
    std::size_t line = 0;
  };

  /// A parsed input table.
  struct input_table {
    /// One entry per data row, in input order.
    std::vector<input_row> rows;
    /// True when the header has a P column.
    bool has_pressure = false;
  };

  /// Parses the CLI input table (AD-27). Units are converted to SI once, here.
  ///
  /// Format:
  /// - Text, one row per line. A leading UTF-8 byte order mark is skipped, CR is stripped, blank lines are ignored, and
  ///   a line whose first non-blank character is '#' is a comment. There are no trailing comments.
  /// - Fields are separated by any run of ',', ' ' or '\t'. Leading and trailing separators are ignored, so "1,,2" has
  ///   two fields.
  /// - The first content line is the mandatory header. Its tokens are `NAME` or `NAME[unit]`, with NAME one of
  ///   FZ, SA, SR, IA, VX (all required) and P (optional), case-insensitive, in any order. Each column may appear
  ///   once.
  /// - Units (compared ASCII case-insensitively; no unit means the SI unit): FZ[N], SA[rad|deg], SR[-], IA[rad|deg],
  ///   VX[m/s], P[Pa]. Degrees are converted as `value * (pi / 180)`.
  /// - Every data row has exactly as many fields as the header. Each field must be a finite number
  ///   (`mf::io::parse_number`).
  /// - Empty text, or text with only comments and blank lines, has no header (SYNTAX). A header without rows gives an
  ///   empty table.
  ///
  /// Errors carry `file_name`, the 1-based line (0 when there is no header line) and the upper-case column name:
  /// SYNTAX (no header, unknown column, malformed unit, field count), DUPLICATE_KEY (column twice), MISSING_KEY
  /// (required column absent), UNSUPPORTED_UNIT, INVALID_NUMBER.
  [[nodiscard]] std::expected<input_table, error> parse_input_table(std::string_view text, std::string file_name = {});

  /// Reads `path` (`read_text_file`) and parses it with `parse_input_table`, using `path` as the file name in errors.
  [[nodiscard]] std::expected<input_table, error> read_input_table(const std::filesystem::path &path);
} // namespace mf::io
