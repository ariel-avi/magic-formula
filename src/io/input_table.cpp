#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <magic_formula/detail/text.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/io/input_table.hpp>
#include <magic_formula/io/number.hpp>
#include <magic_formula/io/text_file.hpp>
#include <magic_formula/model_layout.hpp>
#include <numbers>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mf::io {
  namespace {
    /// A known column: its upper-case name.
    struct column_def {
      std::string_view name;
    };

    /// All columns. The first `REQUIRED_COLUMNS` are required, the rest are optional.
    constexpr std::array<column_def, 6> COLUMNS{
        column_def{.name = "FZ"}, column_def{.name = "SA"}, column_def{.name = "SR"},
        column_def{.name = "IA"}, column_def{.name = "VX"}, column_def{.name = "P"},
    };

    /// Number of leading `COLUMNS` entries that every table must have (FZ, SA, SR, IA, VX).
    constexpr std::size_t REQUIRED_COLUMNS = 5;

    /// Degrees to radians (`value * DEG_TO_RAD`).
    constexpr double DEG_TO_RAD = std::numbers::pi / 180.0;

    /// One accepted unit of a column and its factor to the SI unit.
    struct unit_entry {
      std::string_view column;
      std::string_view unit;
      double factor;
    };

    /// Every accepted (column, unit) pair and its factor to SI; the first entry of a column is its SI unit.
    constexpr std::array<unit_entry, 8> UNITS{
        unit_entry{.column = "FZ", .unit = "N", .factor = 1.0},
        unit_entry{.column = "SA", .unit = "rad", .factor = 1.0},
        unit_entry{.column = "SA", .unit = "deg", .factor = DEG_TO_RAD},
        unit_entry{.column = "SR", .unit = "-", .factor = 1.0},
        unit_entry{.column = "IA", .unit = "rad", .factor = 1.0},
        unit_entry{.column = "IA", .unit = "deg", .factor = DEG_TO_RAD},
        unit_entry{.column = "VX", .unit = "m/s", .factor = 1.0},
        unit_entry{.column = "P", .unit = "Pa", .factor = 1.0},
    };

    /// A header column: which column it is and the factor that converts its values to SI.
    struct column_spec {
      const column_def *def = nullptr;
      double factor = 1.0;
    };

    /// Characters that mark the legacy NAME[unit] header form.
    constexpr std::string_view LEGACY_UNIT_MARKS = "[]";

    /// The unread rest of the text and the 1-based number of the last line read (0 before the first line).
    struct line_cursor {
      std::string_view rest;
      std::size_t line = 0;
    };

    /// UTF-8 byte order mark, skipped at the start of the text (spreadsheet "CSV UTF-8" exports begin with one).
    constexpr std::string_view UTF8_BOM = "\xEF\xBB\xBF";

    /// Field separators: any run of these characters separates two fields.
    constexpr std::string_view SEPARATORS = ", \t";

    /// Splits a line at runs of separators. Leading and trailing separators produce no fields.
    /// `fields` is an out-parameter so that no named return value is destroyed only on an exception path.
    void split_fields(std::string_view line, std::vector<std::string_view> &fields) {
      std::size_t first = line.find_first_not_of(SEPARATORS);
      while (first != std::string_view::npos) {
        line.remove_prefix(first);
        const std::size_t length = std::min(line.find_first_of(SEPARATORS), line.size());
        fields.push_back(line.substr(0, length));
        line.remove_prefix(length);
        first = line.find_first_not_of(SEPARATORS);
      }
    }

    /// Error at `file`:`line` for `key` (upper-case column name, or empty).
    std::unexpected<error> table_error(error_code code, std::string message, const std::string &file, std::size_t line,
                                       std::string_view key) {
      // Built member by member: an aggregate initialiser with a throwing member leaves an untakeable cleanup branch.
      location where;
      where.file = file;
      where.line = line;
      where.key = key;
      return make_error(code, std::move(message), std::move(where));
    }

    /// Reads lines up to the next content line (not blank, not a '#' comment), clears `fields` and splits that line
    /// into it. Returns its 1-based line number, or std::nullopt when the text ends first. A final line without '\n' is
    /// a normal line. `fields` is an out-parameter.
    std::optional<std::size_t> next_content_line(line_cursor &cursor, std::vector<std::string_view> &fields) {
      while (!cursor.rest.empty()) {
        ++cursor.line;
        const std::size_t newline = cursor.rest.find('\n');
        const std::string_view content = detail::trim(cursor.rest.substr(0, newline));
        // `rest` is not empty here, so size() - 1 is valid; npos (no '\n' left) consumes the rest of the text.
        cursor.rest.remove_prefix(std::min(newline, cursor.rest.size() - 1) + 1);
        if (content.empty() || content.starts_with('#')) {
          continue;
        }
        fields.clear();
        split_fields(content, fields);
        return cursor.line;
      }
      return std::nullopt;
    }

    /// The units accepted for `column` in table order, joined with `separator`.
    std::string supported_units(std::string_view column, std::string_view separator) {
      const auto is_column = [column](const unit_entry &entry) { return entry.column == column; };
      const auto unit_name = [](const unit_entry &entry) { return std::string(entry.unit); };
      // Views are not const-iterable in general, so they stay non-const locals.
      auto matching = UNITS | std::views::filter(is_column);
      auto unit_names = matching | std::views::transform(unit_name);
      const std::vector<std::string> names = std::ranges::to<std::vector>(unit_names);
      return detail::join(names, separator);
    }

    /// "FZ: N, SA: rad|deg, SR: -, IA: rad|deg, VX: m/s" for `columns` in header order (P gives "P: Pa").
    std::string unit_hint(std::span<const column_def *const> columns) {
      const auto describe = [](const column_def *def) {
        const std::string units = supported_units(def->name, "|");
        return std::format("{}: {}", def->name, units);
      };
      auto described = columns | std::views::transform(describe);
      const std::vector<std::string> parts = std::ranges::to<std::vector>(described);
      return detail::join(parts, ", ");
    }

    /// True when `columns` already holds `def`.
    bool has_column(std::span<const column_def *const> columns, const column_def *def) {
      return std::ranges::find(columns, def) != columns.end();
    }

    /// Result types of the header and unit-line steps.
    using token_result = std::expected<const column_def *, error>;
    using header_result = std::expected<std::vector<const column_def *>, error>;
    using spec_result = std::expected<column_spec, error>;
    using unit_line_result = std::expected<std::vector<column_spec>, error>;

    /// One header field: a known column name without a unit. A '[' or ']' is the legacy NAME[unit] form (key = the
    /// upper-case text before the first mark); an unknown name gives SYNTAX with the upper-case token as key.
    token_result parse_header_token(std::string_view token, const std::string &file, std::size_t line) {
      const std::size_t mark = token.find_first_of(LEGACY_UNIT_MARKS);
      if (mark != std::string_view::npos) {
        std::string message = std::format("column header '{}' uses the legacy NAME[unit] form; ", token);
        message += "write the column name alone and give its unit on the unit line (the line after the header)";
        const std::string key = detail::to_upper_ascii(token.substr(0, mark));
        return table_error(error_code::SYNTAX, std::move(message), file, line, key);
      }
      const auto is_named = [token](const column_def &def) { return detail::iequals_ascii(def.name, token); };
      // Searched through a span: its iterator is a class type on every standard library (no raw pointer).
      const std::span<const column_def> known_columns(COLUMNS);
      const auto column = std::ranges::find_if(known_columns, is_named);
      if (column == known_columns.end()) {
        std::string message = std::format("unknown column {} (expected FZ, SA, SR, IA, VX, P)", token);
        return table_error(error_code::SYNTAX, std::move(message), file, line, detail::to_upper_ascii(token));
      }
      return &*column;
    }

    /// The header line: every field a known name, no column twice, all required columns present.
    header_result parse_header(std::span<const std::string_view> tokens, const std::string &file, std::size_t line) {
      std::vector<const column_def *> result;
      for (const std::string_view token : tokens) {
        const auto column = parse_header_token(token, file, line);
        if (!column.has_value()) {
          return std::unexpected<error>(column.error());
        }
        if (has_column(result, *column)) {
          std::string message = std::format("duplicate column {}", (*column)->name);
          return table_error(error_code::DUPLICATE_KEY, std::move(message), file, line, (*column)->name);
        }
        result.push_back(*column);
      }
      const std::span<const column_def> required = std::span<const column_def>(COLUMNS).first(REQUIRED_COLUMNS);
      const auto is_absent = [&result](const column_def &def) { return !has_column(result, &def); };
      const auto missing = std::ranges::find_if(required, is_absent);
      if (missing != required.end()) {
        std::string message = std::format("missing required column {} ", missing->name);
        message += "(expected FZ, SA, SR, IA, VX; optional P)";
        return table_error(error_code::MISSING_KEY, std::move(message), file, line, missing->name);
      }
      return result;
    }

    /// True when the first field parses as a number; false for no fields.
    bool starts_with_number(std::span<const std::string_view> fields) {
      return !fields.empty() && parse_number(fields.front()).has_value();
    }

    /// One unit-line entry: the column and the factor of the `unit` that it accepts, or UNSUPPORTED_UNIT.
    spec_result parse_unit(const column_def &def, std::string_view unit, const std::string &file, std::size_t line) {
      const auto accepts = [&def, unit](const unit_entry &candidate) {
        return candidate.column == def.name && detail::iequals_ascii(candidate.unit, unit);
      };
      const std::span<const unit_entry> known_units(UNITS);
      const auto entry = std::ranges::find_if(known_units, accepts);
      if (entry == known_units.end()) {
        std::string message = std::format("unsupported unit '{}' for column {}", unit, def.name);
        message += std::format(" (supported: {})", supported_units(def.name, ", "));
        return table_error(error_code::UNSUPPORTED_UNIT, std::move(message), file, line, def.name);
      }
      return column_spec{.def = &def, .factor = entry->factor};
    }

    /// The unit line (at `line`, after the header at `header_line`): one unit per header column. Checks in order: a
    /// number first (the unit line is missing), the unit count, then every unit left to right.
    unit_line_result parse_unit_line(std::span<const column_def *const> columns,
                                     std::span<const std::string_view> fields, const std::string &file,
                                     std::size_t header_line, std::size_t line) {
      if (starts_with_number(fields)) {
        std::string message = std::format("missing unit line: the line after the header (line {}) ", header_line);
        message += std::format("starts with the number '{}'; ", fields.front());
        message += std::format("it must give one unit per column ({})", unit_hint(columns));
        return table_error(error_code::SYNTAX, std::move(message), file, line, {});
      }
      if (fields.size() != columns.size()) {
        std::string message = std::format("unit line: expected {} units, one per header column, ", columns.size());
        message += std::format("got {} ({})", fields.size(), unit_hint(columns));
        return table_error(error_code::SYNTAX, std::move(message), file, line, {});
      }
      std::vector<column_spec> specs;
      for (const auto [def, field] : std::views::zip(columns, fields)) {
        const auto spec = parse_unit(*def, field, file, line);
        if (!spec.has_value()) {
          return std::unexpected<error>(spec.error());
        }
        specs.push_back(*spec);
      }
      return specs;
    }

    /// One data row: exactly as many numbers as the header has columns, converted to SI and appended to `values` in
    /// header order. `values` is an out-parameter so that no named return value is destroyed only on an exception
    /// path; on an error it may hold a partial row, which the caller discards.
    std::expected<void, error> parse_row(std::span<const column_spec> columns, std::span<const std::string_view> fields,
                                         const std::string &file, std::size_t line, std::vector<double> &values) {
      if (fields.size() != columns.size()) {
        std::string message = std::format("expected {} fields, got {}", columns.size(), fields.size());
        return table_error(error_code::SYNTAX, std::move(message), file, line, {});
      }
      for (const auto [spec, field] : std::views::zip(columns, fields)) {
        const std::optional<double> parsed = parse_number(field);
        if (!parsed.has_value()) {
          std::string message = std::format("invalid number '{}' in column {}", field, spec.def->name);
          return table_error(error_code::INVALID_NUMBER, std::move(message), file, line, spec.def->name);
        }
        values.push_back(*parsed * spec.factor);
      }
      return {};
    }

    /// All remaining content lines as data rows: appends the SI values row-major to `values` and the line numbers to
    /// `lines`.
    std::expected<void, error> parse_rows(line_cursor &cursor, std::span<const column_spec> columns,
                                          const std::string &file, std::vector<std::string_view> &fields,
                                          std::vector<double> &values, std::vector<std::size_t> &lines) {
      while (const auto line = next_content_line(cursor, fields)) {
        const auto row = parse_row(columns, fields, file, *line, values);
        if (!row.has_value()) {
          return std::unexpected<error>(row.error());
        }
        lines.push_back(*line);
      }
      return {};
    }
  } // namespace

  std::expected<input_table, error> parse_input_table(std::string_view text, std::string file_name) {
    if (text.starts_with(UTF8_BOM)) {
      text.remove_prefix(UTF8_BOM.size());
    }
    line_cursor cursor{.rest = text};
    std::vector<std::string_view> fields;
    const auto header_line = next_content_line(cursor, fields);
    if (!header_line.has_value()) {
      return table_error(error_code::SYNTAX, "missing header line", file_name, 0, {});
    }
    const auto names = parse_header(fields, file_name, *header_line);
    if (!names.has_value()) {
      return std::unexpected<error>(names.error());
    }
    const auto unit_line = next_content_line(cursor, fields);
    if (!unit_line.has_value()) {
      std::string message = "missing unit line: the header must be followed by a line with one unit per column ";
      message += std::format("({})", unit_hint(*names));
      return table_error(error_code::SYNTAX, std::move(message), file_name, *header_line, {});
    }
    const auto specs = parse_unit_line(*names, fields, file_name, *header_line, *unit_line);
    if (!specs.has_value()) {
      return std::unexpected<error>(specs.error());
    }
    input_table table;
    table.header_line = *header_line;
    for (const column_def *def : *names) {
      table.columns.emplace_back(def->name);
    }
    std::vector<double> values;
    const auto rows_read = parse_rows(cursor, *specs, file_name, fields, values, table.lines);
    if (!rows_read.has_value()) {
      return std::unexpected<error>(rows_read.error());
    }
    using row_major_matrix = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
    const auto rows = static_cast<Eigen::Index>(table.lines.size());
    const auto cols = static_cast<Eigen::Index>(table.columns.size());
    table.values = Eigen::Map<const row_major_matrix>(values.data(), rows, cols);
    table.file = std::move(file_name);
    return table;
  }

  std::expected<input_table, error> read_input_table(const std::filesystem::path &path) {
    const auto text = read_text_file(path);
    if (!text.has_value()) {
      return std::unexpected<error>(text.error());
    }
    return parse_input_table(*text, path.string());
  }

  std::expected<model_inputs, error> map_inputs(const input_table &table, std::span<const input_channel> channels) {
    model_inputs result;
    result.values.resize(table.values.rows(), static_cast<Eigen::Index>(channels.size()));
    for (std::size_t k = 0; k < channels.size(); ++k) {
      const std::string_view name = channels[k].name;
      const auto is_named = [name](const std::string &column) { return column == name; };
      const auto column = std::ranges::find_if(table.columns, is_named);
      if (column == table.columns.end()) {
        std::string message = std::format("missing required column {}", name);
        return table_error(error_code::MISSING_KEY, std::move(message), table.file, table.header_line, name);
      }
      const Eigen::Index source = column - table.columns.begin();
      result.values.col(static_cast<Eigen::Index>(k)) = table.values.col(source);
    }
    for (const std::string &column : table.columns) {
      const auto is_used = [&column](const input_channel &channel) { return channel.name == column; };
      if (std::ranges::none_of(channels, is_used)) {
        result.ignored_columns.push_back(column);
      }
    }
    result.lines = table.lines;
    return result;
  }

  std::expected<model_inputs, error> read_model_inputs(const std::filesystem::path &path,
                                                       std::span<const input_channel> channels) {
    const std::expected<input_table, error> table = read_input_table(path);
    if (!table.has_value()) {
      return std::unexpected<error>(table.error());
    }
    return map_inputs(*table, channels);
  }
} // namespace mf::io
