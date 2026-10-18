#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <magic_formula/error.hpp>
#include <magic_formula/io/text_file.hpp>
#include <string>

namespace mf::io {
  std::expected<std::string, error> read_text_file(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
      const location where{.file = path.string(), .line = 0, .key = {}};
      return make_error(error_code::IO, "cannot open file for reading", where);
    }
    constexpr std::size_t chunk_bytes = 4096;
    std::array<char, chunk_bytes> chunk{};
    std::string content;
    // istream-level reads turn a failing streambuf (for example a directory) into badbit instead of an exception.
    while (input.good()) {
      input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
      content.append(chunk.data(), static_cast<std::size_t>(input.gcount()));
    }
    if (input.bad()) {
      const location where{.file = path.string(), .line = 0, .key = {}};
      return make_error(error_code::IO, "cannot read file", where);
    }
    return content;
  }
} // namespace mf::io
