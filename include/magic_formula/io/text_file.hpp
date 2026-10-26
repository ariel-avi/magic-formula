#pragma once

#include <expected>
#include <filesystem>
#include <istream>
#include <magic_formula/error.hpp>
#include <string>

namespace mf::io {
  /// Reads the whole file in binary mode (no newline translation, NUL bytes preserved).
  /// Returns an `error_code::IO` error whose location names `path` when `path` is a directory, or the file cannot be
  /// opened or read.
  [[nodiscard]] std::expected<std::string, error> read_text_file(const std::filesystem::path &path);

  /// Reads `input` to its end in chunks. Returns an `error_code::IO` error whose location names `name` when the stream
  /// ends in a read failure (badbit).
  [[nodiscard]] std::expected<std::string, error> read_text_stream(std::istream &input, const std::string &name);
} // namespace mf::io
