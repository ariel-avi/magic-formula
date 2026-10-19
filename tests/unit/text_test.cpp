#include <gtest/gtest.h>
#include <magic_formula/detail/text.hpp>
#include <string>
#include <string_view>
#include <vector>

TEST(text, Trim_AllBlank_ReturnsEmpty) {
  EXPECT_TRUE(mf::detail::trim(" \t\r\v\f").empty());
}

TEST(text, Trim_Empty_ReturnsEmpty) {
  EXPECT_TRUE(mf::detail::trim("").empty());
}

TEST(text, Trim_NoWhitespace_ReturnsInput) {
  EXPECT_EQ(mf::detail::trim("abc"), "abc");
}

TEST(text, Trim_MixedWhitespace_StripsBothEnds) {
  EXPECT_EQ(mf::detail::trim("\t a b \r"), "a b");
}

TEST(text, ToUpperAscii_MixedCase_UppercasesLettersOnly) {
  EXPECT_EQ(mf::detail::to_upper_ascii("aZ_9-"), "AZ_9-");
  EXPECT_EQ(mf::detail::to_upper_ascii("\xC3\xA9"), "\xC3\xA9");
}

TEST(text, ToUpperAscii_BytesAroundLetterRange_Unchanged) {
  // '`' (0x60) is just below 'a', '{' (0x7B) just above 'z'; '@' and '[' bracket 'Z'.
  EXPECT_EQ(mf::detail::to_upper_ascii("`a{z@[Za"), "`A{Z@[ZA");
}

TEST(text, ToUpperAscii_Empty_ReturnsEmpty) {
  EXPECT_TRUE(mf::detail::to_upper_ascii("").empty());
}

TEST(text, IequalsAscii_LengthMismatch_ReturnsFalse) {
  EXPECT_FALSE(mf::detail::iequals_ascii("abc", "abcd"));
}

TEST(text, IequalsAscii_CharacterMismatch_ReturnsFalse) {
  EXPECT_FALSE(mf::detail::iequals_ascii("abc", "abd"));
}

TEST(text, IequalsAscii_EqualIgnoringCase_ReturnsTrue) {
  EXPECT_TRUE(mf::detail::iequals_ascii("Pac2002", "PAC2002"));
}

TEST(text, IequalsAscii_BothEmpty_ReturnsTrue) {
  EXPECT_TRUE(mf::detail::iequals_ascii("", ""));
}

TEST(text, Join_NoItems_ReturnsEmpty) {
  const std::vector<std::string> items;
  EXPECT_TRUE(mf::detail::join(items, ", ").empty());
}

TEST(text, Join_OneItem_ReturnsItem) {
  const std::vector<std::string> items{"A"};
  EXPECT_EQ(mf::detail::join(items, ", "), "A");
}

TEST(text, Join_ThreeItems_InsertsSeparator) {
  const std::vector<std::string> items{"A", "B", "C"};
  EXPECT_EQ(mf::detail::join(items, ", "), "A, B, C");
}
