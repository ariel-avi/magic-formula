#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <expected>
#include <format>
#include <magic_formula/cli/cli.hpp>
#include <magic_formula/detail/text.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/input_table.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/result_writer.hpp>
#include <magic_formula/tire_model.hpp>
#include <magic_formula/version.hpp>
#include <optional>
#include <ostream>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mf::cli {
  namespace {
    // clang-format off
    /// The usage text (AD-27). The results header line must equal `io::RESULTS_HEADER` (checked below).
    constexpr std::string_view USAGE_TEXT = R"usage(Usage: mf [options] <inputs-table> <file.tir>

Evaluates the PAC2002 or MF 5.2 tire model of <file.tir> at every row of <inputs-table> and prints
Fx, Fy, Mz, Mx and My for every row as CSV on stdout.

Options:
  --uncombined   print the pure-slip outputs (Fx0, Fy0, Mz0) instead of the combined-slip ones
  --verbose      print the model information and the ignored .tir keys on stderr
  -h, --help     print this text on stdout and exit
  --version      print the version on stdout and exit

Input table (plain text, one operating point per row):
  - A line whose first non-blank character is '#' is a comment; blank lines are ignored.
  - Fields are separated by any run of commas, blanks or tabs.
  - The first content line is the header. Required columns: FZ SA SR IA VX. Optional column: P (pressure; it is
    read but ignored with a warning, because PAC2002 and MF 5.2 have no pressure input).
    Columns may be in any order and in any letter case.
  - A column name may carry a unit in brackets; without a unit the SI unit applies:
      FZ[N]  SA[rad|deg]  SR[-]  IA[rad|deg]  VX[m/s]  P[Pa]
  - Every data row has exactly as many numbers as the header has columns.

Output (stdout):
  FZ[N],SA[rad],SR[-],IA[rad],VX[m/s],FX[N],FY[N],MZ[N*m],MX[N*m],MY[N*m]
  followed by one line per input row, in input order. The inputs are echoed unclamped in SI units
  (angles in rad), followed by the five outputs in the ISO W-axis sign convention. Nothing is written to
  stdout when an error occurs. Warnings (clamped inputs, low speed, ignored column) go to stderr.

Exit status:
  0  success (also --help and --version)
  1  error: file, model, input table or evaluation problem (message on stderr)
  2  usage error (message and this text on stderr)
)usage";
    // clang-format on
    static_assert(USAGE_TEXT.find(io::RESULTS_HEADER) != std::string_view::npos);

    /// The names of `tyre_side`, indexed by the enumerator value.
    constexpr std::array<std::string_view, 2> SIDE_NAMES{"LEFT", "RIGHT"};

    /// One `input_flags` member and the words of its warning (AD-05, AD-15).
    struct flag_check {
      /// The flag in `tire_outputs::flags`.
      bool input_flags::*flag;
      /// Input field named in the warning.
      std::string_view key;
      /// What happened to the field.
      std::string_view what;
      /// What the model did about it.
      std::string_view consequence;
    };

    // clang-format off
    /// The flag warnings, in the order they are printed.
    constexpr std::array<flag_check, 5> FLAG_CHECKS{
        flag_check{.flag = &input_flags::fz_clamped,
                   .key = "FZ",
                   .what = "outside the property-file range",
                   .consequence = "evaluated at the limit (AD-15)"},
        flag_check{.flag = &input_flags::kappa_clamped,
                   .key = "SR",
                   .what = "outside the property-file range",
                   .consequence = "evaluated at the limit (AD-15)"},
        flag_check{.flag = &input_flags::alpha_clamped,
                   .key = "SA",
                   .what = "outside the property-file range",
                   .consequence = "evaluated at the limit (AD-15)"},
        flag_check{.flag = &input_flags::gamma_clamped,
                   .key = "IA",
                   .what = "outside the property-file range",
                   .consequence = "evaluated at the limit (AD-15)"},
        flag_check{.flag = &input_flags::below_vxlow,
                   .key = "VX",
                   .what = "below VXLOW",
                   .consequence = "VXLOW is not applied (AD-05)"},
    };
    // clang-format on

    /// The mounting side as text ("LEFT" or "RIGHT").
    std::string_view side_name(tyre_side side) {
      const std::span<const std::string_view> names{SIDE_NAMES};
      return names[static_cast<std::size_t>(side)];
    }

    /// The value as text, or "absent".
    std::string describe(const std::optional<int> &value) {
      return value.transform([](int number) { return std::to_string(number); }).value_or("absent");
    }

    /// Writes "mf: error: <failure>" and returns `EXIT_ERROR`.
    int fail(std::ostream &err, const error &failure) {
      err << "mf: error: " << to_string(failure) << '\n';
      return EXIT_ERROR;
    }

    /// Writes "mf: warning: <item>".
    void print_warning(std::ostream &err, const warning &item) {
      err << "mf: warning: " << to_string(item) << '\n';
    }

    /// Builds a warning member by member (moves only) and writes it as "mf: warning: <item>". The item is consumed in
    /// this function and never returned: a returned local would add an exception-cleanup-only line that no test can
    /// execute.
    void print_warning(std::ostream &err, std::string message, std::string file, std::size_t line, std::string key) {
      warning item;
      item.message = std::move(message);
      item.where.file = std::move(file);
      item.where.line = line;
      item.where.key = std::move(key);
      print_warning(err, item);
    }

    /// `--verbose`: the model name, the [MODEL] metadata and the ignored keys, as "mf: info: ..." lines.
    void print_info(std::ostream &err, const io::loaded_model &loaded) {
      const model_metadata &metadata = loaded.model->metadata();
      err << std::format("mf: info: model {}\n", loaded.model->name());
      err << std::format("mf: info: FITTYP {}, USE_MODE {}, TYRESIDE {}\n", describe(metadata.fittyp),
                         describe(metadata.use_mode), side_name(metadata.side));
      err << std::format("mf: info: ignored keys ({}): {}\n", loaded.ignored_keys.size(),
                         detail::join(loaded.ignored_keys, ", "));
    }

    /// One warning per variable that the model clamped (or flagged), naming the first table line and the row count.
    void print_flag_warnings(std::ostream &err, const std::string &inputs_path, std::span<const io::input_row> rows,
                             std::span<const tire_outputs> outputs) {
      for (const flag_check &check : FLAG_CHECKS) {
        const auto is_set = [&check](const tire_outputs &output) { return output.flags.*(check.flag); };
        const std::ptrdiff_t count = std::ranges::count_if(outputs, is_set);
        if (count > 0) {
          const auto first = static_cast<std::size_t>(std::ranges::find_if(outputs, is_set) - outputs.begin());
          const std::string message =
              std::format("{} {} in {} row(s); {}", check.key, check.what, count, check.consequence);
          print_warning(err, message, inputs_path, rows[first].line, std::string(check.key));
        }
      }
    }

    /// Everything printed to stderr on success, in this order: load warnings, `--verbose` information, the pressure
    /// column warning (AD-09), and the flag warnings.
    void print_diagnostics(std::ostream &err, const options &parsed, const io::loaded_model &loaded,
                           const io::input_table &table, std::span<const tire_outputs> outputs) {
      for (const warning &item : loaded.warnings) {
        print_warning(err, item);
      }
      if (parsed.verbose) {
        print_info(err, loaded);
      }
      if (table.has_pressure) {
        print_warning(err, "column P is ignored: PAC2002 and MF 5.2 have no pressure input (AD-09)", parsed.inputs_path,
                      0, "P");
      }
      print_flag_warnings(err, parsed.inputs_path, table.rows, outputs);
    }

    /// The `command::RUN` action: load, read, evaluate, report, write.
    int execute(const options &parsed, std::ostream &out, std::ostream &err) {
      const std::expected<io::loaded_model, error> loaded = io::load_tire_model_file(parsed.tir_path);
      if (!loaded.has_value()) {
        return fail(err, loaded.error());
      }
      const std::expected<io::input_table, error> table = io::read_input_table(parsed.inputs_path);
      if (!table.has_value()) {
        return fail(err, table.error());
      }
      const std::vector<operating_point> points =
          table->rows | std::views::transform(&io::input_row::point) | std::ranges::to<std::vector<operating_point>>();
      std::expected<std::vector<tire_outputs>, batch_error> results =
          loaded->model->evaluate_batch(points, parsed.mode);
      if (!results.has_value()) {
        // The failure is located in the input table: file and 1-based line of the failing row (the key is kept).
        batch_error &failure = results.error();
        failure.cause.where.file = parsed.inputs_path;
        failure.cause.where.line = table->rows[failure.index].line;
        return fail(err, failure.cause);
      }
      print_diagnostics(err, parsed, *loaded, *table, *results);
      io::write_results(out, points, *results);
      out.flush();
      if (out.fail()) {
        err << "mf: error: cannot write results\n";
        return EXIT_ERROR;
      }
      return EXIT_OK;
    }
  } // namespace

  std::expected<options, std::string> parse_command_line(std::span<const std::string_view> args) {
    options parsed;
    std::vector<std::string_view> positionals;
    for (const std::string_view arg : args) {
      if (arg == "-h" || arg == "--help") {
        options help_request;
        help_request.action = command::HELP;
        return help_request;
      }
      if (arg == "--version") {
        options version_request;
        version_request.action = command::VERSION;
        return version_request;
      }
      if (arg == "--uncombined") {
        parsed.mode = slip_mode::UNCOMBINED;
        continue;
      }
      if (arg == "--verbose") {
        parsed.verbose = true;
        continue;
      }
      if (arg.starts_with('-')) {
        return std::unexpected(std::format("unknown option '{}'", arg));
      }
      positionals.push_back(arg);
    }
    if (positionals.size() != 2) {
      const std::size_t count = positionals.size();
      return std::unexpected(std::format("expected 2 arguments <inputs-table> <file.tir>, got {}", count));
    }
    parsed.inputs_path = positionals[0];
    parsed.tir_path = positionals[1];
    return parsed;
  }

  std::string_view usage() noexcept {
    return USAGE_TEXT;
  }

  int run(std::span<const std::string_view> argv, std::ostream &out, std::ostream &err) {
    try {
      const std::expected<options, std::string> parsed =
          parse_command_line(argv.subspan(std::min<std::size_t>(1U, argv.size())));
      if (!parsed.has_value()) {
        err << "mf: error: " << parsed.error() << '\n' << usage();
        return EXIT_USAGE;
      }
      if (parsed->action == command::HELP) {
        out << usage();
        return EXIT_OK;
      }
      if (parsed->action == command::VERSION) {
        out << "mf " << mf::version() << '\n';
        return EXIT_OK;
      }
      return execute(*parsed, out, err);
    } catch (const std::exception &failure) {
      err << "mf: error: " << failure.what() << '\n';
      return EXIT_ERROR;
    } catch (...) {
      err << "mf: error: unknown exception\n";
      return EXIT_ERROR;
    }
  }
} // namespace mf::cli
