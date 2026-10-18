#include <array>
#include <expected>
#include <format>
#include <magic_formula/detail/text.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/io/tir_reader.hpp>
#include <magic_formula/io/units.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace mf::io {
  namespace {
    /// A [UNITS] key and the only unit name accepted for it.
    struct si_unit {
      std::string_view key;
      std::string_view symbol;
    };

    // clang-format off
    /// The single unit table (AD-10): every dimensional [UNITS] key with its SI unit name.
    constexpr std::array<si_unit, 6> SI_UNITS{
        si_unit{.key = "LENGTH", .symbol = "meter"},
        si_unit{.key = "FORCE", .symbol = "newton"},
        si_unit{.key = "ANGLE", .symbol = "radians"},
        si_unit{.key = "MASS", .symbol = "kg"},
        si_unit{.key = "TIME", .symbol = "second"},
        si_unit{.key = "PRESSURE", .symbol = "pascal"},
    };
    // clang-format on
  } // namespace

  std::expected<void, error> check_units(tir_reader &reader) {
    for (const si_unit &unit : SI_UNITS) {
      const std::optional<std::string_view> value = reader.text("UNITS", unit.key);
      if (value.has_value() && !detail::iequals_ascii(detail::trim(*value), unit.symbol)) {
        // text() returned a value, so the entry exists.
        const tir_entry *const entry = reader.document().find("UNITS", unit.key);
        location where;
        where.file = reader.document().file_name();
        where.line = entry->line;
        where.key = unit.key;
        std::string message = std::format("[UNITS] {} = '{}' is not supported; only SI '{}' is accepted (AD-10)",
                                          unit.key, *value, unit.symbol);
        return make_error(error_code::UNSUPPORTED_UNIT, std::move(message), std::move(where));
      }
    }
    return {};
  }
} // namespace mf::io
