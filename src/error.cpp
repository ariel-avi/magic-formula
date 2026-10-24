#include <format>
#include <magic_formula/error.hpp>
#include <optional>
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

    /// what() text of a model_failure: the error, prefixed with the batch row when there is one.
    std::string failure_text(const error &cause, const std::optional<std::size_t> &row) {
      if (row.has_value()) {
        return std::format("row {}: {}", *row, to_string(cause));
      }
      return to_string(cause);
    }
  } // namespace

  model_failure::model_failure(error cause, std::optional<std::size_t> row)
      : std::runtime_error(failure_text(cause, row)), cause_(std::move(cause)), row_(row) {}

  const error &model_failure::cause() const noexcept {
    return cause_;
  }

  std::optional<std::size_t> model_failure::row() const noexcept {
    return row_;
  }

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
