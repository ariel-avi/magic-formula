#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <limits>
#include <magic_formula/error.hpp>
#include <magic_formula/io/input_table.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/model_layout.hpp>
#include <numbers>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {
  /// Degrees to radians, written the way the specification states it (value * (pi / 180)).
  constexpr double DEG_TO_RAD = std::numbers::pi / 180.0;

  /// The five required columns in canonical order.
  constexpr std::array<std::string_view, 5> REQUIRED_COLUMNS{"FZ", "SA", "SR", "IA", "VX"};

  std::uint64_t bits(double value) {
    return std::bit_cast<std::uint64_t>(value);
  }

  /// Bit-exact comparison (identity vectors: parsed text must come back unchanged, or scaled exactly once).
  void expect_same(double actual, double expected) {
    EXPECT_EQ(bits(actual), bits(expected)) << actual << " vs " << expected;
  }

  /// Parses `text` and expects success; returns an empty table on failure (the expectation has then failed).
  mf::io::input_table parse_success(std::string_view text) {
    const auto result = mf::io::parse_input_table(text, "table.csv");
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? std::string{} : mf::to_string(result.error()));
    return result.has_value() ? *result : mf::io::input_table{};
  }

  /// Parses `text` and expects failure; returns a default error on success (the expectation has then failed).
  mf::error parse_failure(std::string_view text, const std::string &file = "table.csv") {
    const auto result = mf::io::parse_input_table(text, file);
    EXPECT_FALSE(result.has_value()) << text;
    return result.has_value() ? mf::error{} : result.error();
  }

  /// Index of the column `name` in `table`, or -1 when absent (the expectation has then failed).
  Eigen::Index column_of(const mf::io::input_table &table, std::string_view name) {
    const auto found = std::ranges::find(table.columns, name);
    EXPECT_NE(found, table.columns.end()) << name;
    return found == table.columns.end() ? -1 : static_cast<Eigen::Index>(found - table.columns.begin());
  }

  /// Value of row `r` in the column `name` (NaN when the column is absent).
  double value_at(const mf::io::input_table &table, Eigen::Index r, std::string_view name) {
    const Eigen::Index c = column_of(table, name);
    return c < 0 ? std::numeric_limits<double>::quiet_NaN() : table.values(r, c);
  }

  /// Expects the five SI fields of row `r` (bit-exact), looked up by column name.
  void expect_point(const mf::io::input_table &table, Eigen::Index r, double fz, double alpha, double kappa,
                    double gamma, double vx) {
    expect_same(value_at(table, r, "FZ"), fz);
    expect_same(value_at(table, r, "SA"), alpha);
    expect_same(value_at(table, r, "SR"), kappa);
    expect_same(value_at(table, r, "IA"), gamma);
    expect_same(value_at(table, r, "VX"), vx);
  }

  /// One unit-table entry: a header, a row and the SI value that the named column must have.
  struct unit_case {
    std::string_view header;
    std::string_view row;
    std::string_view column;
    double expected;
  };

  const std::array<unit_case, 13> UNIT_CASES{
      unit_case{.header = "FZ[N] SA SR IA VX", .row = "4000 0 0 0 0", .column = "FZ", .expected = 4000.0},
      unit_case{.header = "fz[n] SA SR IA VX", .row = "3500.5 0 0 0 0", .column = "FZ", .expected = 3500.5},
      unit_case{.header = "FZ SA[rad] SR IA VX", .row = "0 0.25 0 0 0", .column = "SA", .expected = 0.25},
      unit_case{.header = "FZ SA[deg] SR IA VX", .row = "0 30 0 0 0", .column = "SA", .expected = 30.0 * DEG_TO_RAD},
      unit_case{.header = "FZ SA SR[-] IA VX", .row = "0 0 0.1 0 0", .column = "SR", .expected = 0.1},
      unit_case{.header = "FZ SA SR IA[rad] VX", .row = "0 0 0 0.05 0", .column = "IA", .expected = 0.05},
      unit_case{.header = "FZ SA SR IA[DEG] VX", .row = "0 0 0 -4 0", .column = "IA", .expected = -4.0 * DEG_TO_RAD},
      unit_case{.header = "FZ SA SR IA VX[m/s]", .row = "0 0 0 0 12.5", .column = "VX", .expected = 12.5},
      unit_case{.header = "FZ SA SR IA VX[M/S]", .row = "0 0 0 0 7", .column = "VX", .expected = 7.0},
      unit_case{.header = "FZ SA SR IA VX P[Pa]", .row = "0 0 0 0 0 220000", .column = "P", .expected = 220000.0},
      unit_case{.header = "FZ SA SR IA VX p[PA]", .row = "0 0 0 0 0 180000.5", .column = "P", .expected = 180000.5},
      unit_case{.header = "FZ SA SR IA VX", .row = "4000 0.1 0.2 0.3 7", .column = "SA", .expected = 0.1},
      unit_case{.header = "FZ SA SR IA VX", .row = "4000 0.1 0.2 0.3 7", .column = "IA", .expected = 0.3},
  };

  /// A channel span that names a column the parser never produces ("Q").
  const std::array<mf::input_channel, 2> UNKNOWN_CHANNELS{
      {{.name = "FZ", .unit = "N"}, {.name = "Q", .unit = "-"}},
  };

  class input_table_units : public ::testing::TestWithParam<unit_case> {};

  class input_table_missing_column : public ::testing::TestWithParam<std::string_view> {};

  /// Header tokens with a malformed unit bracket.
  constexpr std::array<std::string_view, 5> MALFORMED_TOKENS{"FZ[", "FZ[]", "FZ[N]x", "FZ[N]]", "FZ[N"};

  class input_table_malformed_unit : public ::testing::TestWithParam<std::string_view> {};

  /// Fixture that owns one unique per-test directory under the system temp directory and removes it afterwards.
  /// `path_` names a file inside it that does not exist until a test writes it.
  class input_table_file : public ::testing::Test {
  protected:
    void SetUp() override {
      const auto *const info = ::testing::UnitTest::GetInstance()->current_test_info();
      std::random_device device;
      const std::string name = std::string("mf_read_input_table_") + info->name() + "_" + std::to_string(device());
      directory_ = std::filesystem::temp_directory_path() / name;
      std::filesystem::create_directories(directory_);
      path_ = directory_ / "inputs.csv";
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

TEST(input_table, Parse_CommentsBlanksCrlf_Ignored) {
  const auto table = parse_success("# comment\r\n"
                                   "\r\n"
                                   "   \r\n"
                                   "FZ[N],SA[rad],SR[-],IA[rad],VX[m/s]\r\n"
                                   "# between rows\r\n"
                                   "4000,0.05,0.1,0,10\r\n"
                                   "\r\n"
                                   "  # indented comment\r\n"
                                   "3000,0,0,0,5\r\n");
  ASSERT_EQ(table.values.rows(), 2);
  EXPECT_EQ(table.columns.size(), 5U);
  EXPECT_EQ(table.header_line, 4U);
  ASSERT_EQ(table.lines.size(), 2U);
  EXPECT_EQ(table.lines[0], 6U);
  EXPECT_EQ(table.lines[1], 9U);
  expect_point(table, 0, 4000.0, 0.05, 0.1, 0.0, 10.0);
  expect_point(table, 1, 3000.0, 0.0, 0.0, 0.0, 5.0);
}

TEST(input_table, Parse_CommaSeparated_Rows) {
  const auto table = parse_success("FZ[N],SA[rad],SR[-],IA[rad],VX[m/s]\n"
                                   "1,2,3,4,5\n"
                                   "6.5,-7,8e-1,0.25,+9\n");
  ASSERT_EQ(table.values.rows(), 2);
  EXPECT_EQ(table.file, "table.csv");
  const std::vector<std::string> expected_columns{"FZ", "SA", "SR", "IA", "VX"};
  EXPECT_EQ(table.columns, expected_columns);
  expect_point(table, 0, 1.0, 2.0, 3.0, 4.0, 5.0);
  expect_point(table, 1, 6.5, -7.0, 0.8, 0.25, 9.0);
  ASSERT_EQ(table.lines.size(), 2U);
  EXPECT_EQ(table.lines[0], 2U);
  EXPECT_EQ(table.lines[1], 3U);
  EXPECT_EQ(table.header_line, 1U);
}

TEST(input_table, Parse_BlankSeparated_Rows) {
  const auto table = parse_success("FZ SA SR IA VX\n"
                                   "1 2 3 4 5\n"
                                   "  6   7  8 9    10  \n");
  ASSERT_EQ(table.values.rows(), 2);
  expect_point(table, 0, 1.0, 2.0, 3.0, 4.0, 5.0);
  expect_point(table, 1, 6.0, 7.0, 8.0, 9.0, 10.0);
}

TEST(input_table, Parse_TabSeparated_Rows) {
  const auto table = parse_success("FZ[N]\tSA[rad]\tSR[-]\tIA[rad]\tVX[m/s]\n"
                                   "1\t2\t3\t4\t5\n");
  ASSERT_EQ(table.values.rows(), 1);
  expect_point(table, 0, 1.0, 2.0, 3.0, 4.0, 5.0);
}

TEST(input_table, Parse_MixedSeparators_Rows) {
  const auto table = parse_success("FZ, SA ,\tSR,,IA VX,\n"
                                   "4000 ,, 0.05\t0.1,0 ,10,\n"
                                   "1,,2,3,,,4,5\n"
                                   ",,7 8 9 10 11,,\n");
  ASSERT_EQ(table.values.rows(), 3);
  expect_point(table, 0, 4000.0, 0.05, 0.1, 0.0, 10.0);
  // "1,,2" is two fields, not three.
  expect_point(table, 1, 1.0, 2.0, 3.0, 4.0, 5.0);
  // Leading and trailing separators produce no fields.
  expect_point(table, 2, 7.0, 8.0, 9.0, 10.0, 11.0);
}

TEST(input_table, Parse_HeaderCaseAndOrder_Mapped) {
  const auto table = parse_success("vx Ia sr SA fz\n"
                                   "5 4 3 2 1\n");
  ASSERT_EQ(table.values.rows(), 1);
  // Columns keep the header order and carry the canonical upper-case names.
  const std::vector<std::string> expected_columns{"VX", "IA", "SR", "SA", "FZ"};
  EXPECT_EQ(table.columns, expected_columns);
  expect_point(table, 0, 1.0, 2.0, 3.0, 4.0, 5.0);
}

TEST(input_table, Parse_NumberForms_Accepted) {
  const auto table = parse_success("FZ SA SR IA VX\n"
                                   "1e3 -0.0 +.5 2.5E-1 -7\n");
  ASSERT_EQ(table.values.rows(), 1);
  // Negative speed and signed zero are parsed as given; rejecting them is the model's job.
  expect_point(table, 0, 1000.0, -0.0, 0.5, 0.25, -7.0);
}

TEST_P(input_table_units, Parse_Unit_AppliesFactor) {
  const unit_case &param = GetParam();
  const std::string text = std::string(param.header) + "\n" + std::string(param.row) + "\n";
  const auto table = parse_success(text);
  ASSERT_EQ(table.values.rows(), 1) << text;
  // Bit-exact: degrees are converted as value * (pi / 180), every other unit is multiplied by exactly 1.
  expect_same(value_at(table, 0, param.column), param.expected);
}

INSTANTIATE_TEST_SUITE_P(cases, input_table_units, ::testing::ValuesIn(UNIT_CASES));

TEST(input_table, Parse_EmptyText_ReturnsSyntax) {
  const mf::error failure = parse_failure("");
  EXPECT_EQ(failure.code, mf::error_code::SYNTAX);
  EXPECT_TRUE(failure.message.contains("missing header line")) << failure.message;
  EXPECT_EQ(failure.where.file, "table.csv");
  EXPECT_EQ(failure.where.line, 0U);
}

TEST(input_table, Parse_OnlyComments_ReturnsSyntax) {
  const mf::error failure = parse_failure("# first\n\n   \r\n\t# second\n");
  EXPECT_EQ(failure.code, mf::error_code::SYNTAX);
  EXPECT_TRUE(failure.message.contains("missing header line")) << failure.message;
}

TEST(input_table, Parse_UnknownColumn_ReturnsSyntax) {
  const mf::error failure = parse_failure("# header comes late\n"
                                          "\n"
                                          "FZ SA SR IA VX foo\n");
  EXPECT_EQ(failure.code, mf::error_code::SYNTAX);
  EXPECT_EQ(failure.where.line, 3U);
  EXPECT_EQ(failure.where.key, "FOO");
  EXPECT_TRUE(failure.message.contains("unknown column foo (expected FZ, SA, SR, IA, VX, P)")) << failure.message;
}

TEST(input_table, Parse_UnknownColumnWithUnit_ReturnsSyntax) {
  const mf::error failure = parse_failure("FZ SA SR IA VX alpha[deg]\n");
  EXPECT_EQ(failure.code, mf::error_code::SYNTAX);
  EXPECT_EQ(failure.where.key, "ALPHA");
  EXPECT_TRUE(failure.message.contains("unknown column alpha[deg]")) << failure.message;
}

TEST_P(input_table_malformed_unit, Parse_MalformedUnit_ReturnsSyntax) {
  const std::string_view token = GetParam();
  const mf::error failure = parse_failure(std::string(token) + " SA SR IA VX\n");
  EXPECT_EQ(failure.code, mf::error_code::SYNTAX) << token;
  EXPECT_EQ(failure.where.line, 1U) << token;
  EXPECT_EQ(failure.where.key, "FZ") << token;
  EXPECT_TRUE(failure.message.contains("malformed unit")) << failure.message;
  EXPECT_TRUE(failure.message.contains(token)) << failure.message;
}

INSTANTIATE_TEST_SUITE_P(cases, input_table_malformed_unit, ::testing::ValuesIn(MALFORMED_TOKENS));

TEST(input_table, Parse_DuplicateColumn_ReturnsDuplicateKey) {
  const mf::error same_unit = parse_failure("FZ SA SR IA VX fz\n");
  EXPECT_EQ(same_unit.code, mf::error_code::DUPLICATE_KEY);
  EXPECT_EQ(same_unit.where.line, 1U);
  EXPECT_EQ(same_unit.where.key, "FZ");

  const mf::error other_unit = parse_failure("FZ SA[deg] SR IA VX SA[rad]\n");
  EXPECT_EQ(other_unit.code, mf::error_code::DUPLICATE_KEY);
  EXPECT_EQ(other_unit.where.key, "SA");
  EXPECT_TRUE(other_unit.message.contains("duplicate column SA")) << other_unit.message;

  const mf::error pressure = parse_failure("FZ SA SR IA VX P P[Pa]\n");
  EXPECT_EQ(pressure.code, mf::error_code::DUPLICATE_KEY);
  EXPECT_EQ(pressure.where.key, "P");
}

TEST_P(input_table_missing_column, Parse_MissingColumn_ReturnsMissingKey) {
  const std::string_view missing = GetParam();
  std::string header = "P";
  for (const std::string_view name : REQUIRED_COLUMNS) {
    if (name != missing) {
      header += ' ';
      header += name;
    }
  }
  const mf::error failure = parse_failure("# comment\n" + header + "\n");
  EXPECT_EQ(failure.code, mf::error_code::MISSING_KEY) << header;
  EXPECT_EQ(failure.where.key, missing) << header;
  EXPECT_EQ(failure.where.line, 2U);
  EXPECT_TRUE(failure.message.contains(missing)) << failure.message;
}

INSTANTIATE_TEST_SUITE_P(cases, input_table_missing_column, ::testing::ValuesIn(REQUIRED_COLUMNS));

TEST(input_table, Parse_EmptyHeaderFields_ReturnsMissingKey) {
  const mf::error failure = parse_failure(",,,\n");
  EXPECT_EQ(failure.code, mf::error_code::MISSING_KEY);
  EXPECT_EQ(failure.where.key, "FZ");
  EXPECT_EQ(failure.where.line, 1U);
}

TEST(input_table, Parse_UnsupportedUnit_ReturnsUnsupportedUnit) {
  const mf::error angle = parse_failure("FZ SA[grad] SR IA VX\n");
  EXPECT_EQ(angle.code, mf::error_code::UNSUPPORTED_UNIT);
  EXPECT_EQ(angle.where.line, 1U);
  EXPECT_EQ(angle.where.key, "SA");
  EXPECT_TRUE(angle.message.contains("'grad'")) << angle.message;
  EXPECT_TRUE(angle.message.contains("column SA")) << angle.message;
  EXPECT_TRUE(angle.message.contains("supported: rad, deg")) << angle.message;

  const mf::error force = parse_failure("FZ[kN] SA SR IA VX\n");
  EXPECT_EQ(force.code, mf::error_code::UNSUPPORTED_UNIT);
  EXPECT_EQ(force.where.key, "FZ");
  EXPECT_TRUE(force.message.contains("supported: N")) << force.message;

  const mf::error speed = parse_failure("FZ SA SR IA VX[km/h]\n");
  EXPECT_EQ(speed.code, mf::error_code::UNSUPPORTED_UNIT);
  EXPECT_EQ(speed.where.key, "VX");

  // A unit of another column is not accepted: degrees mean nothing for a force.
  const mf::error wrong_column = parse_failure("FZ[deg] SA SR IA VX\n");
  EXPECT_EQ(wrong_column.code, mf::error_code::UNSUPPORTED_UNIT);
  EXPECT_EQ(wrong_column.where.key, "FZ");
}

TEST(input_table, Parse_FieldCountMismatch_ReturnsSyntaxWithLine) {
  const mf::error too_few = parse_failure("FZ SA SR IA VX\n"
                                          "1 2 3 4 5\n"
                                          "1 2 3 4\n");
  EXPECT_EQ(too_few.code, mf::error_code::SYNTAX);
  EXPECT_EQ(too_few.where.line, 3U);
  EXPECT_TRUE(too_few.message.contains("expected 5 fields, got 4")) << too_few.message;

  const mf::error too_many = parse_failure("FZ SA SR IA VX\n"
                                           "1 2 3 4 5 6\n");
  EXPECT_EQ(too_many.code, mf::error_code::SYNTAX);
  EXPECT_EQ(too_many.where.line, 2U);
  EXPECT_TRUE(too_many.message.contains("expected 5 fields, got 6")) << too_many.message;

  const mf::error no_fields = parse_failure("FZ SA SR IA VX P\n"
                                            ",,\n");
  EXPECT_EQ(no_fields.code, mf::error_code::SYNTAX);
  EXPECT_EQ(no_fields.where.line, 2U);
  EXPECT_TRUE(no_fields.message.contains("expected 6 fields, got 0")) << no_fields.message;
}

TEST(input_table, Parse_BadNumber_ReturnsInvalidNumberWithLineAndColumn) {
  const mf::error text = parse_failure("FZ SA SR IA VX\n"
                                       "1 2 3 4 5\n"
                                       "1 2 abc 4 5\n");
  EXPECT_EQ(text.code, mf::error_code::INVALID_NUMBER);
  EXPECT_EQ(text.where.line, 3U);
  EXPECT_EQ(text.where.key, "SR");
  EXPECT_TRUE(text.message.contains("'abc'")) << text.message;
  EXPECT_TRUE(text.message.contains("column SR")) << text.message;

  const mf::error infinite = parse_failure("FZ SA SR IA VX\n"
                                           "1 2 3 4 inf\n");
  EXPECT_EQ(infinite.code, mf::error_code::INVALID_NUMBER);
  EXPECT_EQ(infinite.where.line, 2U);
  EXPECT_EQ(infinite.where.key, "VX");
  EXPECT_TRUE(infinite.message.contains("'inf'")) << infinite.message;

  const mf::error not_a_number = parse_failure("VX FZ SA SR IA\n"
                                               "nan 2 3 4 5\n");
  EXPECT_EQ(not_a_number.code, mf::error_code::INVALID_NUMBER);
  EXPECT_EQ(not_a_number.where.key, "VX");

  const mf::error overflow = parse_failure("FZ SA SR IA VX\n"
                                           "1e999 2 3 4 5\n");
  EXPECT_EQ(overflow.code, mf::error_code::INVALID_NUMBER);
  EXPECT_EQ(overflow.where.key, "FZ");
}

TEST(input_table, Parse_BadPressureNumber_ReturnsInvalidNumberForP) {
  const mf::error failure = parse_failure("FZ SA SR IA VX P\n"
                                          "1 2 3 4 5 six\n");
  EXPECT_EQ(failure.code, mf::error_code::INVALID_NUMBER);
  EXPECT_EQ(failure.where.key, "P");
  EXPECT_EQ(failure.where.line, 2U);
}

TEST(input_table, Parse_PressureColumn_SetsHasPressureAndValue) {
  const auto table = parse_success("FZ[N] SA[rad] SR[-] IA[rad] VX[m/s] P[Pa]\n"
                                   "1 2 3 4 5 6\n");
  const std::vector<std::string> expected_columns{"FZ", "SA", "SR", "IA", "VX", "P"};
  EXPECT_EQ(table.columns, expected_columns);
  ASSERT_EQ(table.values.rows(), 1);
  ASSERT_EQ(table.values.cols(), 6);
  expect_same(value_at(table, 0, "P"), 6.0);
  expect_point(table, 0, 1.0, 2.0, 3.0, 4.0, 5.0);
}

TEST(input_table, Parse_PressureColumnFirst_ValuesMapped) {
  const auto table = parse_success("P[Pa] FZ SA SR IA VX\n"
                                   "220000 1 2 3 4 5\n"
                                   "0 6 7 8 9 10\n");
  ASSERT_EQ(table.values.rows(), 2);
  ASSERT_FALSE(table.columns.empty());
  EXPECT_EQ(table.columns.front(), "P");
  expect_same(value_at(table, 0, "P"), 220000.0);
  expect_point(table, 0, 1.0, 2.0, 3.0, 4.0, 5.0);
  expect_same(value_at(table, 1, "P"), 0.0);
  expect_point(table, 1, 6.0, 7.0, 8.0, 9.0, 10.0);
}

TEST(input_table, Parse_NoPressureColumn_PressureEmpty) {
  const auto table = parse_success("FZ SA SR IA VX\n"
                                   "1 2 3 4 5\n");
  EXPECT_EQ(table.columns.size(), 5U);
  EXPECT_EQ(std::ranges::count(table.columns, "P"), 0);
  ASSERT_EQ(table.values.rows(), 1);
  EXPECT_EQ(table.values.cols(), 5);
}

TEST(input_table, Parse_HeaderOnly_ZeroRows) {
  const auto with_newline = parse_success("FZ[N],SA[rad],SR[-],IA[rad],VX[m/s]\n");
  EXPECT_EQ(with_newline.values.rows(), 0);
  EXPECT_EQ(with_newline.values.cols(), 5);
  EXPECT_TRUE(with_newline.lines.empty());
  EXPECT_EQ(with_newline.header_line, 1U);

  const auto without_newline = parse_success("FZ SA SR IA VX P");
  EXPECT_EQ(without_newline.values.rows(), 0);
  EXPECT_EQ(without_newline.values.cols(), 6);
  EXPECT_TRUE(without_newline.lines.empty());
  EXPECT_EQ(without_newline.columns.size(), 6U);
}

TEST(input_table, Parse_NoFinalNewline_LastLineParsed) {
  const auto table = parse_success("FZ SA SR IA VX\n"
                                   "1 2 3 4 5\n"
                                   "6 7 8 9 10");
  ASSERT_EQ(table.values.rows(), 2);
  ASSERT_EQ(table.lines.size(), 2U);
  EXPECT_EQ(table.lines[1], 3U);
  expect_point(table, 1, 6.0, 7.0, 8.0, 9.0, 10.0);
}

TEST(input_table, Parse_Utf8Bom_Skipped) {
  const auto table = parse_success("\xEF\xBB\xBF"
                                   "FZ[N],SA[rad],SR[-],IA[rad],VX[m/s]\n"
                                   "4000,0.1,0.2,0.3,10\n");
  ASSERT_EQ(table.values.rows(), 1);
  ASSERT_EQ(table.lines.size(), 1U);
  EXPECT_EQ(table.lines[0], 2U);
  expect_point(table, 0, 4000.0, 0.1, 0.2, 0.3, 10.0);
}

TEST(input_table, Parse_FileName_InErrorLocation) {
  const mf::error named = parse_failure("FZ SA SR IA VX\n1 2 x 4 5\n", "inputs.csv");
  EXPECT_EQ(named.where.file, "inputs.csv");
  EXPECT_EQ(named.where.line, 2U);
  EXPECT_EQ(mf::to_string(named).rfind("inputs.csv:2: ", 0), 0U) << mf::to_string(named);

  const mf::error unnamed = parse_failure("FZ SA SR IA VX\n1 2 x 4 5\n", "");
  EXPECT_TRUE(unnamed.where.file.empty());

  const auto default_name = mf::io::parse_input_table("");
  ASSERT_FALSE(default_name.has_value());
  EXPECT_TRUE(default_name.error().where.file.empty());

  const mf::error missing_header = parse_failure("", "inputs.csv");
  EXPECT_EQ(missing_header.where.file, "inputs.csv");
}

TEST_F(input_table_file, ReadInputTable_Nonexistent_ReturnsIo) {
  const auto result = mf::io::read_input_table(path_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::IO);
  EXPECT_EQ(result.error().where.file, path_.string());
}

TEST_F(input_table_file, ReadInputTable_File_Succeeds) {
  write_text("# synthetic\nFZ[N],SA[deg],SR[-],IA[rad],VX[m/s]\n4000,30,0.1,0,10\n");
  const auto result = mf::io::read_input_table(path_);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->values.rows(), 1);
  ASSERT_EQ(result->lines.size(), 1U);
  EXPECT_EQ(result->lines[0], 3U);
  EXPECT_EQ(result->file, path_.string());
  expect_point(*result, 0, 4000.0, 30.0 * DEG_TO_RAD, 0.1, 0.0, 10.0);
}

TEST(map_inputs, ReorderedHeader_ColumnsInChannelOrder) {
  const auto table = parse_success("VX IA SR SA FZ\n"
                                   "5 4 3 2 1\n"
                                   "10 9 8 7 6\n");
  const auto mapped = mf::io::map_inputs(table, mf::mf5x::PAC2002_INPUTS);
  ASSERT_TRUE(mapped.has_value());
  ASSERT_EQ(mapped->values.rows(), 2);
  ASSERT_EQ(mapped->values.cols(), 5);
  // Channel order: FZ, SA, SR, IA, VX.
  for (Eigen::Index c = 0; c < 5; ++c) {
    expect_same(mapped->values(0, c), static_cast<double>(c + 1));
    expect_same(mapped->values(1, c), static_cast<double>(c + 6));
  }
}

TEST(map_inputs, PressureColumn_ListedAsIgnored) {
  const auto table = parse_success("FZ SA SR IA VX P\n"
                                   "1 2 3 4 5 6\n");
  const auto mapped = mf::io::map_inputs(table, mf::mf5x::PAC2002_INPUTS);
  ASSERT_TRUE(mapped.has_value());
  const std::vector<std::string> expected_ignored{"P"};
  EXPECT_EQ(mapped->ignored_columns, expected_ignored);
  ASSERT_EQ(mapped->values.cols(), 5);
  expect_same(mapped->values(0, mf::mf5x::VX), 5.0);
}

TEST(map_inputs, NoExtraColumns_IgnoredEmpty) {
  const auto table = parse_success("FZ SA SR IA VX\n"
                                   "1 2 3 4 5\n");
  const auto mapped = mf::io::map_inputs(table, mf::mf5x::PAC2002_INPUTS);
  ASSERT_TRUE(mapped.has_value());
  EXPECT_TRUE(mapped->ignored_columns.empty());
}

TEST(map_inputs, MissingChannel_MissingKeyAtHeaderLine) {
  const auto table = parse_success("# comment\n"
                                   "FZ SA SR IA VX\n"
                                   "1 2 3 4 5\n");
  const auto mapped = mf::io::map_inputs(table, UNKNOWN_CHANNELS);
  ASSERT_FALSE(mapped.has_value());
  EXPECT_EQ(mapped.error().code, mf::error_code::MISSING_KEY);
  EXPECT_EQ(mapped.error().where.file, table.file);
  EXPECT_EQ(mapped.error().where.line, table.header_line);
  EXPECT_EQ(mapped.error().where.line, 2U);
  EXPECT_EQ(mapped.error().where.key, "Q");
  EXPECT_TRUE(mapped.error().message.contains("missing required column Q")) << mapped.error().message;
}

TEST(map_inputs, HeaderOnly_ZeroRowsMatrix) {
  const auto table = parse_success("FZ SA SR IA VX\n");
  const auto mapped = mf::io::map_inputs(table, mf::mf5x::PAC2002_INPUTS);
  ASSERT_TRUE(mapped.has_value());
  EXPECT_EQ(mapped->values.rows(), 0);
  EXPECT_EQ(mapped->values.cols(), 5);
  EXPECT_TRUE(mapped->lines.empty());
}

TEST(map_inputs, LinesPreserved_CommentsAndBlankLinesSkipped) {
  const auto table = parse_success("# comment\n"
                                   "\n"
                                   "FZ SA SR IA VX\n"
                                   "1 2 3 4 5\n"
                                   "\n"
                                   "# between rows\n"
                                   "6 7 8 9 10\n");
  const auto mapped = mf::io::map_inputs(table, mf::mf5x::PAC2002_INPUTS);
  ASSERT_TRUE(mapped.has_value());
  const std::vector<std::size_t> expected_lines{4U, 7U};
  EXPECT_EQ(mapped->lines, expected_lines);
}

TEST(read_model_inputs, ValidFile_MapsColumns) {
  const auto mapped = mf::io::read_model_inputs(mf::test::data_file("inputs_s_vectors.csv"), mf::mf5x::PAC2002_INPUTS);
  ASSERT_TRUE(mapped.has_value());
  ASSERT_EQ(mapped->values.rows(), 6);
  ASSERT_EQ(mapped->values.cols(), 5);
  EXPECT_TRUE(mapped->ignored_columns.empty());
  const std::vector<std::size_t> expected_lines{4U, 5U, 6U, 7U, 8U, 9U};
  EXPECT_EQ(mapped->lines, expected_lines);
  // Row 1 is "4000,0.05,0,0,10"; the last row has a negative load.
  expect_same(mapped->values(1, mf::mf5x::FZ), 4000.0);
  expect_same(mapped->values(1, mf::mf5x::SA), 0.05);
  expect_same(mapped->values(1, mf::mf5x::SR), 0.0);
  expect_same(mapped->values(1, mf::mf5x::IA), 0.0);
  expect_same(mapped->values(1, mf::mf5x::VX), 10.0);
  expect_same(mapped->values(5, mf::mf5x::FZ), -100.0);
}

TEST(read_model_inputs, MissingFile_IoErrorNamesPath) {
  const std::filesystem::path path = mf::test::data_file("no_such_inputs_file.csv");
  const auto mapped = mf::io::read_model_inputs(path, mf::mf5x::PAC2002_INPUTS);
  ASSERT_FALSE(mapped.has_value());
  EXPECT_EQ(mapped.error().code, mf::error_code::IO);
  EXPECT_EQ(mapped.error().where.file, path.string());
}

TEST(read_model_inputs, MissingChannel_ErrorCarriesFileName) {
  const std::filesystem::path path = mf::test::data_file("inputs_s_vectors.csv");
  const auto mapped = mf::io::read_model_inputs(path, UNKNOWN_CHANNELS);
  ASSERT_FALSE(mapped.has_value());
  EXPECT_EQ(mapped.error().code, mf::error_code::MISSING_KEY);
  EXPECT_EQ(mapped.error().where.file, path.string());
  EXPECT_EQ(mapped.error().where.line, 3U);
  EXPECT_EQ(mapped.error().where.key, "Q");
}

TEST(read_model_inputs, DegreesFile_ConvertedToRadians) {
  const auto mapped = mf::io::read_model_inputs(mf::test::data_file("inputs_degrees.txt"), mf::mf5x::PAC2002_INPUTS);
  ASSERT_TRUE(mapped.has_value());
  ASSERT_EQ(mapped->values.rows(), 6);
  const std::vector<std::size_t> expected_lines{5U, 6U, 9U, 10U, 11U, 12U};
  EXPECT_EQ(mapped->lines, expected_lines);
  // The file has the columns reordered (FZ SR SA IA VX); the mapped matrix is in channel order.
  constexpr double angle_tol_rad = 1e-12; // the file prints the degree values with 17 digits
  EXPECT_NEAR(mapped->values(1, mf::mf5x::SA), 0.05, angle_tol_rad);
  EXPECT_NEAR(mapped->values(2, mf::mf5x::SA), 0.1, angle_tol_rad);
  EXPECT_NEAR(mapped->values(1, mf::mf5x::SR), 0.0, angle_tol_rad);
  EXPECT_NEAR(mapped->values(2, mf::mf5x::SR), 0.1, angle_tol_rad);
  EXPECT_NEAR(mapped->values(1, mf::mf5x::IA), 0.0, angle_tol_rad);
  EXPECT_NEAR(mapped->values(1, mf::mf5x::VX), 10.0, angle_tol_rad);
  EXPECT_NEAR(mapped->values(5, mf::mf5x::FZ), -100.0, angle_tol_rad);
}

TEST_F(input_table_file, ReadInputTable_ParseError_NamesThePath) {
  write_text("FZ SA SR IA VX\n1 2 3 4\n");
  const auto result = mf::io::read_input_table(path_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::SYNTAX);
  EXPECT_EQ(result.error().where.file, path_.string());
  EXPECT_EQ(result.error().where.line, 2U);
}
