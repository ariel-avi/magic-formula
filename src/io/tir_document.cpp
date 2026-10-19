#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <magic_formula/detail/text.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/io/text_file.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mf::io {
  namespace {
    /// What the next content line of the current section means.
    enum class section_mode : std::uint8_t {
      NONE,      ///< no section header seen yet
      UNDECIDED, ///< section opened, no content line yet (a leading '{' makes it a table)
      KEYS,      ///< `KEY = value` section
      TABLE,     ///< table section: every content line is a row
    };

    /// Mutable parse state. The pointers refer to members of the `tir_document` under construction.
    struct parse_state {
      const std::string *file = nullptr;
      std::vector<tir_entry> *entries = nullptr;
      std::vector<tir_table> *tables = nullptr;
      std::vector<warning> *warnings = nullptr;
      /// Upper-case name of the current section; empty before the first header.
      std::string section;
      /// Line of the current section header.
      std::size_t section_line = 0;
      section_mode mode = section_mode::NONE;
      /// Row sink of the current table section (the table's rows, or `discarded_rows` for a duplicate table).
      std::vector<std::string> *rows = nullptr;
      std::vector<std::string> discarded_rows;
    };

    /// A parsed value: the text and whether it was quoted.
    struct parsed_value {
      std::string text;
      bool quoted = false;
    };

    /// Characters allowed in a key (AD-20).
    constexpr std::string_view KEY_CHARACTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";

    /// SYNTAX error at the current file and the given line.
    std::unexpected<error> syntax_error(const parse_state &state, std::size_t line, std::string message,
                                        std::string key = {}) {
      // Every throwing copy sits in its own statement; the aggregate is built from noexcept moves only.
      std::string file = *state.file;
      location where{.file = std::move(file), .line = line, .key = std::move(key)};
      return make_error(error_code::SYNTAX, std::move(message), std::move(where));
    }

    /// True when `key` is non-empty and made of `KEY_CHARACTERS` only.
    bool is_valid_key(std::string_view key) {
      return !key.empty() &&
             std::ranges::all_of(key, [](char ch) { return KEY_CHARACTERS.find(ch) != std::string_view::npos; });
    }

    /// SYNTAX error at the line of the first NUL byte (1-based: '\n' count before it plus one).
    std::expected<void, error> check_no_nul(std::string_view text, const std::string &file) {
      const std::size_t nul = text.find('\0');
      if (nul == std::string_view::npos) {
        return {};
      }
      const auto newlines = static_cast<std::size_t>(std::ranges::count(text.substr(0, nul), '\n'));
      std::string message{"NUL byte in file (not ASCII/UTF-8 text)"};
      std::string file_copy = file;
      location where{.file = std::move(file_copy), .line = newlines + 1, .key = {}};
      return make_error(error_code::SYNTAX, std::move(message), std::move(where));
    }

    /// `[name]` line: opens a section. Only blanks or a `$` comment may follow the ']' ('!' is not a trailing comment).
    std::expected<void, error> handle_header(parse_state &state, std::string_view text, std::size_t line) {
      const std::size_t close = text.find(']');
      if (close == std::string_view::npos) {
        return syntax_error(state, line, "unterminated section header");
      }
      const std::string_view name = detail::trim(text.substr(1, close - 1));
      if (name.empty()) {
        return syntax_error(state, line, "empty section name");
      }
      const std::string_view rest = detail::trim(text.substr(close + 1));
      if (!rest.empty() && rest.front() != '$') {
        return syntax_error(state, line, "unexpected text after section header");
      }
      state.section = detail::to_upper_ascii(name);
      state.section_line = line;
      state.mode = section_mode::UNDECIDED;
      return {};
    }

    /// The `{...}` column header of a table section: starts the table. A second table with the same section name is
    /// a warning and its rows are discarded (the first one wins).
    void begin_table(parse_state &state) {
      state.mode = section_mode::TABLE;
      const auto first = std::ranges::find_if(
          *state.tables, [&state](const tir_table &table) { return table.section == state.section; });
      if (first != state.tables->end()) {
        state.discarded_rows.clear();
        state.rows = &state.discarded_rows;
        std::string message = std::format("duplicate table section [{}] (first at line {}) ignored; the first one wins",
                                          state.section, first->line);
        std::string file = *state.file;
        std::string section = state.section;
        location where{.file = std::move(file), .line = state.section_line, .key = std::move(section)};
        warning duplicate{.message = std::move(message), .where = std::move(where)};
        state.warnings->push_back(std::move(duplicate));
        return;
      }
      std::string section = state.section;
      tir_table table{.section = std::move(section), .line = state.section_line, .rows = {}};
      state.tables->push_back(std::move(table));
      state.rows = &state.tables->back().rows;
    }

    /// A content line of a table section: the row without its trailing `$` comment, trimmed, kept raw.
    std::expected<void, error> handle_table_row(parse_state &state, std::string_view text, std::size_t line) {
      const std::string_view row = detail::trim(text.substr(0, text.find('$')));
      if (row.contains('=')) {
        return syntax_error(state, line, std::format("'=' inside table section [{}]", state.section));
      }
      state.rows->emplace_back(row);
      return {};
    }

    /// Value part of `KEY = value`: single-quoted (verbatim between the quotes; only blanks or a `$` comment may
    /// follow) or unquoted (cut at the first `$`, trimmed, may be empty; '!' is part of the value).
    std::expected<parsed_value, error> parse_value(const parse_state &state, std::size_t line, const std::string &key,
                                                   std::string_view raw) {
      const std::string_view value = detail::trim(raw);
      if (!value.starts_with('\'')) {
        return parsed_value{.text = std::string(detail::trim(value.substr(0, value.find('$')))), .quoted = false};
      }
      const std::size_t close = value.find('\'', 1);
      if (close == std::string_view::npos) {
        return syntax_error(state, line, "unterminated quoted string", key);
      }
      const std::string_view after = detail::trim(value.substr(close + 1));
      if (!after.empty() && after.front() != '$') {
        return syntax_error(state, line, "unexpected text after quoted value", key);
      }
      return parsed_value{.text = std::string(value.substr(1, close - 1)), .quoted = true};
    }

    /// Earlier entry with the same identity as `key` in the current section (identity = key, or (section, key) for
    /// `SECTION_SCOPED_KEYS`); nullptr when none.
    const tir_entry *find_duplicate(const parse_state &state, const std::string &key) {
      const bool scoped = std::ranges::contains(SECTION_SCOPED_KEYS, std::string_view{key});
      const auto match = std::ranges::find_if(*state.entries, [&](const tir_entry &entry) {
        return entry.key == key && (!scoped || entry.section == state.section);
      });
      return match == state.entries->end() ? nullptr : &*match;
    }

    /// A `KEY = value` line.
    std::expected<void, error> handle_entry(parse_state &state, std::string_view text, std::size_t line) {
      const std::size_t equals = text.find('=');
      if (equals == std::string_view::npos) {
        return syntax_error(state, line, "missing '=' (expected KEY = value)");
      }
      const std::string_view key_text = detail::trim(text.substr(0, equals));
      if (!is_valid_key(key_text)) {
        return syntax_error(state, line, std::format("invalid key '{}'", key_text));
      }
      std::string key = detail::to_upper_ascii(key_text);
      auto value = parse_value(state, line, key, text.substr(equals + 1));
      if (!value.has_value()) {
        return std::unexpected<error>(std::move(value.error()));
      }
      const tir_entry *const earlier = find_duplicate(state, key);
      if (earlier != nullptr) {
        std::string message = std::format("duplicate key {} (first defined at line {})", key, earlier->line);
        std::string file = *state.file;
        location where{.file = std::move(file), .line = line, .key = std::move(key)};
        return make_error(error_code::DUPLICATE_KEY, std::move(message), std::move(where));
      }
      std::string section = state.section;
      tir_entry entry{.section = std::move(section),
                      .key = std::move(key),
                      .value = std::move(value->text),
                      .quoted = value->quoted,
                      .line = line};
      state.entries->push_back(std::move(entry));
      return {};
    }

    /// One physical line (without its '\n').
    std::expected<void, error> handle_line(parse_state &state, std::string_view raw, std::size_t line) {
      const std::string_view text = detail::trim(raw);
      if (text.empty() || text.front() == '$' || text.front() == '!') {
        return {};
      }
      if (text.front() == '[') {
        return handle_header(state, text, line);
      }
      if (state.mode == section_mode::NONE) {
        return syntax_error(state, line, "content before the first section header");
      }
      if (state.mode == section_mode::UNDECIDED) {
        if (text.front() == '{') {
          begin_table(state);
          return {};
        }
        state.mode = section_mode::KEYS;
      }
      if (state.mode == section_mode::TABLE) {
        return handle_table_row(state, text, line);
      }
      return handle_entry(state, text, line);
    }

    /// Splits `text` at '\n' (a final line without '\n' is a normal line) and handles each line in order.
    std::expected<void, error> parse_lines(parse_state &state, std::string_view text) {
      std::size_t line_number = 0;
      while (!text.empty()) {
        ++line_number;
        const std::size_t newline = text.find('\n');
        const std::string_view line = text.substr(0, newline);
        text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);
        auto handled = handle_line(state, line, line_number);
        if (!handled.has_value()) {
          return handled;
        }
      }
      return {};
    }

    /// Compares a stored upper-case name with a caller-supplied name, ignoring ASCII case.
    bool same_name(const std::string &stored, std::string_view wanted) noexcept {
      return detail::iequals_ascii(stored, wanted);
    }
  } // namespace

  std::expected<tir_document, error> tir_document::parse(std::string_view text, std::string file_name) {
    tir_document document;
    document.file_name_ = std::move(file_name);
    const auto no_nul = check_no_nul(text, document.file_name_);
    if (!no_nul.has_value()) {
      return std::unexpected<error>(no_nul.error());
    }
    constexpr std::string_view bom = "\xEF\xBB\xBF";
    if (text.starts_with(bom)) {
      text.remove_prefix(bom.size());
    }
    parse_state state;
    state.file = &document.file_name_;
    state.entries = &document.entries_;
    state.tables = &document.tables_;
    state.warnings = &document.warnings_;
    const auto parsed = parse_lines(state, text);
    if (!parsed.has_value()) {
      return std::unexpected<error>(parsed.error());
    }
    return document;
  }

  const tir_entry *tir_document::find(std::string_view key) const noexcept {
    const auto match =
        std::ranges::find_if(entries_, [key](const tir_entry &entry) { return same_name(entry.key, key); });
    return match == entries_.end() ? nullptr : &*match;
  }

  const tir_entry *tir_document::find(std::string_view section, std::string_view key) const noexcept {
    const auto match = std::ranges::find_if(entries_, [section, key](const tir_entry &entry) {
      return same_name(entry.section, section) && same_name(entry.key, key);
    });
    return match == entries_.end() ? nullptr : &*match;
  }

  std::span<const tir_entry> tir_document::entries() const noexcept {
    return entries_;
  }

  std::span<const tir_table> tir_document::tables() const noexcept {
    return tables_;
  }

  std::span<const warning> tir_document::warnings() const noexcept {
    return warnings_;
  }

  const std::string &tir_document::file_name() const noexcept {
    return file_name_;
  }

  std::expected<tir_document, error> read_tir_file(const std::filesystem::path &path) {
    const auto text = read_text_file(path);
    if (!text.has_value()) {
      return std::unexpected<error>(text.error());
    }
    return tir_document::parse(*text, path.string());
  }
} // namespace mf::io
