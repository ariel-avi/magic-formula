#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <gtest/gtest.h>
#include <magic_formula/error.hpp>
#include <magic_formula/io/text_file.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/io/tir_editor.hpp>
#include <string>
#include <string_view>

namespace {
  using mf::error_code;
  using mf::io::replace_value;
  using mf::io::tir_document;

  /// Text for assertion messages.
  std::string describe(const std::expected<std::string, mf::error> &result) {
    return result.has_value() ? *result : mf::to_string(result.error());
  }

  /// Value of (`section`, `key`) in `text`, or "<absent>" when `text` does not parse or lacks the entry.
  std::string value_in(std::string_view text, std::string_view section, std::string_view key) {
    const auto document = tir_document::parse(text);
    if (!document.has_value()) {
      return "<unparsable>";
    }
    const mf::io::tir_entry *const entry = document->find(section, key);
    return entry == nullptr ? std::string("<absent>") : entry->value;
  }

  /// Expects `result` to be an INVALID_VALUE at `line` for `key`.
  void expect_invalid_value(const std::expected<std::string, mf::error> &result, std::size_t line,
                            std::string_view key) {
    ASSERT_FALSE(result.has_value()) << describe(result);
    EXPECT_EQ(result.error().code, error_code::INVALID_VALUE) << result.error().message;
    EXPECT_EQ(result.error().where.line, line);
    EXPECT_EQ(result.error().where.key, key);
  }

  std::string read_fixture(std::string_view name) {
    const auto text = mf::io::read_text_file(std::filesystem::path{MF_TEST_DATA_DIR} / name);
    EXPECT_TRUE(text.has_value());
    return text.has_value() ? *text : std::string{};
  }
} // namespace

TEST(tir_editor, ReplaceValue_UnquotedEntry_ChangesOnlyTheValueBytes) {
  const auto result = replace_value("[A]\nKEY = 1 $c\nOTHER = 2\n", "A", "KEY", "2.5");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(*result, "[A]\nKEY = 2.5 $c\nOTHER = 2\n");
}

TEST(tir_editor, ReplaceValue_QuotedEntry_WritesQuotedValue) {
  const auto result = replace_value("[A]\nN = 'old'\n", "A", "N", "new name");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(*result, "[A]\nN = 'new name'\n");
}

TEST(tir_editor, ReplaceValue_TrailingComment_IsPreserved) {
  const auto unquoted = replace_value("[A]\nKEY = 1 $ comment one\n", "A", "KEY", "7");
  ASSERT_TRUE(unquoted.has_value()) << describe(unquoted);
  EXPECT_EQ(*unquoted, "[A]\nKEY = 7 $ comment one\n");

  const auto quoted = replace_value("[A]\nN = 'a' $ comment two\n", "A", "N", "b");
  ASSERT_TRUE(quoted.has_value()) << describe(quoted);
  EXPECT_EQ(*quoted, "[A]\nN = 'b' $ comment two\n");
}

TEST(tir_editor, ReplaceValue_CrlfText_KeepsLineEndings) {
  const std::string text = "[A]\r\nKEY = 1\r\nQ = 'a'\r\n";
  const auto result = replace_value(text, "A", "KEY", "22");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(*result, "[A]\r\nKEY = 22\r\nQ = 'a'\r\n");
  EXPECT_EQ(std::ranges::count(*result, '\r'), std::ranges::count(text, '\r'));
  EXPECT_EQ(std::ranges::count(*result, '\n'), std::ranges::count(text, '\n'));
}

TEST(tir_editor, ReplaceValue_LeadingBom_IsPreserved) {
  const auto result = replace_value("\xEF\xBB\xBF[A]\nKEY = 1\n", "A", "KEY", "9");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(*result, "\xEF\xBB\xBF[A]\nKEY = 9\n");
  EXPECT_EQ(value_in(*result, "A", "KEY"), "9");
}

TEST(tir_editor, ReplaceValue_EmptyUnquotedValue_InsertsValue) {
  const auto bare = replace_value("[A]\nKEY =\n", "A", "KEY", "3");
  ASSERT_TRUE(bare.has_value()) << describe(bare);
  EXPECT_EQ(*bare, "[A]\nKEY =3\n");
  EXPECT_EQ(value_in(*bare, "A", "KEY"), "3");

  const auto commented = replace_value("[A]\nKEY = $c\n", "A", "KEY", "3");
  ASSERT_TRUE(commented.has_value()) << describe(commented);
  EXPECT_EQ(*commented, "[A]\nKEY = 3$c\n");
  EXPECT_EQ(value_in(*commented, "A", "KEY"), "3");
}

TEST(tir_editor, ReplaceValue_MixedCaseSectionAndKey_FindsEntry) {
  const auto result = replace_value("[VERTICAL]\nFNOMIN = 4000\n", "vertical", "fNoMiN", "3500");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(*result, "[VERTICAL]\nFNOMIN = 3500\n");
}

