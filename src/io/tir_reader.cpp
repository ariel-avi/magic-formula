#include <cstddef>
#include <expected>
#include <format>
#include <magic_formula/detail/text.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/io/number.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/io/tir_reader.hpp>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mf::io {
  namespace {
    /// Marks `entry` (an element of `document.entries()`, or nullptr) as consumed and returns it.
    const tir_entry *mark_consumed(const tir_document &document, std::vector<bool> &consumed, const tir_entry *entry) {
      if (entry != nullptr) {
        const auto index = static_cast<std::size_t>(entry - document.entries().data());
        consumed[index] = true;
      }
      return entry;
    }

    /// The value text of `entry`, or nullopt for nullptr.
    std::optional<std::string_view> value_of(const tir_entry *entry) {
      if (entry == nullptr) {
        return std::nullopt;
      }
      return std::string_view{entry->value};
    }

    /// File, line and key of an existing entry.
    location entry_location(const tir_document &document, const tir_entry &entry) {
      std::string file = document.file_name();
      std::string key = entry.key;
      return location{.file = std::move(file), .line = entry.line, .key = std::move(key)};
    }

    /// Converts the value of `entry` (nullptr = absent) to a double; see `tir_reader::number`.
    std::expected<std::optional<double>, error> number_of(const tir_document &document, const tir_entry *entry) {
      if (entry == nullptr) {
        return std::optional<double>{};
      }
      if (entry->quoted) {
        return make_error(error_code::INVALID_VALUE,
                          std::format("{}: expected a number, got the string '{}'", entry->key, entry->value),
                          entry_location(document, *entry));
      }
      const std::optional<double> parsed = parse_number(entry->value);
      if (!parsed.has_value()) {
        return make_error(error_code::INVALID_NUMBER,
                          std::format("{} = '{}' is not a valid number", entry->key, entry->value),
                          entry_location(document, *entry));
      }
      return parsed;
    }
  } // namespace

  tir_reader::tir_reader(const tir_document &document)
      : document_(&document), consumed_(document.entries().size(), false) {}

  const tir_document &tir_reader::document() const noexcept {
    return *document_;
  }

  bool tir_reader::contains(std::string_view key) {
    return mark_consumed(*document_, consumed_, document_->find(key)) != nullptr;
  }

  std::optional<std::string_view> tir_reader::text(std::string_view key) {
    return value_of(mark_consumed(*document_, consumed_, document_->find(key)));
  }

  std::optional<std::string_view> tir_reader::text(std::string_view section, std::string_view key) {
    return value_of(mark_consumed(*document_, consumed_, document_->find(section, key)));
  }

  std::expected<std::optional<double>, error> tir_reader::number(std::string_view key) {
    return number_of(*document_, mark_consumed(*document_, consumed_, document_->find(key)));
  }

  std::expected<std::optional<int>, error> tir_reader::integer(std::string_view key) {
    const tir_entry *const entry = mark_consumed(*document_, consumed_, document_->find(key));
    const auto number_result = number_of(*document_, entry);
    if (!number_result.has_value()) {
      return std::unexpected<error>(number_result.error());
    }
    if (entry == nullptr) {
      return std::optional<int>{};
    }
    const std::optional<int> value = parse_integer(entry->value);
    if (!value.has_value()) {
      return make_error(error_code::INVALID_VALUE, std::format("{} = {} is not an integer", entry->key, entry->value),
                        entry_location(*document_, *entry));
    }
    return value;
  }

  location tir_reader::locate(std::string_view key) const {
    const tir_entry *const entry = document_->find(key);
    const std::size_t line = entry == nullptr ? std::size_t{0} : entry->line;
    std::string file = document_->file_name();
    std::string upper_key = detail::to_upper_ascii(key);
    return location{.file = std::move(file), .line = line, .key = std::move(upper_key)};
  }

  std::vector<location> tir_reader::take_section(std::string_view section) {
    const std::span<const tir_entry> entries = document_->entries();
    const tir_document &document = *document_;
    const auto is_in_section = [entries, section](std::size_t index) {
      return detail::iequals_ascii(entries[index].section, section);
    };
    const auto locate_entry = [entries, &document](std::size_t index) {
      return entry_location(document, entries[index]);
    };
    auto in_section = std::views::iota(std::size_t{0}, entries.size()) | std::views::filter(is_in_section);
    for (const std::size_t index : in_section) {
      consumed_[index] = true;
    }
    return in_section | std::views::transform(locate_entry) | std::ranges::to<std::vector<location>>();
  }

  std::vector<std::string> tir_reader::unconsumed_keys() const {
    const std::span<const tir_entry> entries = document_->entries();
    return std::views::iota(std::size_t{0}, entries.size()) |
           std::views::filter([this](std::size_t index) { return !consumed_[index]; }) |
           std::views::transform([entries](std::size_t index) { return entries[index].key; }) |
           std::ranges::to<std::vector<std::string>>();
  }
} // namespace mf::io
