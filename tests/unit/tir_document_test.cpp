#include <cstddef>
#include <expected>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <magic_formula/error.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace {
  using mf::error_code;
  using mf::io::tir_document;
  using mf::io::tir_entry;

  constexpr std::string_view FILE_NAME = "t.tir";

  std::expected<tir_document, mf::error> parse(std::string_view text) {
    return tir_document::parse(text, std::string(FILE_NAME));
  }

  /// Text for assertion messages.
  std::string describe(const std::expected<tir_document, mf::error> &result) {
    return result.has_value() ? std::string("parsed") : mf::to_string(result.error());
  }

  /// Expects `text` to fail with `code` at `line` of FILE_NAME, with `message_part` in the message.
  void expect_error(std::string_view text, error_code code, std::size_t line, std::string_view message_part) {
    const auto result = parse(text);
    ASSERT_FALSE(result.has_value()) << text;
    EXPECT_EQ(result.error().code, code) << text;
    EXPECT_EQ(result.error().where.file, FILE_NAME) << text;
    EXPECT_EQ(result.error().where.line, line) << text;
    EXPECT_TRUE(result.error().message.contains(message_part)) << result.error().message;
  }

  void expect_syntax_error(std::string_view text, std::size_t line, std::string_view message_part) {
    expect_error(text, error_code::SYNTAX, line, message_part);
  }

  /// Value of the first entry named `key`, or "<absent>".
  std::string value_of(const tir_document &document, std::string_view key) {
    const tir_entry *const entry = document.find(key);
    return entry == nullptr ? std::string("<absent>") : entry->value;
  }

  /// Fixture that owns one unique per-test directory under the system temp directory and removes it afterwards.
  class read_tir_file : public ::testing::Test {
  protected:
    void SetUp() override {
      const auto *const info = ::testing::UnitTest::GetInstance()->current_test_info();
      std::random_device device;
      const std::string name = std::string("mf_read_tir_file_") + info->name() + "_" + std::to_string(device());
      directory_ = std::filesystem::temp_directory_path() / name;
      std::filesystem::create_directories(directory_);
      path_ = directory_ / "model.tir";
    }

    void TearDown() override {
      std::error_code ignored;
      static_cast<void>(std::filesystem::remove_all(directory_, ignored));
    }

    void write_text(std::string_view content) const {
      std::ofstream output(path_, std::ios::binary | std::ios::trunc);
      output.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    std::filesystem::path directory_;
    std::filesystem::path path_;
  };
} // namespace

