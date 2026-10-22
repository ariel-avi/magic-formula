#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <magic_formula/error.hpp>
#include <magic_formula/io/input_table.hpp>
#include <numbers>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

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

  /// Expects the five SI fields of `row` (bit-exact).
  void expect_point(const mf::io::input_row &row, double fz, double alpha, double kappa, double gamma, double vx) {
    expect_same(row.point.fz_n, fz);
    expect_same(row.point.alpha_rad, alpha);
    expect_same(row.point.kappa, kappa);
    expect_same(row.point.gamma_rad, gamma);
    expect_same(row.point.vx_mps, vx);
  }

  /// One unit-table entry: a header, a row and the SI value that the column under test must have.
  struct unit_case {
    std::string_view header;
    std::string_view row;
    double (*get)(const mf::io::input_row &);
    double expected;
  };

  const std::array<unit_case, 13> UNIT_CASES{
      unit_case{.header = "FZ[N] SA SR IA VX",
                .row = "4000 0 0 0 0",
                .get = [](const mf::io::input_row &row) { return row.point.fz_n; },
                .expected = 4000.0},
      unit_case{.header = "fz[n] SA SR IA VX",
                .row = "3500.5 0 0 0 0",
                .get = [](const mf::io::input_row &row) { return row.point.fz_n; },
                .expected = 3500.5},
      unit_case{.header = "FZ SA[rad] SR IA VX",
                .row = "0 0.25 0 0 0",
                .get = [](const mf::io::input_row &row) { return row.point.alpha_rad; },
                .expected = 0.25},
      unit_case{.header = "FZ SA[deg] SR IA VX",
                .row = "0 30 0 0 0",
                .get = [](const mf::io::input_row &row) { return row.point.alpha_rad; },
                .expected = 30.0 * DEG_TO_RAD},
      unit_case{.header = "FZ SA SR[-] IA VX",
                .row = "0 0 0.1 0 0",
                .get = [](const mf::io::input_row &row) { return row.point.kappa; },
                .expected = 0.1},
      unit_case{.header = "FZ SA SR IA[rad] VX",
                .row = "0 0 0 0.05 0",
                .get = [](const mf::io::input_row &row) { return row.point.gamma_rad; },
                .expected = 0.05},
      unit_case{.header = "FZ SA SR IA[DEG] VX",
                .row = "0 0 0 -4 0",
                .get = [](const mf::io::input_row &row) { return row.point.gamma_rad; },
                .expected = -4.0 * DEG_TO_RAD},
      unit_case{.header = "FZ SA SR IA VX[m/s]",
                .row = "0 0 0 0 12.5",
                .get = [](const mf::io::input_row &row) { return row.point.vx_mps; },
                .expected = 12.5},
      unit_case{.header = "FZ SA SR IA VX[M/S]",
                .row = "0 0 0 0 7",
                .get = [](const mf::io::input_row &row) { return row.point.vx_mps; },
                .expected = 7.0},
      unit_case{.header = "FZ SA SR IA VX P[Pa]",
                .row = "0 0 0 0 0 220000",
                .get = [](const mf::io::input_row &row) { return row.pressure_pa.value_or(-1.0); },
                .expected = 220000.0},
      unit_case{.header = "FZ SA SR IA VX p[PA]",
                .row = "0 0 0 0 0 180000.5",
                .get = [](const mf::io::input_row &row) { return row.pressure_pa.value_or(-1.0); },
                .expected = 180000.5},
      unit_case{.header = "FZ SA SR IA VX",
                .row = "4000 0.1 0.2 0.3 7",
                .get = [](const mf::io::input_row &row) { return row.point.alpha_rad; },
                .expected = 0.1},
      unit_case{.header = "FZ SA SR IA VX",
                .row = "4000 0.1 0.2 0.3 7",
                .get = [](const mf::io::input_row &row) { return row.point.gamma_rad; },
                .expected = 0.3},
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
  ASSERT_EQ(table.rows.size(), 2U);
  EXPECT_FALSE(table.has_pressure);
  EXPECT_EQ(table.rows[0].line, 6U);
  EXPECT_EQ(table.rows[1].line, 9U);
  expect_point(table.rows[0], 4000.0, 0.05, 0.1, 0.0, 10.0);
  expect_point(table.rows[1], 3000.0, 0.0, 0.0, 0.0, 5.0);
}

