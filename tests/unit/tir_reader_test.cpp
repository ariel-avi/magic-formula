#include <gtest/gtest.h>
#include <magic_formula/error.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/io/tir_reader.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
  using mf::error_code;
  using mf::io::tir_document;
  using mf::io::tir_reader;

  /// Absolute tolerance for values that are exactly representable (parsed from short decimal text).
  constexpr double VALUE_TOL = 1e-15;

  constexpr std::string_view FILE_NAME = "r.tir";

  /// Document under test; the tests rely on these line numbers: [UNITS] 1, MASS 2, [INERTIA] 3, MASS 4, [A] 5, NUM 6,
  /// EMPTY 7, JUNK 8, QUOTED 9, INT 10, INTD 11, FRAC 12, TEXT 13, BARE 14.
  constexpr std::string_view SAMPLE = R"([UNITS]
MASS = 'kg'
[INERTIA]
MASS = 9.3
[A]
NUM = 4.5
EMPTY =
JUNK = 1 ! x
QUOTED = '12'
INT = 6
INTD = 6.0
FRAC = 5.5
TEXT = 'hello'
BARE = LEFT
)";

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

TEST(tir_reader, Number_Absent_ReturnsEmptyOptional) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  const auto result = reader.number("NOPE");
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->has_value());
}

TEST(tir_reader, Number_Quoted_ReturnsInvalidValue) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  const auto result = reader.number("quoted");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::INVALID_VALUE);
  EXPECT_EQ(result.error().where.file, FILE_NAME);
  EXPECT_EQ(result.error().where.line, 9U);
  EXPECT_EQ(result.error().where.key, "QUOTED");
  EXPECT_TRUE(result.error().message.contains("QUOTED: expected a number, got the string '12'"))
      << result.error().message;
}

TEST(tir_reader, Number_Empty_ReturnsInvalidNumberWithLineAndKey) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  const auto result = reader.number("EMPTY");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::INVALID_NUMBER);
  EXPECT_EQ(result.error().where.file, FILE_NAME);
  EXPECT_EQ(result.error().where.line, 7U);
  EXPECT_EQ(result.error().where.key, "EMPTY");
  EXPECT_TRUE(result.error().message.contains("EMPTY = '' is not a valid number")) << result.error().message;
}

TEST(tir_reader, Number_Junk_ReturnsInvalidNumber) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  const auto result = reader.number("JUNK");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::INVALID_NUMBER);
  EXPECT_EQ(result.error().where.line, 8U);
  EXPECT_TRUE(result.error().message.contains("'1 ! x'")) << result.error().message;
}

TEST(tir_reader, Number_Valid_ReturnsValue) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  const auto result = reader.number("Num");
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_value());
  EXPECT_NEAR(**result, 4.5, VALUE_TOL);
}

TEST(tir_reader, Integer_Valid_ReturnsInt) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  const auto result = reader.integer("INT");
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ(**result, 6);
}

TEST(tir_reader, Integer_IntegralDouble_Accepted) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  const auto result = reader.integer("INTD");
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ(**result, 6);
}

TEST(tir_reader, Integer_NonIntegral_ReturnsInvalidValue) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  const auto result = reader.integer("FRAC");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::INVALID_VALUE);
  EXPECT_EQ(result.error().where.file, FILE_NAME);
  EXPECT_EQ(result.error().where.line, 12U);
  EXPECT_EQ(result.error().where.key, "FRAC");
  EXPECT_TRUE(result.error().message.contains("FRAC = 5.5 is not an integer")) << result.error().message;
}

TEST(tir_reader, Integer_NumberError_Propagated) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  const auto quoted = reader.integer("QUOTED");
  ASSERT_FALSE(quoted.has_value());
  EXPECT_EQ(quoted.error().code, error_code::INVALID_VALUE);
  EXPECT_EQ(quoted.error().where.line, 9U);
  const auto junk = reader.integer("JUNK");
  ASSERT_FALSE(junk.has_value());
  EXPECT_EQ(junk.error().code, error_code::INVALID_NUMBER);
  EXPECT_EQ(junk.error().where.line, 8U);
}

TEST(tir_reader, Integer_Absent_ReturnsEmptyOptional) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  const auto result = reader.integer("NOPE");
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->has_value());
}

TEST(tir_reader, Text_QuotedAndBare_ReturnValue) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  const std::optional<std::string_view> quoted = reader.text("text");
  const std::optional<std::string_view> bare = reader.text("BARE");
  ASSERT_TRUE(quoted.has_value());
  ASSERT_TRUE(bare.has_value());
  EXPECT_EQ(*quoted, "hello");
  EXPECT_EQ(*bare, "LEFT");
}

TEST(tir_reader, Text_Absent_ReturnsNullopt) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  EXPECT_FALSE(reader.text("NOPE").has_value());
}