TEST(tir_document, Parse_L1_ExponentValueKeptRaw) {
  const auto result = parse("[A]\nX = 2.1e+005\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  ASSERT_EQ(result->entries().size(), 1U);
  EXPECT_EQ(result->entries()[0].value, "2.1e+005");
  EXPECT_FALSE(result->entries()[0].quoted);
}

TEST(tir_document, Parse_L2_TrailingDollarComment_Stripped) {
  const auto result = parse("[A]\nX = 1.0 $ Measurement speed\nY = 'q' $ other\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(value_of(*result, "X"), "1.0");
  EXPECT_EQ(value_of(*result, "Y"), "q");
}

TEST(tir_document, Parse_L3_DollarWithoutBlank_ValueIntact) {
  const auto result = parse("[A]\nX = 1.0$c\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(value_of(*result, "X"), "1.0");
}

TEST(tir_document, Parse_L4_BangHeaderLines_Ignored) {
  const auto result = parse("! header one\n! header two\n[A]\n! inside\nX = 1\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  ASSERT_EQ(result->entries().size(), 1U);
  EXPECT_EQ(result->entries()[0].line, 5U);
}

TEST(tir_document, Parse_L5_CrlfAndNoFinalNewline_EqualsLf) {
  const std::string lf = "[A]\nX = 1\nY = 'q'\n\n[B]\nZ = 3";
  const std::string crlf = "[A]\r\nX = 1\r\nY = 'q'\r\n\r\n[B]\r\nZ = 3\r\n";
  const auto lf_result = parse(lf);
  const auto crlf_result = parse(crlf);
  ASSERT_TRUE(lf_result.has_value()) << describe(lf_result);
  ASSERT_TRUE(crlf_result.has_value()) << describe(crlf_result);
  const std::span<const tir_entry> lf_entries = lf_result->entries();
  const std::span<const tir_entry> crlf_entries = crlf_result->entries();
  ASSERT_EQ(lf_entries.size(), 3U);
  ASSERT_EQ(crlf_entries.size(), lf_entries.size());
  for (std::size_t index = 0; index < lf_entries.size(); ++index) {
    EXPECT_EQ(crlf_entries[index].section, lf_entries[index].section) << index;
    EXPECT_EQ(crlf_entries[index].key, lf_entries[index].key) << index;
    EXPECT_EQ(crlf_entries[index].value, lf_entries[index].value) << index;
    EXPECT_EQ(crlf_entries[index].quoted, lf_entries[index].quoted) << index;
    EXPECT_EQ(crlf_entries[index].line, lf_entries[index].line) << index;
  }
  EXPECT_EQ(lf_entries[2].line, 6U);
  EXPECT_EQ(lf_entries[2].value, "3");
}

TEST(tir_document, Parse_L6_MassInUnitsAndInertia_Accepted) {
  const auto result = parse("[UNITS]\nMASS = 'kg'\n[INERTIA]\nMASS = 9.3\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(result->entries().size(), 2U);
  const tir_entry *const units_mass = result->find("UNITS", "MASS");
  const tir_entry *const inertia_mass = result->find("INERTIA", "MASS");
  ASSERT_NE(units_mass, nullptr);
  ASSERT_NE(inertia_mass, nullptr);
  EXPECT_EQ(units_mass->value, "kg");
  EXPECT_TRUE(units_mass->quoted);
  EXPECT_EQ(inertia_mass->value, "9.3");
  EXPECT_FALSE(inertia_mass->quoted);
  // The key-only lookup returns the first one in file order.
  EXPECT_EQ(value_of(*result, "MASS"), "kg");
}

TEST(tir_document, Parse_L7_LongLines_Accepted) {
  const std::string long_value(300, 'v');
  const std::string text =
      "[A]\n$ " + std::string(300, 'x') + "\nX = '" + long_value + "'\n" + "$ " + std::string(300, 'y');
  const auto result = parse(text);
  ASSERT_TRUE(result.has_value()) << describe(result);
  ASSERT_EQ(result->entries().size(), 1U);
  EXPECT_EQ(result->entries()[0].value, long_value);
  EXPECT_EQ(result->entries()[0].line, 3U);
}

TEST(tir_document, Parse_L8_DuplicateKeySameSection_ReturnsDuplicateKeyNamingBothLines) {
  expect_error("[A]\nX = 1\nY = 2\nX = 3\n", error_code::DUPLICATE_KEY, 4, "first defined at line 2");
  const auto result = parse("[A]\nX = 1\nY = 2\nX = 3\n");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().where.key, "X");
  EXPECT_TRUE(result.error().message.contains("duplicate key X"));
}

TEST(tir_document, Parse_DuplicateKeyAcrossSections_ReturnsDuplicateKey) {
  expect_error("[A]\nX = 1\n[B]\nX = 2\n", error_code::DUPLICATE_KEY, 4, "first defined at line 2");
}

TEST(tir_document, Parse_DuplicateKeyDifferentCase_ReturnsDuplicateKey) {
  expect_error("[A]\nX = 1\nx = 2\n", error_code::DUPLICATE_KEY, 3, "duplicate key X");
}

TEST(tir_document, Parse_MassTwiceInUnits_ReturnsDuplicateKey) {
  expect_error("[UNITS]\nMASS = 'kg'\nMASS = 'g'\n", error_code::DUPLICATE_KEY, 3, "first defined at line 2");
}

TEST(tir_document, Parse_L9_EmptyValue_KeptEmpty) {
  const auto result = parse("[A]\nX =\nY = $ only a comment\nZ = ''\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  ASSERT_EQ(result->entries().size(), 3U);
  EXPECT_EQ(result->entries()[0].value, "");
  EXPECT_FALSE(result->entries()[0].quoted);
  EXPECT_EQ(result->entries()[1].value, "");
  EXPECT_FALSE(result->entries()[1].quoted);
  EXPECT_EQ(result->entries()[2].value, "");
  EXPECT_TRUE(result->entries()[2].quoted);
}

TEST(tir_document, Parse_L10_MixedCase_UpperCasedSectionsAndKeys) {
  const auto result = parse("[Mixed_Case]\nfoo_Bar9 = Some Value\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  ASSERT_EQ(result->entries().size(), 1U);
  EXPECT_EQ(result->entries()[0].section, "MIXED_CASE");
  EXPECT_EQ(result->entries()[0].key, "FOO_BAR9");
  // Values keep their case.
  EXPECT_EQ(result->entries()[0].value, "Some Value");
}

TEST(tir_document, Parse_L11_ShapeTable_RowsKeptRaw) {
  const auto result = parse("[MODEL]\nA = 1\n[SHAPE]\n{radial width}\n 1.0 0.0\n 1.0 0.4 $ trailing\n 0.9 0.9\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(result->entries().size(), 1U);
  ASSERT_EQ(result->tables().size(), 1U);
  const auto &table = result->tables()[0];
  EXPECT_EQ(table.section, "SHAPE");
  EXPECT_EQ(table.line, 3U);
  ASSERT_EQ(table.rows.size(), 3U);
  EXPECT_EQ(table.rows[0], "1.0 0.0");
  EXPECT_EQ(table.rows[1], "1.0 0.4");
  EXPECT_EQ(table.rows[2], "0.9 0.9");
  EXPECT_TRUE(result->warnings().empty());
}

TEST(tir_document, Parse_L12_LineWithoutEquals_ReturnsSyntaxWithLine) {
  expect_syntax_error("[A]\nX = 1\nthis is not an entry\n", 3, "missing '='");
}

TEST(tir_document, Parse_Bom_Skipped) {
  const std::string text = std::string("\xEF\xBB\xBF") + "[A]\nX = 1\n";
  const auto result = parse(text);
  ASSERT_TRUE(result.has_value()) << describe(result);
  ASSERT_EQ(result->entries().size(), 1U);
  EXPECT_EQ(result->entries()[0].section, "A");
  EXPECT_EQ(result->entries()[0].line, 2U);
}

TEST(tir_document, Parse_NulByte_ReturnsSyntaxWithLine) {
  std::string text = "[A]\nX = 1\n";
  text.push_back('\0');
  text += "Y = 2\n";
  expect_syntax_error(text, 3, "NUL byte");
}

TEST(tir_document, Parse_Tabs_TreatedAsBlanks) {
  const auto result = parse("\t[A]\t\n\tX\t=\t1\t\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  ASSERT_EQ(result->entries().size(), 1U);
  EXPECT_EQ(result->entries()[0].key, "X");
  EXPECT_EQ(result->entries()[0].value, "1");
}

TEST(tir_document, Parse_EmptyText_EmptyDocument) {
  const auto result = parse("");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_TRUE(result->entries().empty());
  EXPECT_TRUE(result->tables().empty());
  EXPECT_TRUE(result->warnings().empty());
}

TEST(tir_document, Parse_CommentOnlyText_EmptyDocument) {
  const auto result = parse("$ comment\n! bang\n\n   \n\t\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_TRUE(result->entries().empty());
  EXPECT_TRUE(result->tables().empty());
}

TEST(tir_document, Parse_EmptySections_Accepted) {
  const auto result = parse("[A]\n[B]\nX = 1\n[C]\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  ASSERT_EQ(result->entries().size(), 1U);
  EXPECT_EQ(result->entries()[0].section, "B");
}

TEST(tir_document, Parse_SectionHeaderWithBlanks_Normalised) {
  const auto result = parse("[ units ]\nX = 1\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  ASSERT_EQ(result->entries().size(), 1U);
  EXPECT_EQ(result->entries()[0].section, "UNITS");
}

TEST(tir_document, Parse_HeaderWithJunk_ReturnsSyntax) {
  expect_syntax_error("[X] junk\n", 1, "unexpected text after section header");
}

TEST(tir_document, Parse_HeaderWithBangAfter_ReturnsSyntax) {
  expect_syntax_error("$ c\n[X] ! comment\n", 2, "unexpected text after section header");
}

TEST(tir_document, Parse_HeaderWithDollarComment_Accepted) {
  const auto result = parse("[X] $ comment\nA = 1\n[Y]$no blank\nB = 2\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  ASSERT_EQ(result->entries().size(), 2U);
  EXPECT_EQ(result->entries()[0].section, "X");
  EXPECT_EQ(result->entries()[1].section, "Y");
}

TEST(tir_document, Parse_UnterminatedHeader_ReturnsSyntax) {
  expect_syntax_error("[X\nA = 1\n", 1, "unterminated section header");
}

TEST(tir_document, Parse_EmptyHeader_ReturnsSyntax) {
  expect_syntax_error("[]\n", 1, "empty section name");
  expect_syntax_error("[A]\nX = 1\n[  ]\n", 3, "empty section name");
}

TEST(tir_document, Parse_KeyBeforeFirstSection_ReturnsSyntax) {
  expect_syntax_error("X = 1\n[A]\n", 1, "content before the first section header");
  expect_syntax_error("$ c\n! b\n\nX = 1\n", 4, "content before the first section header");
}

TEST(tir_document, Parse_InvalidKeyCharacters_ReturnsSyntax) {
  expect_syntax_error("[A]\nA-B = 1\n", 2, "invalid key 'A-B'");
  expect_syntax_error("[A]\nA B = 1\n", 2, "invalid key 'A B'");
  expect_syntax_error("[A]\nA~B = 1\n", 2, "invalid key 'A~B'");
  expect_syntax_error("[A]\nA:B = 1\n", 2, "invalid key 'A:B'");
}

TEST(tir_document, Parse_ValidKeyCharacters_Accepted) {
  const auto result = parse("[A]\naZ09_ = 1\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(result->entries()[0].key, "AZ09_");
}

TEST(tir_document, Parse_EmptyKey_ReturnsSyntax) {
  expect_syntax_error("[A]\n= 5\n", 2, "invalid key ''");
}

TEST(tir_document, Parse_QuotedValueWithDollarAndBang_Intact) {
  const auto result = parse("[A]\nX = 'a $ b ! c'\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  ASSERT_EQ(result->entries().size(), 1U);
  EXPECT_EQ(result->entries()[0].value, "a $ b ! c");
  EXPECT_TRUE(result->entries()[0].quoted);
}

TEST(tir_document, Parse_QuotedValue_PreservesInnerBlanks) {
  const auto result = parse("[A]\nX = ' Pac2002 '   \n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(result->entries()[0].value, " Pac2002 ");
}

TEST(tir_document, Parse_UnterminatedQuote_ReturnsSyntax) {
  expect_syntax_error("[A]\nX = 'abc\n", 2, "unterminated quoted string");
  expect_syntax_error("[A]\nX = '\n", 2, "unterminated quoted string");
  const auto result = parse("[A]\nX = 'abc\n");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().where.key, "X");
}

TEST(tir_document, Parse_JunkAfterQuote_ReturnsSyntax) {
  expect_syntax_error("[A]\nX = 'a' b\n", 2, "unexpected text after quoted value");
}

TEST(tir_document, Parse_CommentAfterQuote_Accepted) {
  const auto result = parse("[A]\nX = 'a' $ c\nY = 'b'$c\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(value_of(*result, "X"), "a");
  EXPECT_EQ(value_of(*result, "Y"), "b");
}

TEST(tir_document, Parse_BangInsideUnquotedValue_Kept) {
  const auto result = parse("[A]\nX = 1 ! x\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(value_of(*result, "X"), "1 ! x");
}

TEST(tir_document, Parse_EqualsInsideTable_ReturnsSyntax) {
  expect_syntax_error("[SHAPE]\n{a b}\n1 2\n3 = 4\n", 4, "'=' inside table section [SHAPE]");
}

TEST(tir_document, Parse_EqualsInTableRowComment_Accepted) {
  const auto result = parse("[SHAPE]\n{a b}\n1 2 $ a=b\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  ASSERT_EQ(result->tables().size(), 1U);
  ASSERT_EQ(result->tables()[0].rows.size(), 1U);
  EXPECT_EQ(result->tables()[0].rows[0], "1 2");
}

TEST(tir_document, Parse_TableCommentsAndBlankLines_Skipped) {
  const auto result = parse("[SHAPE]\n{a b}\n$ comment\n\n! bang\n1 2\n   \n3 4\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  ASSERT_EQ(result->tables().size(), 1U);
  ASSERT_EQ(result->tables()[0].rows.size(), 2U);
  EXPECT_EQ(result->tables()[0].rows[0], "1 2");
  EXPECT_EQ(result->tables()[0].rows[1], "3 4");
}

TEST(tir_document, Parse_DuplicateTableSection_WarnsFirstWins) {
  const auto result = parse("[SHAPE]\n{a}\n1 2\n[X]\nK = 1\n[shape]\n{b}\n3 4\n5 6\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  ASSERT_EQ(result->tables().size(), 1U);
  ASSERT_EQ(result->tables()[0].rows.size(), 1U);
  EXPECT_EQ(result->tables()[0].rows[0], "1 2");
  ASSERT_EQ(result->warnings().size(), 1U);
  const mf::warning &duplicate = result->warnings()[0];
  EXPECT_EQ(duplicate.where.file, FILE_NAME);
  EXPECT_EQ(duplicate.where.line, 6U);
  EXPECT_EQ(duplicate.where.key, "SHAPE");
  EXPECT_TRUE(duplicate.message.contains("duplicate table section [SHAPE]")) << duplicate.message;
  EXPECT_TRUE(duplicate.message.contains("first at line 1")) << duplicate.message;
  EXPECT_EQ(result->entries().size(), 1U);
}

TEST(tir_document, Parse_TableHeaderOnly_EmptyRows) {
  const auto result = parse("[SHAPE]\n{radial width}\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  ASSERT_EQ(result->tables().size(), 1U);
  EXPECT_EQ(result->tables()[0].line, 1U);
  EXPECT_TRUE(result->tables()[0].rows.empty());
  EXPECT_TRUE(result->entries().empty());
}

TEST(tir_document, Parse_TwoTableSections_RowsGoToTheirOwnTable) {
  // Three distinct tables (the vector of tables reallocates while rows are still being added), then a duplicate
  // of the first one whose rows must be discarded without touching any stored table.
  const auto result = parse("[SHAPE]\n{a}\n1 2\n3 4\n"
                            "[OTHER]\n{b}\n5 6\n"
                            "[THIRD]\n{c}\n7 8\n9 10\n11 12\n"
                            "[KEYS]\nK = 1\n"
                            "[shape]\n{d}\n13 14\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  const auto tables = result->tables();
  ASSERT_EQ(tables.size(), 3U);
  EXPECT_EQ(tables[0].section, "SHAPE");
  EXPECT_EQ(tables[0].line, 1U);
  ASSERT_EQ(tables[0].rows.size(), 2U);
  EXPECT_EQ(tables[0].rows[0], "1 2");
  EXPECT_EQ(tables[0].rows[1], "3 4");
  EXPECT_EQ(tables[1].section, "OTHER");
  EXPECT_EQ(tables[1].line, 5U);
  ASSERT_EQ(tables[1].rows.size(), 1U);
  EXPECT_EQ(tables[1].rows[0], "5 6");
  EXPECT_EQ(tables[2].section, "THIRD");
  EXPECT_EQ(tables[2].line, 8U);
  ASSERT_EQ(tables[2].rows.size(), 3U);
  EXPECT_EQ(tables[2].rows[0], "7 8");
  EXPECT_EQ(tables[2].rows[1], "9 10");
  EXPECT_EQ(tables[2].rows[2], "11 12");
  EXPECT_EQ(result->warnings().size(), 1U);
  EXPECT_EQ(result->entries().size(), 1U);
}

TEST(tir_document, Parse_NonTableSectionFirstLineWithoutEquals_ReturnsSyntax) {
  expect_syntax_error("[A]\nno equals here\nX = 1\n", 2, "missing '='");
}

TEST(tir_document, Parse_BraceLineInsideKeySection_ReturnsSyntax) {
  expect_syntax_error("[A]\nX = 1\n{a b}\n", 3, "missing '='");
}

TEST(tir_document, Parse_ValueContainingEquals_SplitsAtFirst) {
  const auto result = parse("[A]\nA = b=c\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(value_of(*result, "A"), "b=c");
}

TEST(tir_document, Parse_FileName_UsedInErrorLocation) {
  const auto named = tir_document::parse("[A]\nbad\n", "my.tir");
  ASSERT_FALSE(named.has_value());
  EXPECT_EQ(named.error().where.file, "my.tir");
  EXPECT_EQ(named.error().where.line, 2U);
  EXPECT_EQ(mf::to_string(named.error()), "my.tir:2: " + named.error().message);

  const auto anonymous = tir_document::parse("[A]\nbad\n");
  ASSERT_FALSE(anonymous.has_value());
  EXPECT_TRUE(anonymous.error().where.file.empty());
  EXPECT_EQ(anonymous.error().where.line, 2U);

  const auto ok = tir_document::parse("[A]\nX = 1\n", "ok.tir");
  ASSERT_TRUE(ok.has_value());
  EXPECT_EQ(ok->file_name(), "ok.tir");
}

TEST(tir_document, Find_Hit_Miss_CaseInsensitive) {
  const auto result = parse("[A]\nFnomin = 4000\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  const tir_entry *const upper = result->find("FNOMIN");
  const tir_entry *const lower = result->find("fnomin");
  ASSERT_NE(upper, nullptr);
  EXPECT_EQ(upper, lower);
  EXPECT_EQ(upper, result->entries().data());
  EXPECT_EQ(result->find("FNOMIN2"), nullptr);
  EXPECT_EQ(result->find("FNOMI"), nullptr);
}

TEST(tir_document, FindSection_HitAndMisses) {
  const auto result = parse("[Sec]\nKey = 1\n");
  ASSERT_TRUE(result.has_value()) << describe(result);
  const tir_entry *const hit = result->find("sec", "KEY");
  ASSERT_NE(hit, nullptr);
  EXPECT_EQ(hit, result->entries().data());
  EXPECT_EQ(result->find("OTHER", "KEY"), nullptr);
  EXPECT_EQ(result->find("SEC", "OTHER"), nullptr);
}

TEST_F(read_tir_file, ReadTirFile_LexicalMix_Succeeds) {
  const std::filesystem::path path = std::filesystem::path{MF_TEST_DATA_DIR} / "lexical_mix.tir";
  const auto result = mf::io::read_tir_file(path);
  ASSERT_TRUE(result.has_value()) << describe(result);
  EXPECT_EQ(result->file_name(), path.string());
  EXPECT_EQ(value_of(*result, "FNOMIN"), "4.0e+003");
  const tir_entry *const tyreside = result->find("TYRESIDE");
  ASSERT_NE(tyreside, nullptr);
  EXPECT_EQ(tyreside->value, "LEFT");
  EXPECT_FALSE(tyreside->quoted);
  EXPECT_EQ(value_of(*result, "PDX1"), "1.0");
  EXPECT_EQ(value_of(*result, "LONGVL"), "16.6");
  EXPECT_EQ(value_of(*result, "PCY1"), "1.3");
  EXPECT_EQ(value_of(*result, "TIRE_DESCRIPTION"), "synthetic $ not a comment ! not a comment");
  ASSERT_EQ(result->tables().size(), 1U);
  EXPECT_EQ(result->tables()[0].section, "SHAPE");
  EXPECT_EQ(result->tables()[0].rows.size(), 3U);
  const tir_entry *const units_mass = result->find("UNITS", "MASS");
  const tir_entry *const inertia_mass = result->find("INERTIA", "MASS");
  ASSERT_NE(units_mass, nullptr);
  ASSERT_NE(inertia_mass, nullptr);
  EXPECT_EQ(units_mass->value, "kg");
  EXPECT_EQ(inertia_mass->value, "9.3");
  EXPECT_TRUE(result->warnings().empty());
}

TEST_F(read_tir_file, ReadTirFile_Nonexistent_ReturnsIo) {
  const auto result = mf::io::read_tir_file(path_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::IO);
  EXPECT_EQ(result.error().where.file, path_.string());
}

TEST_F(read_tir_file, ReadTirFile_SyntaxError_CarriesFileName) {
  write_text("[A]\nX = 1\nbroken line\n");
  const auto result = mf::io::read_tir_file(path_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, error_code::SYNTAX);
  EXPECT_EQ(result.error().where.file, path_.string());
  EXPECT_EQ(result.error().where.line, 3U);
}