TEST(input_table, Parse_CommaSeparated_Rows) {
  const auto table = parse_success("FZ[N],SA[rad],SR[-],IA[rad],VX[m/s]\n"
                                   "1,2,3,4,5\n"
                                   "6.5,-7,8e-1,0.25,+9\n");
  ASSERT_EQ(table.rows.size(), 2U);
  // Header order FZ, SA, SR, IA, VX maps to fz_n, alpha_rad, kappa, gamma_rad, vx_mps.
  expect_point(table.rows[0], 1.0, 2.0, 3.0, 4.0, 5.0);
  expect_point(table.rows[1], 6.5, -7.0, 0.8, 0.25, 9.0);
  EXPECT_EQ(table.rows[0].line, 2U);
  EXPECT_EQ(table.rows[1].line, 3U);
}

TEST(input_table, Parse_BlankSeparated_Rows) {
  const auto table = parse_success("FZ SA SR IA VX\n"
                                   "1 2 3 4 5\n"
                                   "  6   7  8 9    10  \n");
  ASSERT_EQ(table.rows.size(), 2U);
  expect_point(table.rows[0], 1.0, 2.0, 3.0, 4.0, 5.0);
  expect_point(table.rows[1], 6.0, 7.0, 8.0, 9.0, 10.0);
}

TEST(input_table, Parse_TabSeparated_Rows) {
  const auto table = parse_success("FZ[N]\tSA[rad]\tSR[-]\tIA[rad]\tVX[m/s]\n"
                                   "1\t2\t3\t4\t5\n");
  ASSERT_EQ(table.rows.size(), 1U);
  expect_point(table.rows[0], 1.0, 2.0, 3.0, 4.0, 5.0);
}

TEST(input_table, Parse_MixedSeparators_Rows) {
  const auto table = parse_success("FZ, SA ,\tSR,,IA VX,\n"
                                   "4000 ,, 0.05\t0.1,0 ,10,\n"
                                   "1,,2,3,,,4,5\n"
                                   ",,7 8 9 10 11,,\n");
  ASSERT_EQ(table.rows.size(), 3U);
  expect_point(table.rows[0], 4000.0, 0.05, 0.1, 0.0, 10.0);
  // "1,,2" is two fields, not three.
  expect_point(table.rows[1], 1.0, 2.0, 3.0, 4.0, 5.0);
  // Leading and trailing separators produce no fields.
  expect_point(table.rows[2], 7.0, 8.0, 9.0, 10.0, 11.0);
}

TEST(input_table, Parse_HeaderCaseAndOrder_Mapped) {
  const auto table = parse_success("vx Ia sr SA fz\n"
                                   "5 4 3 2 1\n");
  ASSERT_EQ(table.rows.size(), 1U);
  expect_point(table.rows[0], 1.0, 2.0, 3.0, 4.0, 5.0);
}

TEST(input_table, Parse_NumberForms_Accepted) {
  const auto table = parse_success("FZ SA SR IA VX\n"
                                   "1e3 -0.0 +.5 2.5E-1 -7\n");
  ASSERT_EQ(table.rows.size(), 1U);
  // Negative speed and signed zero are parsed as given; rejecting them is the model's job.
  expect_point(table.rows[0], 1000.0, -0.0, 0.5, 0.25, -7.0);
}

