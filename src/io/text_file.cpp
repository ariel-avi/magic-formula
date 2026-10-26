#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <istream>
#include <magic_formula/error.hpp>
#include <magic_formula/io/text_file.hpp>
#include <string>
#include <system_error>

namespace mf::io {
  std::expected<std::string, error> read_text_file(const std::filesystem::path &path) {
    // POSIX fopen opens a directory, and libc++ then reads it as an empty file (fread 0 = EOF, no ferror check), so
    // reject directories before opening. The error_code overload never throws; a missing path is simply not a
    // directory and falls through to the open check below.
    std::error_code status_error;
    if (std::filesystem::is_directory(path, status_error)) {
      const location where{.file = path.string(), .line = 0, .key = {}};
      return make_error(error_code::IO, "path is a directory, not a file", where);
    }
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
      const location where{.file = path.string(), .line = 0, .key = {}};
      return make_error(error_code::IO, "cannot open file for reading", where);
    }
    const std::string name = path.string();
    return read_text_stream(input, name);
  }

  std::expected<std::string, error> read_text_stream(std::istream &input, const std::string &name) {
    constexpr std::size_t chunk_bytes = 4096;
    std::array<char, chunk_bytes> chunk{};
    std::string content;
    // istream-level reads turn a failing streambuf into badbit instead of an exception (libstdc++ throws from
    // underflow on a read(2) error, e.g. EIO).
    while (input.good()) {
      input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
      content.append(chunk.data(), static_cast<std::size_t>(input.gcount()));
    }
    if (input.bad()) {
      const location where{.file = name, .line = 0, .key = {}};
      return make_error(error_code::IO, "cannot read file", where);
    }
    return content;
  }
} // namespace mf::io
