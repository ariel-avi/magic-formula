#include <cstddef>
#include <gtest/gtest.h>
#include <magic_formula/error.hpp>
#include <optional>
#include <stdexcept>
#include <string>

namespace {
  mf::location make_location(const std::string &file, std::size_t line, const std::string &key) {
    return mf::location{.file = file, .line = line, .key = key};
  }
} // namespace

TEST(error, ToStringLocation_FileAndLine_ReturnsFileColonLine) {
  EXPECT_EQ(mf::to_string(make_location("a.tir", 12, "")), "a.tir:12");
}

TEST(error, ToStringLocation_FileOnly_ReturnsFile) {
  EXPECT_EQ(mf::to_string(make_location("a.tir", 0, "")), "a.tir");
}

TEST(error, ToStringLocation_LineOnly_ReturnsLineN) {
  EXPECT_EQ(mf::to_string(make_location("", 7, "")), "line 7");
}

TEST(error, ToStringLocation_Empty_ReturnsEmpty) {
  EXPECT_EQ(mf::to_string(make_location("", 0, "FZ")), "");
}

TEST(error, ToStringLocation_KeyIsNeverPrinted_ReturnsFileColonLine) {
  EXPECT_EQ(mf::to_string(make_location("a.tir", 3, "PCX1")), "a.tir:3");
}

TEST(error, ToStringError_WithLocation_PrefixesLocation) {
  const mf::error value{.code = mf::error_code::SYNTAX, .message = "bad", .where = make_location("a.tir", 3, "")};
  EXPECT_EQ(mf::to_string(value), "a.tir:3: bad");
}

TEST(error, ToStringError_WithoutLocation_ReturnsMessage) {
  const mf::error value{.code = mf::error_code::SYNTAX, .message = "bad", .where = make_location("", 0, "")};
  EXPECT_EQ(mf::to_string(value), "bad");
}

TEST(error, ToStringWarning_WithLocation_PrefixesLocation) {
  const mf::warning value{.message = "careful", .where = make_location("b.tir", 9, "")};
  EXPECT_EQ(mf::to_string(value), "b.tir:9: careful");
}

TEST(error, ToStringWarning_WithoutLocation_ReturnsMessage) {
  const mf::warning value{.message = "careful", .where = make_location("", 0, "")};
  EXPECT_EQ(mf::to_string(value), "careful");
}

TEST(error, MakeError_AllFields_StoresCodeMessageLocation) {
  const auto result = mf::make_error(mf::error_code::MISSING_KEY, "missing", make_location("c.tir", 4, "FNOMIN"));
  EXPECT_EQ(result.error().code, mf::error_code::MISSING_KEY);
  EXPECT_EQ(result.error().message, "missing");
  EXPECT_EQ(result.error().where.file, "c.tir");
  EXPECT_EQ(result.error().where.line, 4U);
  EXPECT_EQ(result.error().where.key, "FNOMIN");
}

TEST(model_failure, What_WithoutRow_EqualsToStringOfCause) {
  const mf::error cause{
      .code = mf::error_code::INVALID_INPUT, .message = "VX is not finite (nan)", .where = make_location("", 0, "VX")};
  const mf::model_failure failure(cause, std::nullopt);
  EXPECT_EQ(std::string(failure.what()), mf::to_string(cause));
}

TEST(model_failure, What_WithRow_PrefixesRowIndex) {
  const mf::error cause{
      .code = mf::error_code::INVALID_INPUT, .message = "VX is not finite (nan)", .where = make_location("", 0, "VX")};
  const mf::model_failure failure(cause, std::size_t{3});
  EXPECT_EQ(std::string(failure.what()), "row 3: " + mf::to_string(cause));
}

TEST(model_failure, What_WithRowAndLocatedCause_KeepsLocation) {
  const mf::error cause{
      .code = mf::error_code::INVALID_INPUT, .message = "bad value", .where = make_location("a.csv", 7, "FZ")};
  const mf::model_failure failure(cause, std::size_t{0});
  EXPECT_EQ(std::string(failure.what()), "row 0: a.csv:7: bad value");
}

TEST(model_failure, Accessors_ReturnCauseAndRow) {
  const mf::error cause{
      .code = mf::error_code::NUMERICAL, .message = "non-finite FX output (inf)", .where = make_location("", 0, "FX")};
  const mf::model_failure with_row(cause, std::size_t{5});
  EXPECT_EQ(with_row.cause().code, mf::error_code::NUMERICAL);
  EXPECT_EQ(with_row.cause().message, "non-finite FX output (inf)");
  EXPECT_EQ(with_row.cause().where.key, "FX");
  ASSERT_TRUE(with_row.row().has_value());
  EXPECT_EQ(*with_row.row(), 5U);

  const mf::model_failure without_row(cause, std::nullopt);
  EXPECT_FALSE(without_row.row().has_value());
}

TEST(model_failure, Thrown_CaughtAsRuntimeError) {
  const mf::error cause{.code = mf::error_code::INVALID_ARGUMENT, .message = "wrong size", .where = {}};
  try {
    throw mf::model_failure(cause, std::nullopt);
  } catch (const std::runtime_error &caught) {
    EXPECT_EQ(std::string(caught.what()), "wrong size");
    return;
  }
  FAIL() << "model_failure was not caught as std::runtime_error";
}

TEST(error_code, InvalidArgument_DistinctFromNumerical) {
  EXPECT_NE(mf::error_code::INVALID_ARGUMENT, mf::error_code::NUMERICAL);
  const auto result = mf::make_error(mf::error_code::INVALID_ARGUMENT, "size");
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_ARGUMENT);
}

TEST(error, MakeError_DefaultLocation_IsEmpty) {
  const auto result = mf::make_error(mf::error_code::IO, "io");
  EXPECT_EQ(result.error().code, mf::error_code::IO);
  EXPECT_EQ(result.error().message, "io");
  EXPECT_TRUE(result.error().where.file.empty());
  EXPECT_EQ(result.error().where.line, 0U);
  EXPECT_TRUE(result.error().where.key.empty());
}