TEST_P(input_table_units, Parse_Unit_AppliesFactor) {
  const unit_case &param = GetParam();
  const std::string text = std::string(param.header) + "\n" + std::string(param.row) + "\n";
  const auto table = parse_success(text);
  ASSERT_EQ(table.rows.size(), 1U) << text;
  // Bit-exact: degrees are converted as value * (pi / 180), every other unit is multiplied by exactly 1.
  expect_same(param.get(table.rows[0]), param.expected);
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
  EXPECT_TRUE(table.has_pressure);
  ASSERT_EQ(table.rows.size(), 1U);
  ASSERT_TRUE(table.rows[0].pressure_pa.has_value());
  expect_same(table.rows[0].pressure_pa.value_or(-1.0), 6.0);
  expect_point(table.rows[0], 1.0, 2.0, 3.0, 4.0, 5.0);
}

TEST(input_table, Parse_PressureColumnFirst_ValuesMapped) {
  const auto table = parse_success("P[Pa] FZ SA SR IA VX\n"
                                   "220000 1 2 3 4 5\n"
                                   "0 6 7 8 9 10\n");
  EXPECT_TRUE(table.has_pressure);
  ASSERT_EQ(table.rows.size(), 2U);
  ASSERT_TRUE(table.rows[0].pressure_pa.has_value());
  expect_same(table.rows[0].pressure_pa.value_or(-1.0), 220000.0);
  expect_point(table.rows[0], 1.0, 2.0, 3.0, 4.0, 5.0);
  ASSERT_TRUE(table.rows[1].pressure_pa.has_value());
  expect_same(table.rows[1].pressure_pa.value_or(-1.0), 0.0);
  expect_point(table.rows[1], 6.0, 7.0, 8.0, 9.0, 10.0);
}

TEST(input_table, Parse_NoPressureColumn_PressureEmpty) {
  const auto table = parse_success("FZ SA SR IA VX\n"
                                   "1 2 3 4 5\n");
  EXPECT_FALSE(table.has_pressure);
  ASSERT_EQ(table.rows.size(), 1U);
  EXPECT_FALSE(table.rows[0].pressure_pa.has_value());
}

TEST(input_table, Parse_HeaderOnly_ZeroRows) {
  const auto with_newline = parse_success("FZ[N],SA[rad],SR[-],IA[rad],VX[m/s]\n");
  EXPECT_TRUE(with_newline.rows.empty());
  EXPECT_FALSE(with_newline.has_pressure);

  const auto without_newline = parse_success("FZ SA SR IA VX P");
  EXPECT_TRUE(without_newline.rows.empty());
  EXPECT_TRUE(without_newline.has_pressure);
}

TEST(input_table, Parse_NoFinalNewline_LastLineParsed) {
  const auto table = parse_success("FZ SA SR IA VX\n"
                                   "1 2 3 4 5\n"
                                   "6 7 8 9 10");
  ASSERT_EQ(table.rows.size(), 2U);
  EXPECT_EQ(table.rows[1].line, 3U);
  expect_point(table.rows[1], 6.0, 7.0, 8.0, 9.0, 10.0);
}

TEST(input_table, Parse_Utf8Bom_Skipped) {
  const auto table = parse_success("\xEF\xBB\xBF"
                                   "FZ[N],SA[rad],SR[-],IA[rad],VX[m/s]\n"
                                   "4000,0.1,0.2,0.3,10\n");
  ASSERT_EQ(table.rows.size(), 1U);
  EXPECT_EQ(table.rows[0].line, 2U);
  expect_point(table.rows[0], 4000.0, 0.1, 0.2, 0.3, 10.0);
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
  ASSERT_EQ(result->rows.size(), 1U);
  EXPECT_EQ(result->rows[0].line, 3U);
  expect_point(result->rows[0], 4000.0, 30.0 * DEG_TO_RAD, 0.1, 0.0, 10.0);
}

TEST_F(input_table_file, ReadInputTable_ParseError_NamesThePath) {
  write_text("FZ SA SR IA VX\n1 2 3 4\n");
  const auto result = mf::io::read_input_table(path_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::SYNTAX);
  EXPECT_EQ(result.error().where.file, path_.string());
  EXPECT_EQ(result.error().where.line, 2U);
}
