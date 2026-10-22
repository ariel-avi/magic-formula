#include <array>
#include <format>
#include <gtest/gtest.h>
#include <magic_formula/error.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/io/tir_reader.hpp>
#include <magic_formula/io/units.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
  using mf::error_code;
  using mf::io::check_units;
  using mf::io::tir_document;
  using mf::io::tir_reader;

  constexpr std::string_view FILE_NAME = "u.tir";

  /// The six keys that check_units looks at, in table order.
  constexpr std::array<std::string_view, 6> UNIT_KEYS{"LENGTH", "FORCE", "ANGLE", "MASS", "TIME", "PRESSURE"};

  /// A [UNITS] section that names the SI unit of all six keys.
  constexpr std::string_view ALL_SI = R"([UNITS]
LENGTH = 'meter'
FORCE = 'newton'
ANGLE = 'radians'
MASS = 'kg'
TIME = 'second'
PRESSURE = 'pascal'
)";

  class units_wrong : public ::testing::TestWithParam<std::string_view> {};

  /// Parses `text`; a parse failure is a test failure and yields an empty document.
  tir_document make_document(std::string_view text) {
    auto result = tir_document::parse(text, std::string(FILE_NAME));
    if (!result.has_value()) {
      ADD_FAILURE() << mf::to_string(result.error());
      return tir_document::parse("", std::string(FILE_NAME)).value();
    }
    return std::move(result).value();
  }
} // namespace

TEST(units, CheckUnits_NoUnitsSection_Succeeds) {
  // U2: an absent [UNITS] section means SI.
  const tir_document document = make_document("[MODEL]\nFITTYP = 6\n");
  tir_reader reader(document);
  EXPECT_TRUE(check_units(reader).has_value());
  const std::vector<std::string> expected{"FITTYP"};
  EXPECT_EQ(reader.unconsumed_keys(), expected);
}

TEST(units, CheckUnits_AllSiSymbols_Succeeds) {
  const tir_document document = make_document(ALL_SI);
  tir_reader reader(document);
  EXPECT_TRUE(check_units(reader).has_value());
  EXPECT_TRUE(reader.unconsumed_keys().empty());
}

TEST(units, CheckUnits_UnitsSectionWithoutKeys_Succeeds) {
  const tir_document document = make_document("[UNITS]\n");
  tir_reader reader(document);
  EXPECT_TRUE(check_units(reader).has_value());
}

TEST(units, CheckUnits_UppercaseSymbol_Succeeds) {
  // U3: symbols are compared case-insensitively.
  const tir_document document = make_document("[UNITS]\nFORCE = 'NEWTON'\n");
  tir_reader reader(document);
  EXPECT_TRUE(check_units(reader).has_value());
}

TEST(units, CheckUnits_UnquotedSymbol_Succeeds) {
  const tir_document document = make_document("[UNITS]\nLENGTH = meter\n");
  tir_reader reader(document);
  EXPECT_TRUE(check_units(reader).has_value());
}

TEST(units, CheckUnits_BlanksInsideQuotes_Succeeds) {
  const tir_document document = make_document("[UNITS]\nLENGTH = '  Meter '\n");
  tir_reader reader(document);
  EXPECT_TRUE(check_units(reader).has_value());
}

TEST(units, CheckUnits_Millimeter_ReturnsUnsupportedUnit) {
  // U1: only SI is supported.
  const tir_document document = make_document("[MDI_HEADER]\nFILE_TYPE = 'tir'\n[UNITS]\nLENGTH = 'mm'\n");
  tir_reader reader(document);
  const auto result = check_units(reader);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_UNIT);
  EXPECT_EQ(result.error().where.key, "LENGTH");
  EXPECT_EQ(result.error().where.line, 4U);
  EXPECT_EQ(result.error().where.file, FILE_NAME);
  EXPECT_EQ(result.error().message, "[UNITS] LENGTH = 'mm' is not supported; only SI 'meter' is accepted");
}

TEST_P(units_wrong, CheckUnits_WrongSymbol_ReturnsUnsupportedUnitNamingKeyAndValue) {
  const std::string_view key = GetParam();
  const tir_document document = make_document(std::format("[UNITS]\n{} = 'bogus'\n", key));
  tir_reader reader(document);
  const auto result = check_units(reader);
  ASSERT_FALSE(result.has_value()) << key;
  EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_UNIT) << key;
  EXPECT_EQ(result.error().where.key, key);
  EXPECT_EQ(result.error().where.line, 2U) << key;
  EXPECT_EQ(result.error().where.file, FILE_NAME) << key;
  EXPECT_TRUE(result.error().message.contains(key)) << result.error().message;
  EXPECT_TRUE(result.error().message.contains("'bogus'")) << result.error().message;
}

INSTANTIATE_TEST_SUITE_P(keys, units_wrong, ::testing::ValuesIn(UNIT_KEYS));

TEST(units, CheckUnits_TwoWrongKeys_ReportsFirstInTableOrder) {
  const tir_document document = make_document("[UNITS]\nANGLE = 'degrees'\nFORCE = 'lbf'\n");
  tir_reader reader(document);
  const auto result = check_units(reader);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().where.key, "FORCE");
  EXPECT_EQ(result.error().where.line, 3U);
}

TEST(units, CheckUnits_MassOutsideUnits_NotChecked) {
  const tir_document document = make_document("[INERTIA]\nMASS = 9.3\n");
  tir_reader reader(document);
  EXPECT_TRUE(check_units(reader).has_value());
  const std::vector<std::string> expected{"MASS"};
  EXPECT_EQ(reader.unconsumed_keys(), expected);
}

TEST(units, CheckUnits_MassInBothSections_ChecksOnlyTheUnitsEntry) {
  // [INERTIA] MASS comes first in the file; the check must still use [UNITS] MASS (line 4).
  const tir_document document = make_document("[INERTIA]\nMASS = 9.3\n[UNITS]\nMASS = 'lb'\n");
  tir_reader reader(document);
  const auto result = check_units(reader);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::UNSUPPORTED_UNIT);
  EXPECT_EQ(result.error().where.key, "MASS");
  EXPECT_EQ(result.error().where.line, 4U);
}

TEST(units, CheckUnits_MassInBothSectionsWithKg_SucceedsAndLeavesInertiaMassUnconsumed) {
  const tir_document document = make_document("[UNITS]\nMASS = 'kg'\n[INERTIA]\nMASS = 9.3\n");
  tir_reader reader(document);
  EXPECT_TRUE(check_units(reader).has_value());
  const std::vector<std::string> expected{"MASS"};
  EXPECT_EQ(reader.unconsumed_keys(), expected);
}

TEST(units, CheckUnits_UnknownUnitsKey_StaysUnconsumed) {
  const tir_document document = make_document("[UNITS]\nTEMPERATURE = 'kelvin'\n");
  tir_reader reader(document);
  EXPECT_TRUE(check_units(reader).has_value());
  const std::vector<std::string> expected{"TEMPERATURE"};
  EXPECT_EQ(reader.unconsumed_keys(), expected);
}
