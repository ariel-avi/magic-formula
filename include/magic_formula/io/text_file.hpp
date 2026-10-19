#pragma once

#include <expected>
#include <filesystem>
#include <magic_formula/error.hpp>
#include <string>

namespace mf::io {
  /// Reads the whole file in binary mode (no newline translation, NUL bytes preserved).
  /// Returns an `error_code::IO` error whose location names `path` when `path` is a directory, or the file cannot be
  /// opened or read.
  [[nodiscard]] std::expected<std::string, error> read_text_file(const std::filesystem::path &path);
} // namespace mf::io
