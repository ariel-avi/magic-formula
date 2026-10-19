#include <cstddef>
#include <gtest/gtest.h>
#include <magic_formula/error.hpp>
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

TEST(error, MakeError_DefaultLocation_IsEmpty) {
  const auto result = mf::make_error(mf::error_code::IO, "io");
  EXPECT_EQ(result.error().code, mf::error_code::IO);
  EXPECT_EQ(result.error().message, "io");
  EXPECT_TRUE(result.error().where.file.empty());
  EXPECT_EQ(result.error().where.line, 0U);
  EXPECT_TRUE(result.error().where.key.empty());
}
