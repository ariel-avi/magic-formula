#pragma once

#include <cstdint>
#include <expected>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>

namespace mf::cli {
  /// Process exit status: success, `--help` and `--version`.
  inline constexpr int EXIT_OK = 0;
  /// Process exit status: a file, model, input-table or evaluation error, a caught exception, or an output failure.
  /// The message is on stderr. Nothing has been written to stdout unless the failure happened while the results were
  /// being written (an output failure, or an exception thrown by the output stream), where partial output can remain.
  inline constexpr int EXIT_ERROR = 1;
  /// Process exit status: bad command line (message and usage text on stderr).
  inline constexpr int EXIT_USAGE = 2;

  /// What the command line asks for.
  enum class command : std::uint8_t {
    RUN,    ///< evaluate the input table with the tire model
    HELP,   ///< print the usage text on stdout
    VERSION ///< print "mf <version>" on stdout
  };

  /// A parsed command line.
  struct options {
    /// The requested action; the paths below are only set for `command::RUN`.
    command action = command::RUN;
    /// First positional argument: the input table.
    std::string inputs_path;
    /// Second positional argument: the .tir property file.
    std::string tir_path;
    /// `--uncombined` selects pure-slip outputs, otherwise combined slip.
    slip_mode mode = slip_mode::COMBINED;
    /// `--verbose` prints on stderr the model information, the ignored .tir keys, the coordinate-system
    /// identification summary, every indicator reading, the conversion line and the table-system line.
    bool verbose = false;
    /// Enable MF6.1 turn slip (`--turn-slip`).
    bool turn_slip = false;
    /// `--system=iso|sae`: the model is converted to this system before evaluation (coefficient sign flips, applied
    /// only when its identified system differs); empty (default): the model as identified, no conversion.
    std::optional<coordinate_system> target_system;
    /// `--table-system=iso|sae`: the system of the input table and of the printed rows; empty (default): the evaluated
    /// model's loaded side (`loaded_side()`), so no data conversion.
    std::optional<coordinate_system> table_system;
  };

  /// Parses the command line (without the program name). The arguments are scanned left to right:
  /// `--uncombined`, `--turn-slip` and `--verbose` set their option, `--system=<v>` and `--table-system=<v>` set theirs
  /// (<v> is iso or sae in any letter case; the last occurrence wins; the forms without '=' are unknown options), `-h`
  /// / `--help` return `command::HELP` and `--version` returns `command::VERSION` immediately (whatever else is on the
  /// line), any other argument that starts with '-' (including a lone "-") is an error, and everything else is a
  /// positional argument. A `command::RUN` needs exactly two positional arguments: the input table and the .tir file.
  /// @param args command-line arguments, `argv[0]` excluded
  /// @return the options, or the usage-error message ("unknown option '<arg>'",
  ///         "invalid value '<v>' for --system (expected iso or sae)",
  ///         "invalid value '<v>' for --table-system (expected iso or sae)" or
  ///         "expected 2 arguments <inputs-table> <file.tir>, got <n>")
  [[nodiscard]] std::expected<options, std::string> parse_command_line(std::span<const std::string_view> args);

  /// The usage text printed by `--help` and after a usage error: synopsis, options, the input-table format (columns,
  /// units, separators, comments), the output header, and the exit statuses. Ends with a newline.
  [[nodiscard]] std::string_view usage() noexcept;

  /// The application: parses the command line, loads the .tir file, identifies its coordinate system
  /// (`tire_model::find_coordinate_system`), converts the model with `--system` (`convert_coordinate_system`; the
  /// equations never change), reads the input table, converts the table values from the table system
  /// (`--table-system`, default the evaluated model's `loaded_side()`, so no conversion) to the model's side
  /// (`convert_inputs`), evaluates them with `checked_evaluate_batch` and `checked_evaluate_characteristics_batch`,
  /// converts the outputs and the characteristics back to the table system (`convert_outputs`,
  /// `convert_characteristics`) and writes the results as CSV (`io::write_results`) to `out`: columns follow the model
  /// layout. MF6.1 has seven inputs, thirteen outputs and eight characteristics. MF5.x has 23 columns: the five inputs
  /// (echoed as read, in the table system), the five outputs, the five 0/1 flag columns (FZ_CLAMPED, SR_CLAMPED,
  /// SA_CLAMPED, IA_CLAMPED, BELOW_VXLOW) and the eight characteristics (DFY_DSA, DFX_DSR, FY_OVER_SA, FX_OVER_SR, DY,
  /// DX, SA_AT_PEAK_FY, SR_AT_PEAK_FX). A NaN prints as "nan"; it appears only in SA_AT_PEAK_FY and SR_AT_PEAK_FX (no
  /// stationary point, or no contact). Diagnostics go to `err`, in this order: load warnings, the identification
  /// warnings of the evaluated model, optional `--verbose` information (model information, ignored keys, the
  /// identification of the model as loaded with every indicator reading, the conversion, the table system), one warning
  /// per table column that the model has no input for (for example P), the load-side warning
  /// (`load_side_warning` in the table system) naming the first offending table line, and one clamp / VXLOW warning per
  /// flag naming the first table line and the number of rows.
  /// Nothing is written to `out` for a usage, file, model,
  /// input-table or evaluation error, because the results are written only after every row has been evaluated. The
  /// results are then streamed to `out` row by row, so if `out` fails or throws while they are written, the rows
  /// already written remain and the call returns `EXIT_ERROR` ("mf: error: cannot write results" for a failed stream).
  ///
  /// This is the only place that catches exceptions and it never throws: any `std::exception` (for example
  /// `std::bad_alloc`, or `std::ios_base::failure` from a stream with an exception mask) becomes
  /// "mf: error: <what>" and `EXIT_ERROR`. A defensive `catch (...)` also converts any other thrown object (for example
  /// one thrown through a stream whose buffer throws a non-standard type) into "mf: error: unknown exception" and
  /// `EXIT_ERROR`.
  /// @param argv program name followed by the arguments; may be empty
  /// @param out results, `--help` and `--version` text
  /// @param err errors, warnings and information
  /// @return `EXIT_OK`, `EXIT_ERROR` or `EXIT_USAGE`
  [[nodiscard]] int run(std::span<const std::string_view> argv, std::ostream &out, std::ostream &err);
} // namespace mf::cli
