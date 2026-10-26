#include <cstddef>
#include <expected>
#include <format>
#include <iterator>
#include <magic_formula/detail/text.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/io/tir_editor.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace mf::io {
  namespace {
    /// INVALID_VALUE for the entry `key` at `line` (no file name: the caller only has text).
    std::unexpected<error> invalid_value(std::string message, std::size_t line, const std::string &key) {
      std::string key_copy = key;
      location where{.file = {}, .line = line, .key = std::move(key_copy)};
      return make_error(error_code::INVALID_VALUE, std::move(message), std::move(where));
    }
  } // namespace

  std::expected<std::string, error> replace_value(std::string_view text, std::string_view section, std::string_view key,
                                                  std::string_view value) {
    const auto document = tir_document::parse(text);
    if (!document.has_value()) {
      return std::unexpected<error>(document.error());
    }
    const tir_entry *const entry = document->find(section, key);
    if (entry == nullptr) {
      std::string upper_key = detail::to_upper_ascii(key);
      std::string upper_section = detail::to_upper_ascii(section);
      std::string message = std::format("no key {} in section [{}]", upper_key, upper_section);
      location where{.file = {}, .line = 0, .key = std::move(upper_key)};
      return make_error(error_code::MISSING_KEY, std::move(message), std::move(where));
    }
    if (value.find_first_of("\r\n") != std::string_view::npos) {
      return invalid_value(std::format("value of {} must not contain a line break", entry->key), entry->line,
                           entry->key);
    }
    std::string edited{text.substr(0, entry->value_offset)};
    edited += entry->quoted ? "'" : "";
    edited += value;
    edited += entry->quoted ? "'" : "";
    edited += text.substr(entry->value_offset + entry->value_length);

    const auto reparsed = tir_document::parse(edited);
    if (!reparsed.has_value()) {
      return invalid_value(std::format("value of {} is not valid .tir text: {}", entry->key, reparsed.error().message),
                           entry->line, entry->key);
    }
    // The index is stable: every byte before the edited value is unchanged, the edited line keeps its key and '=', and
    // the line-break guard above means no line is added or removed.
    const auto index = static_cast<std::size_t>(std::distance(document->entries().data(), entry));
    const tir_entry &back = reparsed->entries()[index];
    if (back.quoted != entry->quoted || back.value != value) {
      return invalid_value(std::format("value of {} does not read back unchanged (quotes, '$' and surrounding blanks "
                                       "are not allowed in this value)",
                                       entry->key),
                           entry->line, entry->key);
    }
    return edited;
  }
} // namespace mf::io
