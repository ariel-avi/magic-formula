#pragma once

#include <cstdint>
#include <expected>
#include <magic_formula/evaluation_types.hpp>
#include <ostream>
#include <span>
#include <string>
#include <string_view>

namespace mf::cli {
  /// Process exit status: success, `--help` and `--version` (AD-27).
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
    /// `--uncombined` selects pure-slip outputs, otherwise combined slip (AD-16).
    slip_mode mode = slip_mode::COMBINED;
    /// `--verbose` prints the model information and the ignored .tir keys on stderr.
    bool verbose = false;
  };

  /// Parses the command line (without the program name). The arguments are scanned left to right:
  /// `--uncombined` and `--verbose` set their option, `-h` / `--help` return `command::HELP` and `--version`
  /// returns `command::VERSION` immediately (whatever else is on the line), any other argument that starts with '-'
  /// (including a lone "-") is an error, and everything else is a positional argument. A `command::RUN` needs exactly
  /// two positional arguments: the input table and the .tir file.
  /// @param args command-line arguments, `argv[0]` excluded
  /// @return the options, or the usage-error message ("unknown option '<arg>'" or
  ///         "expected 2 arguments <inputs-table> <file.tir>, got <n>")
  [[nodiscard]] std::expected<options, std::string> parse_command_line(std::span<const std::string_view> args);

  /// The usage text printed by `--help` and after a usage error: synopsis, options, the input-table format (columns,
  /// units, separators, comments), the output header, and the exit statuses. Ends with a newline.
  [[nodiscard]] std::string_view usage() noexcept;

  /// The application: parses the command line, loads the .tir file, reads the input table, evaluates every row and
  /// writes the results as CSV (`io::write_results`) to `out`. Diagnostics go to `err` (AD-27): load warnings,
  /// optional `--verbose` information, the pressure-column warning (AD-09), and one clamp / VXLOW warning per variable
  /// naming the first table line and the number of rows. Nothing is written to `out` for a usage, file, model,
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