TEST(tir_editor, ReplaceValue_SectionScopedMass_EditsOnlyNamedSection) {
  const auto result = replace_value("[UNITS]\nMASS = 'kg'\n[INERTIA]\nMASS = 9.3\n", "INERTIA", "MASS", "10");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(value_in(*result, "INERTIA", "MASS"), "10");
  EXPECT_EQ(value_in(*result, "UNITS", "MASS"), "kg");
}

TEST(tir_editor, ReplaceValue_TableSection_RowsUnchanged) {
  const std::string text = read_fixture("lexical_mix.tir");
  const auto result = replace_value(text, "VERTICAL", "FNOMIN", "3000");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(value_in(*result, "VERTICAL", "FNOMIN"), "3000");

  const auto before = tir_document::parse(text);
  const auto after = tir_document::parse(*result);
  ASSERT_TRUE(before.has_value());
  ASSERT_TRUE(after.has_value());
  ASSERT_EQ(before->tables().size(), after->tables().size());
  ASSERT_EQ(before->tables().size(), 1U);
  EXPECT_EQ(before->tables()[0].section, after->tables()[0].section);
  EXPECT_EQ(before->tables()[0].rows, after->tables()[0].rows);

  const mf::io::tir_entry *const entry = before->find("VERTICAL", "FNOMIN");
  ASSERT_NE(entry, nullptr);
  const std::string_view old_text = text;
  const std::string_view new_text = *result;
  EXPECT_EQ(new_text.substr(0, entry->value_offset), old_text.substr(0, entry->value_offset));
  EXPECT_EQ(new_text.substr(entry->value_offset + 4U), old_text.substr(entry->value_offset + entry->value_length));
}

TEST(tir_editor, ReplaceValue_RestoreOriginalValue_TextByteIdentical) {
  const std::string text = "[A]\nKEY = 1.5 $ note\nQ = 'a b'\n";
  const auto edited = replace_value(text, "A", "KEY", "5");
  ASSERT_TRUE(edited.has_value()) << describe(edited);
  const auto restored = replace_value(*edited, "A", "KEY", "1.5");
  ASSERT_TRUE(restored.has_value()) << describe(restored);
  EXPECT_EQ(*restored, text);
}

TEST(tir_editor, ReplaceValue_UnparsableText_ReturnsParseError) {
  const auto result = replace_value("KEY = 1\n", "A", "KEY", "2");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::SYNTAX);
}

TEST(tir_editor, ReplaceValue_UnknownKey_MissingKey) {
  const auto result = replace_value("[A]\nKEY = 1\n", "A", "nope", "2");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::MISSING_KEY);
  EXPECT_EQ(result.error().where.line, 0U);
  EXPECT_EQ(result.error().where.key, "NOPE");
}

TEST(tir_editor, ReplaceValue_KeyInOtherSection_MissingKey) {
  const auto result = replace_value("[A]\nX = 1\n[B]\nKEY = 1\n", "A", "KEY", "2");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::MISSING_KEY);
  EXPECT_EQ(result.error().where.key, "KEY");
}

TEST(tir_editor, ReplaceValue_LineFeedOrCarriageReturn_InvalidValue) {
  const std::string text = "[A]\nX = 1\nKEY = 1\n";
  expect_invalid_value(replace_value(text, "A", "KEY", "1\n2"), 3, "KEY");
  expect_invalid_value(replace_value(text, "A", "KEY", "1\r"), 3, "KEY");
}

TEST(tir_editor, ReplaceValue_QuoteInsideQuotedValue_InvalidValue) {
  const auto result = replace_value("[A]\nN = 'old'\n", "A", "N", "a'b");
  expect_invalid_value(result, 2, "N");
  ASSERT_FALSE(result.has_value());
  EXPECT_TRUE(result.error().message.contains("not valid .tir text")) << result.error().message;
  EXPECT_TRUE(result.error().message.contains("unexpected text after quoted value")) << result.error().message;
}

TEST(tir_editor, ReplaceValue_NulByte_InvalidValue) {
  expect_invalid_value(replace_value("[A]\nKEY = 1\n", "A", "KEY", std::string_view("1\0", 2)), 2, "KEY");
}

TEST(tir_editor, ReplaceValue_QuotedTextForUnquotedEntry_InvalidValue) {
  expect_invalid_value(replace_value("[A]\nKEY = 1\n", "A", "KEY", "'x'"), 2, "KEY");
}

TEST(tir_editor, ReplaceValue_DollarInUnquotedValue_InvalidValue) {
  expect_invalid_value(replace_value("[A]\nKEY = 1\n", "A", "KEY", "1$"), 2, "KEY");
}

TEST(tir_editor, ReplaceValue_SurroundingBlanks_InvalidValue) {
  expect_invalid_value(replace_value("[A]\nKEY = 1\n", "A", "KEY", " 1 "), 2, "KEY");
}
