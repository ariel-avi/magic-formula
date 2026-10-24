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
    /// rows x columns, SI units, unclamped, exactly as read (after unit conversion).
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
  /// - Empty text, or text with only comments and blank lines, has no header (SYNTAX). A header without rows gives a
  ///   0 x columns matrix.
  ///
  /// Errors carry `file_name`, the 1-based line (0 when there is no header line) and the upper-case column name:
  /// SYNTAX (no header, unknown column, malformed unit, field count), DUPLICATE_KEY (column twice), MISSING_KEY
  /// (required column absent), UNSUPPORTED_UNIT, INVALID_NUMBER.
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
