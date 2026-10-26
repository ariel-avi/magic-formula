#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <expected>
#include <format>
#include <magic_formula/cli/cli.hpp>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/detail/text.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/input_table.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/result_writer.hpp>
#include <magic_formula/model_checks.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/tire_model.hpp>
#include <magic_formula/version.hpp>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mf::cli {
  namespace {
    // clang-format off
    /// The usage text. The results header line equals `io::results_header` of the MF 5.x models (tested).
    constexpr std::string_view USAGE_TEXT = R"usage(Usage: mf [options] <inputs-table> <file.tir>

Evaluates the PAC2002 or MF 5.2 tire model of <file.tir> at every row of <inputs-table> and prints
Fx, Fy, Mz, Mx and My, and the clamp and low-speed flags, for every row as CSV on stdout.

Options:
  --uncombined   print the pure-slip outputs (Fx0, Fy0, Mz0) instead of the combined-slip ones
  --verbose      print on stderr the model information, the ignored .tir keys, the coordinate-system
                 identification summary, every indicator reading, the conversion line and the
                 table-system line
  --system=iso|sae
                 convert the model to this coordinate system before evaluation (coefficient sign
                 flips, applied only when its identified system differs); default: the model as identified
  --table-system=iso|sae
                 coordinate system of the input table and of the printed rows; default: the side the
                 evaluated model is loaded on (FZ > 0: iso, FZ < 0: sae), so no data conversion
  -h, --help     print this text on stdout and exit
  --version      print the version on stdout and exit

Coordinate systems:
  ISO: x forward, y left, z up; a loaded tire has FZ > 0 and dFy/dSA > 0.
  SAE: x forward, y right, z down; a loaded tire has FZ < 0 and dFy/dSA < 0.
  SA, SR and IA have the same definition in both; FZ, FY, MZ and MY change sign.
  The model's system is identified from its evaluated behaviour and reported as a warning when the
  coefficients mix conventions or the evidence is weak (details with --verbose).
  Converting a model flips coefficient signs; the equations never change.

Input table (plain text, one operating point per row):
  - A line whose first non-blank character is '#' is a comment; blank lines are ignored.
  - Fields are separated by any run of commas, blanks or tabs.
  - The first content line is the header. Required columns: FZ SA SR IA VX. Optional column: P (pressure; it is
    read but ignored with a warning, because PAC2002 and MF 5.2 have no pressure input).
    Columns may be in any order and in any letter case.
  - The next non-comment line after the header is the mandatory unit line: one unit per column, in header order
    (column names only on the header line).
    Accepted units:
      FZ: N  SA: rad|deg  SR: -  IA: rad|deg  VX: m/s  P: Pa
    A unit in brackets after a name (FZ[N]) is an error. Example header and unit line:
      FZ,SA,SR,IA,VX
      N,deg,-,deg,m/s
  - Every data row has exactly as many numbers as the header has columns.

Output (stdout):
  FZ[N],SA[rad],SR[-],IA[rad],VX[m/s],FX[N],FY[N],MZ[N*m],MX[N*m],MY[N*m],FZ_CLAMPED[-],SR_CLAMPED[-],SA_CLAMPED[-],IA_CLAMPED[-],BELOW_VXLOW[-]
  followed by one line per input row, in input order. The inputs are echoed unclamped in SI units
  (angles in rad), followed by the five outputs, then five flags (1 = FZ/SR/SA/IA was clamped to the
  property-file range or VX is below VXLOW, 0 otherwise). The inputs and outputs are printed in the table system
  (--table-system). Nothing is written to stdout when an error occurs.
  Warnings (load side, coordinate system, clamped inputs, low speed, ignored column) go to stderr.

Exit status:
  0  success (also --help and --version)
  1  error: file, model, input table or evaluation problem (message on stderr)
  2  usage error (message and this text on stderr)
)usage";
    // clang-format on

    /// The names of `tyre_side`, indexed by the enumerator value.
    constexpr std::array<std::string_view, 2> SIDE_NAMES{"LEFT", "RIGHT"};

    /// Prefix of the `--system` option.
    constexpr std::string_view SYSTEM_PREFIX = "--system=";

    /// Prefix of the `--table-system` option.
    constexpr std::string_view TABLE_SYSTEM_PREFIX = "--table-system=";

    /// "no" and "yes", indexed by a bool.
    constexpr std::array<std::string_view, 2> YES_NO{"no", "yes"};

    /// The FZ relation of a loaded tire, indexed by `coordinate_system` (ISO, SAE).
    constexpr std::array<std::string_view, 2> LOADED_FZ_WORDS{"> 0", "< 0"};

    /// The vote as text, indexed by vote + 1.
    constexpr std::array<std::string_view, 3> VOTE_WORDS{"-1", "0", "+1"};

    /// The value of a `--system=` or `--table-system=` argument, or the usage error.
    std::expected<coordinate_system, std::string> system_value(std::string_view arg, std::string_view prefix) {
      const std::string_view value = arg.substr(prefix.size());
      const std::optional<coordinate_system> system = parse_coordinate_system(value);
      if (!system.has_value()) {
        const std::string_view option = prefix.substr(0, prefix.size() - 1);
        return std::unexpected(std::format("invalid value '{}' for {} (expected iso or sae)", value, option));
      }
      return *system;
    }

    /// Applies `--system=<v>` or `--table-system=<v>`; false when `arg` is neither.
    std::expected<bool, std::string> apply_system_option(std::string_view arg, options &parsed) {
      if (arg.starts_with(SYSTEM_PREFIX)) {
        const std::expected<coordinate_system, std::string> value = system_value(arg, SYSTEM_PREFIX);
        if (!value.has_value()) {
          return std::unexpected(value.error());
        }
        parsed.target_system = *value;
        return true;
      }
      if (arg.starts_with(TABLE_SYSTEM_PREFIX)) {
        const std::expected<coordinate_system, std::string> value = system_value(arg, TABLE_SYSTEM_PREFIX);
        if (!value.has_value()) {
          return std::unexpected(value.error());
        }
        parsed.table_system = *value;
        return true;
      }
      return false;
    }

    /// The words of the warning of one flag output channel.
    struct flag_wording {
      /// Name of the flag output channel.
      std::string_view channel;
      /// Input field named in the warning.
      std::string_view key;
      /// What happened to the field.
      std::string_view what;
      /// What the model did about it.
      std::string_view consequence;
    };

    // clang-format off
    /// The flag warnings, keyed by output channel name; printed in the order of the model's output channels.
    constexpr std::array<flag_wording, 5> FLAG_WORDINGS{
        flag_wording{.channel = "FZ_CLAMPED",
                     .key = "FZ",
                     .what = "outside the property-file range",
                     .consequence = "evaluated at the limit"},
        flag_wording{.channel = "SR_CLAMPED",
                     .key = "SR",
                     .what = "outside the property-file range",
                     .consequence = "evaluated at the limit"},
        flag_wording{.channel = "SA_CLAMPED",
                     .key = "SA",
                     .what = "outside the property-file range",
                     .consequence = "evaluated at the limit"},
        flag_wording{.channel = "IA_CLAMPED",
                     .key = "IA",
                     .what = "outside the property-file range",
                     .consequence = "evaluated at the limit"},
        flag_wording{.channel = "BELOW_VXLOW",
                     .key = "VX",
                     .what = "below VXLOW",
                     .consequence = "VXLOW is not applied"},
    };
    // clang-format on

    /// The mounting side as text ("LEFT" or "RIGHT").
    std::string_view side_name(tyre_side side) {
      const std::span<const std::string_view> names{SIDE_NAMES};
      return names[static_cast<std::size_t>(side)];
    }

    /// The value as text, or "absent".
    std::string describe(const std::optional<int> &value) {
      if (!value.has_value()) {
        return "absent";
      }
      return std::to_string(*value);
    }

    /// Writes "mf: error: <failure>" and returns `EXIT_ERROR`.
    int fail(std::ostream &err, const error &failure) {
      // A named local keeps its destructor on the normal path of the closing brace, so that line is executed.
      const std::string text = to_string(failure);
      err << "mf: error: " << text << '\n';
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

    /// One warning per flag output channel that is set on some row, naming the first such table line and the row count.
    /// The flags are found by their channel names in `FLAG_WORDINGS`; other output channels are skipped.
    void print_flag_warnings(std::ostream &err, const std::string &inputs_path, std::span<const std::size_t> lines,
                             const model_layout &layout, const Eigen::Ref<const Eigen::MatrixXd> &outputs) {
      for (std::size_t j = 0; j < layout.outputs.size(); ++j) {
        const std::string_view channel = layout.outputs[j].name;
        const auto is_channel = [channel](const flag_wording &wording) { return wording.channel == channel; };
        // NOLINTNEXTLINE(readability-qualified-auto): the array iterator is a class type on some standard libraries
        const auto wording = std::ranges::find_if(FLAG_WORDINGS, is_channel);
        if (wording == FLAG_WORDINGS.end()) {
          continue;
        }
        const auto column = static_cast<Eigen::Index>(j);
        const Eigen::Index count = (outputs.col(column).array() != 0.0).count();
        if (count > 0) {
          // maxCoeff reports the first maximum, i.e. the first row where the flag is set.
          Eigen::Index first = 0;
          (outputs.col(column).array() != 0.0).cast<int>().maxCoeff(&first);
          const std::string message =
              std::format("{} {} in {} row(s); {}", wording->key, wording->what, count, wording->consequence);
          print_warning(err, message, inputs_path, lines[static_cast<std::size_t>(first)], std::string(wording->key));
        }
      }
    }

    /// `--verbose`: one indicator reading as an "mf: info: coordinate indicator ..." line.
    void print_reading(std::ostream &err, const indicator_reading &reading) {
      const std::span<const std::string_view> votes{VOTE_WORDS};
      const int vote_index = reading.vote + 1;
      err << std::format("mf: info: coordinate indicator {} at {:g} x F'z0 ({:.6g} N): {:.6g} {}, normalised {:.3g}, "
                         "{}, vote {}, weight {:.3g}\n",
                         reading.definition.name, reading.load_scale, reading.load_n, reading.value,
                         reading.definition.unit, reading.normalised, reading_status_name(reading.status),
                         votes[static_cast<std::size_t>(vote_index)], reading.weight);
    }

    /// `--verbose`: the decision of one axis as "<SIGN> (<BASIS>, confidence <c>)".
    std::string describe_decision(const axis_decision &decision) {
      return std::format("{} ({}, confidence {:.2f})", axis_sign_name(decision.sign),
                         decision_basis_name(decision.basis), decision.confidence);
    }

    /// `--verbose`: the identification of the source model and every reading behind it.
    void print_identification(std::ostream &err, const coordinate_identification &identification) {
      const std::string vertical = describe_decision(identification.vertical);
      const std::string lateral = describe_decision(identification.lateral);
      err << std::format("mf: info: coordinate system of the model: {}; vertical {}; lateral {}\n",
                         system_classification_name(identification.classification), vertical, lateral);
      for (const indicator_reading &reading : identification.readings) {
        print_reading(err, reading);
      }
    }

    /// `--verbose`: the conversion line (only when the model was converted) and the table line.
    void print_conversion_info(std::ostream &err, const options &parsed,
                               const std::optional<model_conversion> &conversion, coordinate_system table,
                               coordinate_system side) {
      const std::span<const std::string_view> yes_no{YES_NO};
      if (conversion.has_value()) {
        err << std::format("mf: info: model converted to {} (vertical transform: {}, lateral transform: {})\n",
                           coordinate_system_name(parsed.target_system.value_or(side)),
                           yes_no[static_cast<std::size_t>(conversion->applied.vertical)],
                           yes_no[static_cast<std::size_t>(conversion->applied.lateral)]);
      }
      const std::span<const std::string_view> relations{LOADED_FZ_WORDS};
      err << std::format("mf: info: table in {}; the model is loaded at FZ {}\n", coordinate_system_name(table),
                         relations[static_cast<std::size_t>(side)]);
    }

    /// What `print_diagnostics` needs besides the streams, the options and the results.
    struct diagnostics_input {
      /// The loaded model (warnings and ignored keys); the converted model, when any, is `evaluated`.
      const io::loaded_model *loaded = nullptr;
      /// The model that was evaluated.
      const tire_model *evaluated = nullptr;
      /// Identification of the model as loaded.
      const coordinate_identification *source_id = nullptr;
      /// Identification of the evaluated model (its warnings are printed).
      const coordinate_identification *evaluated_id = nullptr;
      /// The conversion, when `--system` was given.
      const std::optional<model_conversion> *conversion = nullptr;
      /// The system of the table.
      coordinate_system table = coordinate_system::ISO;
      /// The loaded side of the evaluated model.
      coordinate_system side = coordinate_system::ISO;
    };

    /// Everything printed to stderr on success, in this order: load warnings, the identification warnings of the
    /// evaluated model, `--verbose` information, one warning per ignored table column, the load-side warning of the
    /// table (in the table system, naming the first offending table line), and the flag warnings.
    void print_diagnostics(std::ostream &err, const options &parsed, const diagnostics_input &state,
                           const io::model_inputs &inputs, const Eigen::Ref<const Eigen::MatrixXd> &outputs) {
      const tire_model &model = *state.evaluated;
      for (const warning &item : state.loaded->warnings) {
        print_warning(err, item);
      }
      for (const warning &item : state.evaluated_id->warnings) {
        print_warning(err, item);
      }
      if (parsed.verbose) {
        print_info(err, *state.loaded);
        print_identification(err, *state.source_id);
        print_conversion_info(err, parsed, *state.conversion, state.table, state.side);
      }
      for (const std::string &column : inputs.ignored_columns) {
        const std::string message =
            std::format("column {} is ignored: {} has no {} input", column, model.name(), column);
        print_warning(err, message, parsed.inputs_path, 0, column);
      }
      const std::optional<batch_warning> side =
          load_side_warning(census_fz(model.layout(), inputs.values), state.table);
      if (side.has_value()) {
        print_warning(err, side->cause.message, parsed.inputs_path, inputs.lines[side->index], side->cause.where.key);
      }
      print_flag_warnings(err, parsed.inputs_path, inputs.lines, model.layout(), outputs);
    }

    /// The `command::RUN` action: load, identify, convert, read, evaluate, report, write.
    int execute(const options &parsed, std::ostream &out, std::ostream &err) {
      const model_options evaluation{.mode = parsed.mode, .throw_if_model_failure = false};
      const std::expected<io::loaded_model, error> loaded = io::load_tire_model_file(parsed.tir_path, evaluation);
      if (!loaded.has_value()) {
        return fail(err, loaded.error());
      }
      const tire_model &model = *loaded->model;
      const coordinate_identification source_id = model.find_coordinate_system();
      std::optional<model_conversion> conversion;
      if (parsed.target_system.has_value()) {
        conversion.emplace(model.convert_coordinate_system(*parsed.target_system));
      }
      const tire_model &evaluated = conversion.has_value() ? *conversion->model : model;
      const coordinate_identification evaluated_id = evaluated.find_coordinate_system();
      const std::expected<io::model_inputs, error> inputs =
          io::read_model_inputs(parsed.inputs_path, evaluated.layout().inputs);
      if (!inputs.has_value()) {
        return fail(err, inputs.error());
      }
      const coordinate_system side = evaluated.loaded_side();
      const coordinate_system table = parsed.table_system.value_or(side);
      Eigen::MatrixXd values = inputs->values;
      convert_inputs(evaluated.layout(), table, side, values);
      std::expected<Eigen::MatrixXd, batch_error> outputs = checked_evaluate_batch(evaluated, values);
      if (!outputs.has_value()) {
        // The failure is located in the input table: file and 1-based line of the failing row (the key is kept).
        batch_error &failure = outputs.error();
        failure.cause.where.file = parsed.inputs_path;
        failure.cause.where.line = inputs->lines[failure.index];
        return fail(err, failure.cause);
      }
      convert_outputs(evaluated.layout(), side, table, *outputs);
      const diagnostics_input state{.loaded = &*loaded,
                                    .evaluated = &evaluated,
                                    .source_id = &source_id,
                                    .evaluated_id = &evaluated_id,
                                    .conversion = &conversion,
                                    .table = table,
                                    .side = side};
      print_diagnostics(err, parsed, state, *inputs, *outputs);
      io::write_results(out, evaluated.layout(), inputs->values, *outputs);
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
      const std::expected<bool, std::string> handled = apply_system_option(arg, parsed);
      if (!handled.has_value()) {
        return std::unexpected(handled.error());
      }
      if (*handled) {
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
