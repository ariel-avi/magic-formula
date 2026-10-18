#include <format>
#include <magic_formula/error.hpp>
#include <string>
#include <utility>

namespace mf {
  namespace {
    /// "<location>: <message>", or "<message>" when the location string is empty.
    std::string prefixed(const location &where, const std::string &message) {
      const std::string prefix = to_string(where);
      if (prefix.empty()) {
        return message;
      }
      return std::format("{}: {}", prefix, message);
    }
  } // namespace

  std::unexpected<error> make_error(error_code code, std::string message, location where) {
    return std::unexpected<error>(error{.code = code, .message = std::move(message), .where = std::move(where)});
  }

  std::string to_string(const location &where) {
    if (where.file.empty()) {
      if (where.line > 0) {
        return std::format("line {}", where.line);
      }
      return {};
    }
    if (where.line > 0) {
      return std::format("{}:{}", where.file, where.line);
    }
    return where.file;
  }

  std::string to_string(const error &value) {
    return prefixed(value.where, value.message);
  }

  std::string to_string(const warning &value) {
    return prefixed(value.where, value.message);
  }
} // namespace mf