TEST(tir_reader, TextSection_ScopedLookup) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  const std::optional<std::string_view> units_mass = reader.text("UNITS", "MASS");
  const std::optional<std::string_view> inertia_mass = reader.text("inertia", "mass");
  ASSERT_TRUE(units_mass.has_value());
  ASSERT_TRUE(inertia_mass.has_value());
  EXPECT_EQ(*units_mass, "kg");
  EXPECT_EQ(*inertia_mass, "9.3");
  EXPECT_FALSE(reader.text("A", "MASS").has_value());
  // The key-only lookup returns the first entry in file order.
  const std::optional<std::string_view> first = reader.text("MASS");
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(*first, "kg");
}

TEST(tir_reader, Contains_PresentAndAbsent) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  EXPECT_TRUE(reader.contains("num"));
  EXPECT_FALSE(reader.contains("zzz"));
  const std::vector<std::string> unconsumed = reader.unconsumed_keys();
  EXPECT_EQ(unconsumed.size(), document.entries().size() - 1);
  for (const std::string &key : unconsumed) {
    EXPECT_NE(key, "NUM");
  }
}

TEST(tir_reader, Locate_Present_ReturnsFileLineKey) {
  const tir_document document = make_document(SAMPLE);
  const tir_reader reader(document);
  const mf::location where = reader.locate("num");
  EXPECT_EQ(where.file, FILE_NAME);
  EXPECT_EQ(where.line, 6U);
  EXPECT_EQ(where.key, "NUM");
}

TEST(tir_reader, Locate_Absent_ReturnsLineZero) {
  const tir_document document = make_document(SAMPLE);
  const tir_reader reader(document);
  const mf::location where = reader.locate("nope");
  EXPECT_EQ(where.file, FILE_NAME);
  EXPECT_EQ(where.line, 0U);
  EXPECT_EQ(where.key, "NOPE");
  // locate() does not consume anything.
  EXPECT_EQ(reader.unconsumed_keys().size(), document.entries().size());
}

TEST(tir_reader, UnconsumedKeys_AfterMixedConsumption_FileOrder) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  ASSERT_EQ(reader.unconsumed_keys().size(), 11U);

  EXPECT_TRUE(reader.text("INERTIA", "MASS").has_value());
  EXPECT_TRUE(reader.number("NUM").has_value());
  // A failing conversion still consumes the entry.
  EXPECT_FALSE(reader.number("QUOTED").has_value());
  EXPECT_TRUE(reader.text("TEXT").has_value());
  EXPECT_TRUE(reader.integer("INT").has_value());
  EXPECT_FALSE(reader.integer("FRAC").has_value());

  const std::vector<std::string> expected{"MASS", "EMPTY", "JUNK", "INTD", "BARE"};
  EXPECT_EQ(reader.unconsumed_keys(), expected);
}

TEST(tir_reader, TakeSection_Present_ReturnsLocationsInFileOrderAndConsumes) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  const std::vector<mf::location> taken = reader.take_section("a");
  ASSERT_EQ(taken.size(), 9U);
  EXPECT_EQ(taken.front().file, FILE_NAME);
  EXPECT_EQ(taken.front().line, 6U);
  EXPECT_EQ(taken.front().key, "NUM");
  EXPECT_EQ(taken.back().line, 14U);
  EXPECT_EQ(taken.back().key, "BARE");

  // The entries of [A] are consumed; [UNITS] MASS and [INERTIA] MASS are not.
  const std::vector<std::string> expected{"MASS", "MASS"};
  EXPECT_EQ(reader.unconsumed_keys(), expected);
}

TEST(tir_reader, TakeSection_InertiaOnly_LeavesUnitsMassUnconsumed) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  const std::vector<mf::location> taken = reader.take_section("INERTIA");
  ASSERT_EQ(taken.size(), 1U);
  EXPECT_EQ(taken[0].line, 4U);
  EXPECT_EQ(taken[0].key, "MASS");

  // The unit lookup of [UNITS] still works and finds its own entry.
  const std::optional<std::string_view> units_mass = reader.text("UNITS", "MASS");
  ASSERT_TRUE(units_mass.has_value());
  EXPECT_EQ(*units_mass, "kg");
  EXPECT_EQ(reader.unconsumed_keys().size(), 9U);
}

TEST(tir_reader, TakeSection_Absent_ReturnsEmptyAndConsumesNothing) {
  const tir_document document = make_document(SAMPLE);
  tir_reader reader(document);
  EXPECT_TRUE(reader.take_section("STRUCTURAL").empty());
  EXPECT_EQ(reader.unconsumed_keys().size(), document.entries().size());
}

TEST(tir_reader, Document_ReturnsReference) {
  const tir_document document = make_document(SAMPLE);
  const tir_reader reader(document);
  EXPECT_EQ(&reader.document(), &document);
}
