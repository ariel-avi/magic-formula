#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <magic_formula/detail/text.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/input_table.hpp>
#include <magic_formula/io/number.hpp>
#include <magic_formula/io/text_file.hpp>
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
    /// The values of one data row, one member per column, already in SI units.
    struct row_values {
      double fz = 0.0;
      double sa = 0.0;
      double sr = 0.0;
      double ia = 0.0;
      double vx = 0.0;
      double p = 0.0;
    };

    /// A known column: its upper-case name and the `row_values` member that receives its value.
    struct column_def {
      std::string_view name;
      double row_values::*target;
    };

    /// All columns. The first `REQUIRED_COLUMNS` are required, the rest are optional.
    constexpr std::array<column_def, 6> COLUMNS{
        column_def{.name = "FZ", .target = &row_values::fz}, column_def{.name = "SA", .target = &row_values::sa},
        column_def{.name = "SR", .target = &row_values::sr}, column_def{.name = "IA", .target = &row_values::ia},
        column_def{.name = "VX", .target = &row_values::vx}, column_def{.name = "P", .target = &row_values::p},
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

    /// The only unit table. The first entry of a column is its SI unit, which applies when no unit is given.
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

    /// The parsed header line.
    struct header {
      std::vector<column_spec> columns;
      bool has_pressure = false;
    };

    /// Parse state: the file name for diagnostics, the header (once seen) and the table being built.
    struct parse_state {
      std::string file;
      std::optional<header> head;
      input_table table;
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

    /// "N, ..." list of the units accepted for `column`, for diagnostics.
    std::string supported_units(std::string_view column) {
      const auto is_column = [column](const unit_entry &entry) { return entry.column == column; };
      const auto unit_name = [](const unit_entry &entry) { return std::string(entry.unit); };
      // Views are not const-iterable in general, so they stay non-const locals.
      auto matching = UNITS | std::views::filter(is_column);
      auto unit_names = matching | std::views::transform(unit_name);
      const std::vector<std::string> names = std::ranges::to<std::vector>(unit_names);
      return detail::join(names, ", ");
    }

    /// One header token, `NAME` or `NAME[unit]`: the column and its unit factor.
    std::expected<column_spec, error> parse_header_token(std::string_view token, const std::string &file,
                                                         std::size_t line) {
      const std::size_t open = token.find('[');
      const std::string_view name = token.substr(0, open);
      const auto is_named = [name](const column_def &def) { return detail::iequals_ascii(def.name, name); };
      // Searched through a span: its iterator is a class type on every standard library (no raw pointer).
      const std::span<const column_def> known_columns(COLUMNS);
      const auto column = std::ranges::find_if(known_columns, is_named);
      if (column == known_columns.end()) {
        std::string message = std::format("unknown column {} (expected FZ, SA, SR, IA, VX, P)", token);
        return table_error(error_code::SYNTAX, std::move(message), file, line, detail::to_upper_ascii(name));
      }
      std::string_view unit;
      if (open != std::string_view::npos) {
        const std::size_t close = token.find(']');
        if (close != token.size() - 1 || close <= open + 1) {
          return table_error(error_code::SYNTAX,
                             std::format("malformed unit in column header {} (expected NAME or NAME[unit])", token),
                             file, line, column->name);
        }
        unit = token.substr(open + 1, close - open - 1);
      }
      const std::string_view column_name = column->name;
      const auto accepts = [column_name, unit](const unit_entry &candidate) {
        return candidate.column == column_name && (unit.empty() || detail::iequals_ascii(candidate.unit, unit));
      };
      const std::span<const unit_entry> known_units(UNITS);
      const auto entry = std::ranges::find_if(known_units, accepts);
      if (entry == known_units.end()) {
        return table_error(error_code::UNSUPPORTED_UNIT,
                           std::format("unsupported unit '{}' for column {} (supported: {})", unit, column->name,
                                       supported_units(column->name)),
                           file, line, column->name);
      }
      return column_spec{.def = &*column, .factor = entry->factor};
    }

    /// True when `columns` already contains `def`.
    bool has_column(const std::vector<column_spec> &columns, const column_def *def) {
      return std::ranges::any_of(columns, [def](const column_spec &spec) { return spec.def == def; });
    }

    /// The header line: every token is a known column with a supported unit, no column twice, all required present.
    std::expected<header, error> parse_header(std::span<const std::string_view> tokens, const std::string &file,
                                              std::size_t line) {
      header result;
      for (const std::string_view token : tokens) {
        const auto spec = parse_header_token(token, file, line);
        if (!spec.has_value()) {
          return std::unexpected<error>(spec.error());
        }
        if (has_column(result.columns, spec->def)) {
          return table_error(error_code::DUPLICATE_KEY, std::format("duplicate column {}", spec->def->name), file, line,
                             spec->def->name);
        }
        result.columns.push_back(*spec);
      }
      const std::span<const column_def> required = std::span<const column_def>(COLUMNS).first(REQUIRED_COLUMNS);
      const auto is_absent = [&result](const column_def &def) { return !has_column(result.columns, &def); };
      const auto missing = std::ranges::find_if(required, is_absent);
      if (missing != required.end()) {
        std::string message = std::format("missing required column {} ", missing->name);
        message += "(expected FZ, SA, SR, IA, VX; optional P)";
        return table_error(error_code::MISSING_KEY, std::move(message), file, line, missing->name);
      }
      result.has_pressure = has_column(result.columns, &COLUMNS.back());
      return result;
    }

    /// One data row: exactly as many numbers as the header has columns, converted to SI.
    std::expected<input_row, error> parse_row(const header &head, std::span<const std::string_view> fields,
                                              const std::string &file, std::size_t line) {
      if (fields.size() != head.columns.size()) {
        return table_error(error_code::SYNTAX,
                           std::format("expected {} fields, got {}", head.columns.size(), fields.size()), file, line,
                           {});
      }
      row_values values;
      for (const auto [spec, field] : std::views::zip(head.columns, fields)) {
        const std::optional<double> parsed = parse_number(field);
        if (!parsed.has_value()) {
          return table_error(error_code::INVALID_NUMBER,
                             std::format("invalid number '{}' in column {}", field, spec.def->name), file, line,
                             spec.def->name);
        }
        values.*(spec.def->target) = *parsed * spec.factor;
      }
      return input_row{.point = operating_point{.fz_n = values.fz,
                                                .kappa = values.sr,
                                                .alpha_rad = values.sa,
                                                .gamma_rad = values.ia,
                                                .vx_mps = values.vx},
                       .pressure_pa = head.has_pressure ? std::optional<double>(values.p) : std::nullopt,
                       .line = line};
    }

    /// One content line (not blank, not a comment): the header when none was seen yet, otherwise a data row.
    std::expected<void, error> handle_content_line(parse_state &state, std::string_view content, std::size_t line) {
      std::vector<std::string_view> fields;
      split_fields(content, fields);
      if (!state.head.has_value()) {
        auto parsed = parse_header(fields, state.file, line);
        if (!parsed.has_value()) {
          return std::unexpected<error>(std::move(parsed.error()));
        }
        state.table.has_pressure = parsed->has_pressure;
        state.head = std::move(*parsed);
        return {};
      }
      const auto row = parse_row(*state.head, fields, state.file, line);
      if (!row.has_value()) {
        return std::unexpected<error>(row.error());
      }
      state.table.rows.push_back(*row);
      return {};
    }

    /// Walks the text line by line (a final line without '\n' is a normal line), skipping blank and comment lines.
    std::expected<void, error> parse_lines(parse_state &state, std::string_view text) {
      std::size_t line_number = 0;
      while (!text.empty()) {
        ++line_number;
        const std::size_t newline = text.find('\n');
        const std::string_view content = detail::trim(text.substr(0, newline));
        // `text` is not empty here, so size() - 1 is valid; npos (no '\n' left) consumes the rest of the text.
        text.remove_prefix(std::min(newline, text.size() - 1) + 1);
        if (content.empty() || content.starts_with('#')) {
          continue;
        }
        auto handled = handle_content_line(state, content, line_number);
        if (!handled.has_value()) {
          return handled;
        }
      }
      return {};
    }
  } // namespace

  std::expected<input_table, error> parse_input_table(std::string_view text, std::string file_name) {
    parse_state state{.file = std::move(file_name), .head = std::nullopt, .table = {}};
    if (text.starts_with(UTF8_BOM)) {
      text.remove_prefix(UTF8_BOM.size());
    }
    const auto parsed = parse_lines(state, text);
    if (!parsed.has_value()) {
      return std::unexpected<error>(parsed.error());
    }
    if (!state.head.has_value()) {
      return table_error(error_code::SYNTAX, "missing header line", state.file, 0, {});
    }
    return std::move(state.table);
  }

  std::expected<input_table, error> read_input_table(const std::filesystem::path &path) {
    const auto text = read_text_file(path);
    if (!text.has_value()) {
      return std::unexpected<error>(text.error());
    }
    return parse_input_table(*text, path.string());
  }
} // namespace mf::io
