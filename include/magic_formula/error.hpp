#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

namespace mf {
  /// Category of a recoverable error. Carried by `mf::error` inside `std::expected<T, mf::error>`.
  enum class error_code : std::uint8_t {
    IO,                ///< a file cannot be opened or read
    SYNTAX,            ///< lexical or structural problem in a .tir file or an input table
    INVALID_NUMBER,    ///< text that is not a finite number
    DUPLICATE_KEY,     ///< a key or column that appears twice
    MISSING_KEY,       ///< a required key or column is absent
    INVALID_VALUE,     ///< a well-formed value that is not acceptable (range, type, relation)
    UNSUPPORTED_UNIT,  ///< a unit other than the supported SI units
    UNSUPPORTED_MODEL, ///< a model/format identification that routes to no implemented model
    NOT_IMPLEMENTED,   ///< a recognised but unimplemented feature (for example TYRESIDE mirroring)
    INVALID_INPUT,     ///< an operating point outside the model domain
    NUMERICAL,         ///< a non-finite result
  };

  /// Where a diagnostic applies. An empty file, line 0 and an empty key mean "not applicable".
  struct location {
    /// File name as given by the caller.
    std::string file;
    /// 1-based line; 0 = not applicable.
    std::size_t line = 0;
    /// Upper-case .tir key, table column or input field (FZ, SA, SR, IA, VX; outputs FX..MY).
    std::string key;
  };

  /// A recoverable error: category, human-readable message and source location.
  struct error {
    error_code code = error_code::INVALID_VALUE;
    /// What went wrong, naming the key/column and the offending value.
    std::string message;
    location where;
  };

  /// A non-fatal diagnostic. The library returns warnings as data and never prints them.
  struct warning {
    /// What the caller should know.
    std::string message;
    location where;
  };

  /// Builds the `std::unexpected` for `return make_error(...);` in a function returning `std::expected<T, error>`.
  [[nodiscard]] std::unexpected<error> make_error(error_code code, std::string message, location where = {});

  /// Formats a location for humans: "file:line" (file and line), "file" (file only), "line N" (line only) or ""
  /// (neither). The key is never printed.
  [[nodiscard]] std::string to_string(const location &where);

  /// Formats an error as "<location>: <message>", or just "<message>" when the location string is empty.
  [[nodiscard]] std::string to_string(const error &value);

  /// Formats a warning as "<location>: <message>", or just "<message>" when the location string is empty.
  [[nodiscard]] std::string to_string(const warning &value);
} // namespace mf
