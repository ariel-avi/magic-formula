#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <gtest/gtest.h>
#include <ios>
#include <magic_formula/cli/cli.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/number.hpp>
#include <magic_formula/io/result_writer.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/version.hpp>
#include <optional>
#include <ostream>
#include <span>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
  using mf::test::bits;
  using mf::test::data_file;
  using mf::test::near_scaled;
  using mf::test::printed_tol;
  using mf::test::rel_near_result;

  /// Number of comma-separated fields on every result line (5 inputs + 5 outputs + 5 flags + 8 characteristics).
  constexpr std::size_t FIELD_COUNT = 23;

  /// Number of characteristic fields at the end of every result line.
  constexpr std::size_t CHARACTERISTIC_COUNT = 8;

  /// Index of the first characteristic field of a result line.
  constexpr std::size_t FIRST_CHARACTERISTIC = FIELD_COUNT - CHARACTERISTIC_COUNT;

  /// The exact header line of every run (PAC2002 and MF 5.2 alike).
  constexpr std::string_view EXPECTED_HEADER =
      "FZ[N],SA[rad],SR[-],IA[rad],VX[m/s],FX[N],FY[N],MZ[N*m],MX[N*m],MY[N*m],FZ_CLAMPED[-],SR_CLAMPED[-],"
      "SA_CLAMPED[-],IA_CLAMPED[-],BELOW_VXLOW[-],DFY_DSA[N/rad],DFX_DSR[N],FY_OVER_SA[N/rad],FX_OVER_SR[N],DY[N],"
      "DX[N],SA_AT_PEAK_FY[rad],SR_AT_PEAK_FX[-]";

  /// Exit status and both streams of one in-process run.
  struct run_result {
    int code = -1;
    std::string out;
    std::string err;
  };

  /// One parsed result line: the five echoed inputs, the five outputs, the five 0/1 flags, then the eight
  /// characteristics in the order of `mf::mf5x::characteristic_index` (a printed "nan" reads as NaN).
  struct result_row {
    double fz = 0.0;
    double sa = 0.0;
    double sr = 0.0;
    double ia = 0.0;
    double vx = 0.0;
    double fx = 0.0;
    double fy = 0.0;
    double mz = 0.0;
    double mx = 0.0;
    double my = 0.0;
    double fz_clamped = 0.0;
    double sr_clamped = 0.0;
    double sa_clamped = 0.0;
    double ia_clamped = 0.0;
    double below_vxlow = 0.0;
    std::array<double, CHARACTERISTIC_COUNT> characteristics{};
  };

  /// A streambuf that rejects every character, so that every insertion into a stream on it fails.
  class failing_buffer : public std::streambuf {
  protected:
    int_type overflow(int_type /*character*/) override {
      return traits_type::eof();
    }
  };

  /// A streambuf whose overflow() throws something that is not a std::exception.
  class non_std_throwing_buffer : public std::streambuf {
  protected:
    int_type overflow(int_type /*character*/) override {
      throw 42; // NOLINT(hicpp-exception-baseclass,cert-err60-cpp): the test needs a non-std exception type
    }
  };

  /// Calls `mf::cli::run` with "mf" followed by `arguments`.
  run_result run_cli(const std::vector<std::string> &arguments) {
    std::vector<std::string_view> argv{"mf"};
    argv.insert(argv.end(), arguments.begin(), arguments.end());
    std::ostringstream out;
    std::ostringstream err;
    run_result result;
    result.code = mf::cli::run(argv, out, err);
    result.out = out.str();
    result.err = err.str();
    return result;
  }

  /// Arguments for a normal run: the options, then the input table and the .tir file of tests/data.
  std::vector<std::string> file_arguments(std::string_view table, std::string_view tir,
                                          std::vector<std::string> options = {}) {
    options.push_back(data_file(table).string());
    options.push_back(data_file(tir).string());
    return options;
  }

  /// Runs the CLI on two files of tests/data.
  run_result run_files(std::string_view table, std::string_view tir, std::vector<std::string> options = {}) {
    return run_cli(file_arguments(table, tir, std::move(options)));
  }

  /// Splits `text` into lines (a final line terminator does not produce an empty last line).
  std::vector<std::string> lines_of(const std::string &text) {
    std::istringstream stream(text);
    std::vector<std::string> lines;
    for (std::string line; std::getline(stream, line);) {
      lines.push_back(line);
    }
    return lines;
  }

  /// The text fields of one result line.
  std::vector<std::string> fields_of(const std::string &line) {
    std::vector<std::string> fields;
    std::istringstream stream(line);
    for (std::string field; std::getline(stream, field, ',');) {
      fields.push_back(field);
    }
    return fields;
  }

  /// The numbers of one result line; "nan" reads as NaN, any other field that is not a number makes the test fail and
  /// reads as NaN.
  result_row row_of(const std::string &line) {
    std::vector<double> values;
    for (const std::string &field : fields_of(line)) {
      const std::optional<double> parsed = mf::io::parse_number(field);
      EXPECT_TRUE(parsed.has_value() || field == "nan") << "not a number: '" << field << "' in " << line;
      values.push_back(parsed.value_or(std::nan("")));
    }
    if (values.size() != FIELD_COUNT) {
      ADD_FAILURE() << "expected " << FIELD_COUNT << " fields, got " << values.size() << " in " << line;
      return result_row{};
    }
    std::array<double, CHARACTERISTIC_COUNT> characteristics{};
    std::copy(values.begin() + static_cast<std::ptrdiff_t>(FIRST_CHARACTERISTIC), values.end(),
              characteristics.begin());
    return result_row{.fz = values[0],
                      .sa = values[1],
                      .sr = values[2],
                      .ia = values[3],
                      .vx = values[4],
                      .fx = values[5],
                      .fy = values[6],
                      .mz = values[7],
                      .mx = values[8],
                      .my = values[9],
                      .fz_clamped = values[10],
                      .sr_clamped = values[11],
                      .sa_clamped = values[12],
                      .ia_clamped = values[13],
                      .below_vxlow = values[14],
                      .characteristics = characteristics};
  }

  /// All data rows of a stdout text (the header line is skipped).
  std::vector<result_row> rows_of(const std::string &out) {
    const std::vector<std::string> lines = lines_of(out);
    std::vector<result_row> rows;
    for (std::size_t index = 1; index < lines.size(); ++index) {
      rows.push_back(row_of(lines[index]));
    }
    return rows;
  }

  /// How often `needle` occurs in `text`.
  std::size_t count_of(const std::string &text, std::string_view needle) {
    std::size_t count = 0;
    for (std::size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + needle.size())) {
      ++count;
    }
    return count;
  }

  /// Every error run: exit 1, nothing on stdout, a message that starts with "mf: error: ".
  void expect_failed_run(const run_result &result) {
    EXPECT_EQ(result.code, mf::cli::EXIT_ERROR);
    EXPECT_TRUE(result.out.empty()) << result.out;
    EXPECT_TRUE(result.err.starts_with("mf: error: ")) << result.err;
  }

  /// A usage error: exit 2, nothing on stdout, the message and the usage text on stderr.
  void expect_usage_run(const run_result &result, std::string_view message) {
    EXPECT_EQ(result.code, mf::cli::EXIT_USAGE);
    EXPECT_TRUE(result.out.empty()) << result.out;
    EXPECT_TRUE(result.err.starts_with(std::string("mf: error: ").append(message))) << result.err;
    EXPECT_TRUE(result.err.ends_with(mf::cli::usage())) << result.err;
  }

  /// The echoed inputs of a row are bit-equal to the table values (FZ, SA, SR, IA, VX).
  void expect_inputs(const result_row &row, double fz, double sa, double sr, double ia, double vx) {
    EXPECT_EQ(bits(row.fz), bits(fz));
    EXPECT_EQ(bits(row.sa), bits(sa));
    EXPECT_EQ(bits(row.sr), bits(sr));
    EXPECT_EQ(bits(row.ia), bits(ia));
    EXPECT_EQ(bits(row.vx), bits(vx));
  }

  /// The flags of a row are exactly 0 or 1 as given (FZ, SR, SA, IA clamped; VX below VXLOW).
  void expect_flags(const result_row &row, bool fz_clamped, bool sr_clamped, bool sa_clamped, bool ia_clamped,
                    bool below_vxlow) {
    EXPECT_EQ(bits(row.fz_clamped), bits(fz_clamped ? 1.0 : 0.0));
    EXPECT_EQ(bits(row.sr_clamped), bits(sr_clamped ? 1.0 : 0.0));
    EXPECT_EQ(bits(row.sa_clamped), bits(sa_clamped ? 1.0 : 0.0));
    EXPECT_EQ(bits(row.ia_clamped), bits(ia_clamped ? 1.0 : 0.0));
    EXPECT_EQ(bits(row.below_vxlow), bits(below_vxlow ? 1.0 : 0.0));
  }

  /// All five flags of a row are 0.
  void expect_no_flags(const result_row &row) {
    expect_flags(row, false, false, false, false, false);
  }

  /// V22: Fz <= 0 gives exactly zero (of either sign) for all five outputs.
  void expect_zero_outputs(const result_row &row) {
    EXPECT_EQ(bits(row.fx), bits(0.0));
    EXPECT_EQ(bits(row.fy), bits(0.0));
    EXPECT_EQ(bits(row.mz), bits(0.0));
    EXPECT_EQ(bits(row.mx), bits(0.0));
    EXPECT_EQ(bits(row.my), bits(0.0));
  }

  /// `actual` equals `expected` within 1e-9 relative, plus a 1e-12 absolute floor for values that are exactly zero in
  /// one of the two runs.
  bool near_relative(double actual, double expected) {
    return std::abs(actual - expected) <= (1e-9 * std::abs(expected)) + 1e-12;
  }

  /// Fixed text of the pressure warning for PAC2002.
  constexpr std::string_view PRESSURE_WARNING = "column P is ignored: PAC2002 has no P input";

  /// Fixed text of the pressure warning for MF 5.2.
  constexpr std::string_view PRESSURE_WARNING_MF52 = "column P is ignored: MF 5.2 has no P input";

  /// The start of the identification warning of the S-derived models (vertical axis ISO, lateral axis SAE).
  constexpr std::string_view MIXED_WARNING_START = "mf: warning: coordinate system: the coefficients mix conventions";

  /// The whole identification warning of the S-derived models.
  constexpr std::string_view MIXED_WARNING =
      "mf: warning: coordinate system: the coefficients mix conventions: the vertical axis is ISO (a loaded tire has "
      "FZ > 0) but the lateral axis is SAE (dFy/dSA < 0); convert the model to ISO or SAE\n";

  /// The S-derived models have a loaded-ISO vertical axis and an SAE lateral axis: exactly one identification warning,
  /// the mixed-conventions one.
  void expect_mixed_warning(const std::string &err) {
    EXPECT_EQ(count_of(err, "mf: warning: coordinate system: "), 1U) << err;
    EXPECT_EQ(count_of(err, MIXED_WARNING), 1U) << err;
  }

  /// The warning of an ISO table that mixes FZ signs, one row of the opposite sign.
  std::string mixed_iso_fz_warning(std::string_view table, std::size_t line) {
    return "mf: warning: " + data_file(table).string() + ":" + std::to_string(line) +
           ": FZ < 0 in 1 row(s) of an ISO table: a loaded tire has FZ > 0 in ISO; these rows are lift-off (zero "
           "forces and moments) (the table mixes FZ signs)\n";
  }
} // namespace

TEST(cli_run, Run_PacSVectors_PrintsExpectedRows) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S.tir");
  EXPECT_EQ(result.code, mf::cli::EXIT_OK);
  // The coefficients mix conventions (FNOMIN > 0 but dFy/dSA < 0), and the FZ = -100 row (line 10) of the ISO table is
  // a lift-off.
  expect_mixed_warning(result.err);
  EXPECT_TRUE(result.err.ends_with(mixed_iso_fz_warning("inputs_s_vectors.csv", 10))) << result.err;
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 2U) << result.err;

  const std::vector<std::string> lines = lines_of(result.out);
  ASSERT_EQ(lines.size(), 7U);
  EXPECT_EQ(lines[0], mf::io::results_header(mf::mf5x::PAC2002_LAYOUT));
  ASSERT_FALSE(result.out.empty());
  EXPECT_EQ(result.out.back(), '\n');

  const std::vector<result_row> rows = rows_of(result.out);
  ASSERT_EQ(rows.size(), 6U);

  // V2: pure longitudinal slip, Fx0 = 4000*sin(1.5*atan(4/3)).
  expect_inputs(rows[0], 4000.0, 0.0, 0.1, 0.0, 10.0);
  EXPECT_NEAR(rows[0].fx, 3935.48, printed_tol(2));
  // V3 / V11: pure slip angle, Fy0 and Mz0.
  expect_inputs(rows[1], 4000.0, 0.05, 0.0, 0.0, 10.0);
  EXPECT_NEAR(rows[1].fy, -2376.40, printed_tol(2));
  EXPECT_NEAR(rows[1].mz, 60.46, printed_tol(2));
  // V12: Fx = Fx0 * cos(pi/4).
  expect_inputs(rows[2], 4000.0, 0.1, 0.1, 0.0, 10.0);
  EXPECT_NEAR(rows[2].fx, 2782.80, printed_tol(2));
  // V13.
  expect_inputs(rows[3], 4000.0, 0.05, 0.1, 0.0, 10.0);
  EXPECT_NEAR(rows[3].fy, -1680.37, printed_tol(2));
  // V22: Fz = 0 and Fz = -100 give exactly zero outputs; the inputs are still echoed.
  expect_inputs(rows[4], 0.0, 0.05, 0.1, 0.0, 10.0);
  expect_zero_outputs(rows[4]);
  expect_inputs(rows[5], -100.0, 0.05, 0.1, 0.0, 10.0);
  expect_zero_outputs(rows[5]);

  // My = -Fz*R0*QSY1 = -12 N*m for the four loaded rows (V5, V21).
  for (std::size_t index = 0; index < 4; ++index) {
    EXPECT_TRUE(rel_near_result(rows[index].my, -12.0, mf::test::REL_TOL_CLOSED_FORM)) << "row " << index;
  }
}

TEST(cli_run, Run_Mf52Vectors_PrintsM1M2M5) {
  const run_result result = run_files("inputs_s_vectors.csv", "mf52_S52.tir");
  EXPECT_EQ(result.code, mf::cli::EXIT_OK);
  // The PAC2002-only camber factors of the file give one warning.
  EXPECT_TRUE(result.err.contains("mf: warning: ")) << result.err;
  EXPECT_TRUE(result.err.contains("LGAX, LGAY, LGAZ")) << result.err;
  // The load warning, the identification warning and the load-side warning.
  expect_mixed_warning(result.err);
  EXPECT_TRUE(result.err.ends_with(mixed_iso_fz_warning("inputs_s_vectors.csv", 10))) << result.err;
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 3U) << result.err;

  const std::vector<result_row> rows = rows_of(result.out);
  ASSERT_EQ(rows.size(), 6U);
  // M1.
  EXPECT_NEAR(rows[0].fx, 3935.48, printed_tol(2));
  // M2 and M5.
  EXPECT_NEAR(rows[1].fy, -2376.40, printed_tol(2));
  EXPECT_NEAR(rows[1].mz, 60.46, printed_tol(2));
  // Fz <= 0 gives zero outputs for MF 5.2 as well.
  expect_zero_outputs(rows[4]);
  expect_zero_outputs(rows[5]);
}

TEST(cli_run, Run_Mf52UserPff_ExitsZeroWithFiniteRowsEqualToMf52Run) {
  // PROPERTY_FILE_FORMAT = 'USER' with FITTYP 6 routes to MF 5.2: same numbers, no warning.
  const run_result user = run_files("inputs_s_vectors.csv", "mf52_user_pff.tir");
  EXPECT_EQ(user.code, mf::cli::EXIT_OK) << user.err;
  // No load warning: only the identification warning and the load-side warning.
  expect_mixed_warning(user.err);
  EXPECT_TRUE(user.err.ends_with(mixed_iso_fz_warning("inputs_s_vectors.csv", 10))) << user.err;
  EXPECT_EQ(count_of(user.err, "mf: warning: "), 2U) << user.err;

  const std::vector<result_row> rows = rows_of(user.out);
  ASSERT_EQ(rows.size(), 6U);
  for (const result_row &row : rows) {
    EXPECT_TRUE(std::isfinite(row.fx) && std::isfinite(row.fy) && std::isfinite(row.mz) && std::isfinite(row.mx) &&
                std::isfinite(row.my));
  }
  EXPECT_NEAR(rows[0].fx, 3935.48, printed_tol(2));
  EXPECT_NEAR(rows[1].fy, -2376.40, printed_tol(2));

  const run_result reference = run_files("inputs_s_vectors.csv", "mf52_S52.tir");
  ASSERT_EQ(reference.code, mf::cli::EXIT_OK) << reference.err;
  EXPECT_EQ(user.out, reference.out);
}

TEST(cli_run, Run_DegreesTable_EqualsCsvRun) {
  const run_result reference = run_files("inputs_s_vectors.csv", "pac2002_S.tir");
  const run_result degrees = run_files("inputs_degrees.txt", "pac2002_S.tir");
  ASSERT_EQ(reference.code, mf::cli::EXIT_OK);
  ASSERT_EQ(degrees.code, mf::cli::EXIT_OK) << degrees.err;
  // The FZ = -100 row is on line 14 of this table.
  expect_mixed_warning(degrees.err);
  EXPECT_TRUE(degrees.err.ends_with(mixed_iso_fz_warning("inputs_degrees.txt", 14))) << degrees.err;
  EXPECT_EQ(count_of(degrees.err, "mf: warning: "), 2U) << degrees.err;
  EXPECT_EQ(lines_of(degrees.out).front(), mf::io::results_header(mf::mf5x::PAC2002_LAYOUT));

  const std::vector<result_row> expected = rows_of(reference.out);
  const std::vector<result_row> actual = rows_of(degrees.out);
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t index = 0; index < expected.size(); ++index) {
    const result_row &want = expected[index];
    const result_row &got = actual[index];
    EXPECT_TRUE(near_relative(got.fz, want.fz)) << "FZ row " << index;
    EXPECT_TRUE(near_relative(got.sa, want.sa)) << "SA row " << index;
    EXPECT_TRUE(near_relative(got.sr, want.sr)) << "SR row " << index;
    EXPECT_TRUE(near_relative(got.ia, want.ia)) << "IA row " << index;
    EXPECT_TRUE(near_relative(got.vx, want.vx)) << "VX row " << index;
    EXPECT_TRUE(near_relative(got.fx, want.fx)) << "FX row " << index;
    EXPECT_TRUE(near_relative(got.fy, want.fy)) << "FY row " << index;
    EXPECT_TRUE(near_relative(got.mz, want.mz)) << "MZ row " << index;
    EXPECT_TRUE(near_relative(got.mx, want.mx)) << "MX row " << index;
    EXPECT_TRUE(near_relative(got.my, want.my)) << "MY row " << index;
    EXPECT_EQ(bits(got.fz_clamped), bits(want.fz_clamped)) << "FZ_CLAMPED row " << index;
    EXPECT_EQ(bits(got.sr_clamped), bits(want.sr_clamped)) << "SR_CLAMPED row " << index;
    EXPECT_EQ(bits(got.sa_clamped), bits(want.sa_clamped)) << "SA_CLAMPED row " << index;
    EXPECT_EQ(bits(got.ia_clamped), bits(want.ia_clamped)) << "IA_CLAMPED row " << index;
    EXPECT_EQ(bits(got.below_vxlow), bits(want.below_vxlow)) << "BELOW_VXLOW row " << index;
  }
}

TEST(cli_run, Run_Uncombined_PrintsPureForces) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S.tir", {"--uncombined"});
  EXPECT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  expect_mixed_warning(result.err);
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 2U) << result.err;
  const std::vector<result_row> rows = rows_of(result.out);
  ASSERT_EQ(rows.size(), 6U);
  // Row 3 has kappa = 0.1 and alpha = 0.1; pure slip leaves Fx0 of V2 uncut by the combined-slip weighting.
  EXPECT_NEAR(rows[2].fx, 3935.48, printed_tol(2));
  // Pure lateral force of V3 at alpha = 0.05 in row 4, regardless of the longitudinal slip.
  EXPECT_NEAR(rows[3].fy, -2376.40, printed_tol(2));
}

TEST(cli_run, Run_Verbose_PrintsModelMetadataAndIgnoredKeys) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S.tir", {"--verbose"});
  EXPECT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  EXPECT_TRUE(result.err.contains("mf: info: model PAC2002\n")) << result.err;
  EXPECT_TRUE(result.err.contains("mf: info: FITTYP absent, USE_MODE 4, TYRESIDE LEFT\n")) << result.err;
  EXPECT_TRUE(result.err.contains(
      "mf: info: ignored keys (7): FILE_TYPE, FILE_VERSION, FILE_FORMAT, LCZ, LSGKP, LSGAL, LGYR\n"))
      << result.err;
  // The model line, the metadata line, the ignored keys, the identification line, 29 readings and the table line.
  EXPECT_EQ(count_of(result.err, "mf: info: "), 3U + 1U + 29U + 1U) << result.err;
  // The identification warning and the load-side warning of the FZ = -100 row.
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 2U) << result.err;
  EXPECT_EQ(lines_of(result.out).size(), 7U);
}

TEST(cli_run, Run_PressureColumn_WarnsOnceAndSucceeds) {
  const run_result result = run_files("inputs_pressure.csv", "pac2002_S.tir");
  EXPECT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  // The identification warning and the pressure warning.
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 2U) << result.err;
  expect_mixed_warning(result.err);
  EXPECT_EQ(count_of(result.err, PRESSURE_WARNING), 1U) << result.err;
  EXPECT_TRUE(result.err.contains(data_file("inputs_pressure.csv").string() + ": " + std::string(PRESSURE_WARNING)))
      << result.err;

  const std::vector<std::string> lines = lines_of(result.out);
  ASSERT_EQ(lines.size(), 2U);
  EXPECT_EQ(lines[0], mf::io::results_header(mf::mf5x::PAC2002_LAYOUT));
  const result_row row = row_of(lines[1]);
  // The pressure is not an output column and does not change the result: same point as row 4 of the vectors table.
  expect_inputs(row, 4000.0, 0.05, 0.1, 0.0, 10.0);
  EXPECT_NEAR(row.fy, -1680.37, printed_tol(2));
}

TEST(cli_run, Run_Ranges_OneWarningPerVariable) {
  const run_result result = run_files("inputs_ranges.csv", "pac2002_S_rng.tir");
  EXPECT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  const std::string table = data_file("inputs_ranges.csv").string();
  EXPECT_TRUE(result.err.contains(table + ":6: SR outside the property-file range in 1 row(s); "
                                          "evaluated at the limit\n"))
      << result.err;
  EXPECT_TRUE(result.err.contains(table + ":7: SA outside the property-file range in 1 row(s); "
                                          "evaluated at the limit\n"))
      << result.err;
  EXPECT_TRUE(result.err.contains(table + ":8: IA outside the property-file range in 1 row(s); "
                                          "evaluated at the limit\n"))
      << result.err;
  EXPECT_TRUE(result.err.contains(table + ":9: FZ outside the property-file range in 2 row(s); "
                                          "evaluated at the limit\n"))
      << result.err;
  // The four range warnings and the identification warning.
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 5U) << result.err;

  const std::expected<mf::io::loaded_model, mf::error> loaded =
      mf::io::load_tire_model_file(data_file("pac2002_S_rng.tir"));
  ASSERT_TRUE(loaded.has_value()) << mf::to_string(loaded.error());
  const Eigen::VectorXd &limits = loaded->model->limits();
  for (const int limit : {mf::mf5x::FZMIN, mf::mf5x::FZMAX, mf::mf5x::KPUMAX, mf::mf5x::ALPMAX, mf::mf5x::CAMMAX}) {
    ASSERT_TRUE(std::isfinite(limits(limit))) << "limit " << limit;
  }

  // Rows 2..6 of the output are the table lines 5..9; every one is evaluated at the limit it exceeds (V19).
  const std::vector<mf::test::point> at_limits{
      mf::test::point{
          .fz_n = 4000.0, .kappa = limits(mf::mf5x::KPUMAX), .alpha_rad = 0.0, .gamma_rad = 0.0, .vx_mps = 10.0},
      mf::test::point{
          .fz_n = 4000.0, .kappa = 0.0, .alpha_rad = limits(mf::mf5x::ALPMAX), .gamma_rad = 0.0, .vx_mps = 10.0},
      mf::test::point{
          .fz_n = 4000.0, .kappa = 0.0, .alpha_rad = 0.0, .gamma_rad = limits(mf::mf5x::CAMMAX), .vx_mps = 10.0},
      mf::test::point{
          .fz_n = limits(mf::mf5x::FZMAX), .kappa = 0.1, .alpha_rad = 0.0, .gamma_rad = 0.0, .vx_mps = 10.0},
      mf::test::point{
          .fz_n = limits(mf::mf5x::FZMIN), .kappa = 0.1, .alpha_rad = 0.0, .gamma_rad = 0.0, .vx_mps = 10.0},
  };
  const std::vector<result_row> rows = rows_of(result.out);
  ASSERT_EQ(rows.size(), 6U);
  // The inputs are echoed unclamped.
  expect_inputs(rows[1], 4000.0, 0.0, 10.0, 0.0, 10.0);
  expect_inputs(rows[2], 4000.0, 2.0, 0.0, 0.0, 10.0);
  expect_inputs(rows[3], 4000.0, 0.0, 0.0, 0.5, 10.0);
  expect_inputs(rows[4], 20000.0, 0.0, 0.1, 0.0, 10.0);
  expect_inputs(rows[5], 1e-9, 0.0, 0.1, 0.0, 10.0);
  // The CLI evaluates as a batch, the expected values per point: they agree within the batch tolerance.
  constexpr double force_scale_n = 4000.0;
  constexpr double moment_scale_nm = 1200.0;
  for (std::size_t index = 0; index < at_limits.size(); ++index) {
    const Eigen::VectorXd expected = loaded->model->evaluate(mf::test::input_of(at_limits[index]));
    const result_row &row = rows[index + 1];
    EXPECT_TRUE(near_scaled(row.fx, expected(mf::mf5x::FX), mf::test::BATCH_REL_TOL, force_scale_n))
        << "FX row " << index;
    EXPECT_TRUE(near_scaled(row.fy, expected(mf::mf5x::FY), mf::test::BATCH_REL_TOL, force_scale_n))
        << "FY row " << index;
    EXPECT_TRUE(near_scaled(row.mz, expected(mf::mf5x::MZ), mf::test::BATCH_REL_TOL, moment_scale_nm))
        << "MZ row " << index;
    EXPECT_TRUE(near_scaled(row.mx, expected(mf::mf5x::MX), mf::test::BATCH_REL_TOL, moment_scale_nm))
        << "MX row " << index;
    EXPECT_TRUE(near_scaled(row.my, expected(mf::mf5x::MY), mf::test::BATCH_REL_TOL, moment_scale_nm))
        << "MY row " << index;
  }
}

TEST(cli_run, Run_LowSpeedPac2002_WarnsVxlowOnce) {
  const run_result result = run_files("inputs_low_speed.csv", "pac2002_S.tir");
  EXPECT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  const std::string table = data_file("inputs_low_speed.csv").string();
  EXPECT_TRUE(result.err.contains(table + ":4: VX below VXLOW in 2 row(s); VXLOW is not applied\n")) << result.err;
  // The identification warning and the VXLOW warning.
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 2U) << result.err;
  EXPECT_EQ(rows_of(result.out).size(), 2U);
}

TEST(cli_run, Run_LowSpeedMf52_ErrorAtLine5) {
  const run_result result = run_files("inputs_low_speed.csv", "mf52_S52.tir");
  expect_failed_run(result);
  const std::string table = data_file("inputs_low_speed.csv").string();
  EXPECT_TRUE(result.err.contains(table + ":5")) << result.err;
  EXPECT_TRUE(result.err.contains("VX = 0 m/s")) << result.err;
}

TEST(cli_run, Run_NegativeVx_ErrorNamesLine5) {
  const run_result result = run_files("inputs_negative_vx.csv", "pac2002_S.tir");
  expect_failed_run(result);
  EXPECT_TRUE(result.err.contains(data_file("inputs_negative_vx.csv").string() + ":5")) << result.err;
  EXPECT_TRUE(result.err.contains("VX = -1 m/s")) << result.err;
}

TEST(cli_run, Run_MalformedTable_ErrorNamesLine5) {
  const run_result result = run_files("inputs_malformed.csv", "pac2002_S.tir");
  expect_failed_run(result);
  EXPECT_TRUE(result.err.contains(data_file("inputs_malformed.csv").string() + ":5")) << result.err;
}

namespace {
  /// An input table that the loader must reject, the line of the error and a fragment of its message.
  struct bad_table_case {
    std::string_view file;
    std::size_t line;
    std::string message;
  };

  /// Prints the member values (GoogleTest would otherwise print the raw bytes, uninitialised padding included).
  // NOLINTNEXTLINE(readability-identifier-naming): GoogleTest finds the printer by the name PrintTo
  void PrintTo(const bad_table_case &param, std::ostream *os) {
    *os << param.file << ":" << param.line << ": " << param.message;
  }

  class cli_bad_input_table : public ::testing::TestWithParam<bad_table_case> {};

  /// The unit hint of the parser messages for the columns FZ SA SR IA VX.
  const std::string UNIT_HINT = "FZ: N, SA: rad|deg, SR: -, IA: rad|deg, VX: m/s";
} // namespace

TEST_P(cli_bad_input_table, Run_BadInputTable_Exit1AtLine) {
  const bad_table_case &param = GetParam();
  const run_result result = run_files(param.file, "pac2002_S.tir");
  expect_failed_run(result);
  const std::string expected =
      "mf: error: " + data_file(param.file).string() + ":" + std::to_string(param.line) + ": " + param.message + "\n";
  EXPECT_EQ(result.err, expected);
}

INSTANTIATE_TEST_SUITE_P(
    cases, cli_bad_input_table,
    ::testing::Values(
        bad_table_case{.file = "inputs_legacy_brackets.csv",
                       .line = 2,
                       .message = "column header 'FZ[N]' uses the legacy NAME[unit] form; write the column name alone "
                                  "and give its unit on the unit line (the line after the header)"},
        bad_table_case{.file = "inputs_no_unit_line.csv",
                       .line = 3,
                       .message = "missing unit line: the line after the header (line 2) starts with the number "
                                  "'4000'; it must give one unit per column (" +
                                  UNIT_HINT + ")"},
        bad_table_case{
            .file = "inputs_header_only.csv",
            .line = 2,
            .message = "missing unit line: the header must be followed by a line with one unit per column (" +
                       UNIT_HINT + ")"},
        bad_table_case{.file = "inputs_unit_count.csv",
                       .line = 3,
                       .message = "unit line: expected 5 units, one per header column, got 4 (" + UNIT_HINT + ")"},
        bad_table_case{.file = "inputs_unknown_unit.csv",
                       .line = 3,
                       .message = "unsupported unit 'kN' for column FZ (supported: N)"},
        bad_table_case{.file = "inputs_wrong_unit_column.csv",
                       .line = 3,
                       .message = "unsupported unit 'deg' for column FZ (supported: N)"}));

TEST(cli_run, Run_PartialCombined_MissingKeyRhx1) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S_part.tir");
  expect_failed_run(result);
  EXPECT_TRUE(result.err.contains("RHX1")) << result.err;
  EXPECT_TRUE(result.err.contains(data_file("pac2002_S_part.tir").string())) << result.err;
}

TEST(cli_run, Run_Mf61_Exit1) {
  const run_result result = run_files("inputs_s_vectors.csv", "mf61_unsupported.tir");
  expect_failed_run(result);
  EXPECT_TRUE(result.err.contains("FITTYP 61")) << result.err;
}

TEST(cli_run, Run_NonexistentTir_Exit1) {
  const run_result result = run_files("inputs_s_vectors.csv", "does_not_exist.tir");
  expect_failed_run(result);
  EXPECT_TRUE(result.err.contains(data_file("does_not_exist.tir").string())) << result.err;
}

TEST(cli_run, Run_NonexistentTable_Exit1) {
  const run_result result = run_files("does_not_exist.csv", "pac2002_S.tir");
  expect_failed_run(result);
  EXPECT_TRUE(result.err.contains(data_file("does_not_exist.csv").string())) << result.err;
}

TEST(cli_run, Run_NoArguments_Exit2WithUsageOnErr) {
  const run_result result = run_cli({});
  expect_usage_run(result, "expected 2 arguments <inputs-table> <file.tir>, got 0\n");
}

TEST(cli_run, Run_UnknownOption_Exit2) {
  const run_result result = run_cli(file_arguments("inputs_s_vectors.csv", "pac2002_S.tir", {"--bogus"}));
  expect_usage_run(result, "unknown option '--bogus'\n");
}

TEST(cli_run, Run_Help_Exit0UsageOnOut) {
  const run_result result = run_cli({"--help"});
  EXPECT_EQ(result.code, mf::cli::EXIT_OK);
  EXPECT_EQ(result.out, std::string(mf::cli::usage()));
  EXPECT_TRUE(result.err.empty()) << result.err;
}

TEST(cli_run, Run_Version_Exit0) {
  const run_result result = run_cli({"--version"});
  EXPECT_EQ(result.code, mf::cli::EXIT_OK);
  EXPECT_EQ(result.out, "mf " + std::string(mf::version()) + "\n");
  EXPECT_TRUE(result.err.empty()) << result.err;
}

TEST(cli_run, Run_EmptyArgv_Exit2) {
  std::ostringstream out;
  std::ostringstream err;
  const int code = mf::cli::run(std::span<const std::string_view>{}, out, err);
  EXPECT_EQ(code, mf::cli::EXIT_USAGE);
  EXPECT_TRUE(out.str().empty()) << out.str();
  const std::string message = err.str();
  EXPECT_TRUE(message.starts_with("mf: error: expected 2 arguments <inputs-table> <file.tir>, got 0\n")) << message;
}

TEST(cli_run, Run_FailingOutputStream_ReportsCannotWrite) {
  failing_buffer buffer;
  std::ostream out(&buffer);
  std::ostringstream err;
  // A table and a model without any warning, so that stderr holds only the output failure.
  const std::vector<std::string> arguments = file_arguments("inputs_sae.csv", "pac2002_S_sae.tir");
  std::vector<std::string_view> argv{"mf"};
  argv.insert(argv.end(), arguments.begin(), arguments.end());
  const int code = mf::cli::run(argv, out, err);
  EXPECT_EQ(code, mf::cli::EXIT_ERROR);
  EXPECT_TRUE(out.fail());
  EXPECT_EQ(err.str(), "mf: error: cannot write results\n");
}

TEST(cli_run, Run_ThrowingOutputStream_CaughtExit1) {
  failing_buffer buffer;
  std::ostream out(&buffer);
  out.exceptions(std::ios::badbit);
  std::ostringstream err;
  const std::vector<std::string> arguments = file_arguments("inputs_sae.csv", "pac2002_S_sae.tir");
  std::vector<std::string_view> argv{"mf"};
  argv.insert(argv.end(), arguments.begin(), arguments.end());
  const int code = mf::cli::run(argv, out, err);
  EXPECT_EQ(code, mf::cli::EXIT_ERROR);
  EXPECT_TRUE(err.str().starts_with("mf: error: ")) << err.str();
  EXPECT_NE(err.str(), "mf: error: cannot write results\n");
}

namespace {
  /// The line has exactly FIELD_COUNT comma-separated fields.
  bool has_field_count(const std::string &line) {
    return count_of(line, ",") + 1U == FIELD_COUNT;
  }
} // namespace

TEST(cli, Run_Header_TwentyThreeColumnsExact) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S.tir");
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  const std::vector<std::string> lines = lines_of(result.out);
  ASSERT_EQ(lines.size(), 7U);
  EXPECT_EQ(lines[0], EXPECTED_HEADER);
  EXPECT_EQ(lines[0], mf::io::results_header(mf::mf5x::PAC2002_LAYOUT));
  EXPECT_EQ(count_of(lines[0], ",") + 1U, FIELD_COUNT);
  EXPECT_TRUE(std::ranges::all_of(lines, has_field_count)) << result.out;

  const run_result mf52 = run_files("inputs_s_vectors.csv", "mf52_S52.tir");
  ASSERT_EQ(mf52.code, mf::cli::EXIT_OK) << mf52.err;
  EXPECT_EQ(lines_of(mf52.out).front(), EXPECTED_HEADER);
  EXPECT_EQ(lines_of(mf52.out).front(), mf::io::results_header(mf::mf5x::MF52_LAYOUT));
}

TEST(cli, Run_RangesFixture_FlagColumnsMatchWarnings) {
  const run_result result = run_files("inputs_ranges.csv", "pac2002_S_rng.tir");
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  const std::string table = data_file("inputs_ranges.csv").string();
  // The warnings are unchanged and appear in the order FZ, SR, SA, IA.
  const std::string sr = table + ":6: SR outside the property-file range in 1 row(s); evaluated at the limit\n";
  const std::string sa = table + ":7: SA outside the property-file range in 1 row(s); evaluated at the limit\n";
  const std::string ia = table + ":8: IA outside the property-file range in 1 row(s); evaluated at the limit\n";
  const std::string fz = table + ":9: FZ outside the property-file range in 2 row(s); evaluated at the limit\n";
  EXPECT_TRUE(result.err.contains(sr)) << result.err;
  EXPECT_TRUE(result.err.contains(sa)) << result.err;
  EXPECT_TRUE(result.err.contains(ia)) << result.err;
  EXPECT_TRUE(result.err.contains(fz)) << result.err;
  // The four range warnings and the identification warning.
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 5U) << result.err;
  const std::size_t at_fz = result.err.find(fz);
  const std::size_t at_sr = result.err.find(sr);
  const std::size_t at_sa = result.err.find(sa);
  const std::size_t at_ia = result.err.find(ia);
  EXPECT_LT(at_fz, at_sr);
  EXPECT_LT(at_sr, at_sa);
  EXPECT_LT(at_sa, at_ia);

  const std::vector<result_row> rows = rows_of(result.out);
  ASSERT_EQ(rows.size(), 6U);
  // Row 0 is inside every range; rows 1..3 exceed SR, SA and IA; rows 4 and 5 exceed FZ.
  expect_flags(rows[0], false, false, false, false, false);
  expect_flags(rows[1], false, true, false, false, false);
  expect_flags(rows[2], false, false, true, false, false);
  expect_flags(rows[3], false, false, false, true, false);
  expect_flags(rows[4], true, false, false, false, false);
  expect_flags(rows[5], true, false, false, false, false);
}

TEST(cli, Run_LowSpeedFixture_BelowVxlowColumnOne) {
  const run_result result = run_files("inputs_low_speed.csv", "pac2002_S.tir");
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  const std::vector<result_row> rows = rows_of(result.out);
  ASSERT_EQ(rows.size(), 2U);
  expect_flags(rows[0], false, false, false, false, true);
  expect_flags(rows[1], false, false, false, false, true);
  EXPECT_TRUE(result.err.contains("VX below VXLOW in 2 row(s); VXLOW is not applied\n")) << result.err;
}

TEST(cli, Run_PressureColumn_Pac2002Wording) {
  const run_result result = run_files("inputs_pressure.csv", "pac2002_S.tir");
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  const std::string table = data_file("inputs_pressure.csv").string();
  expect_mixed_warning(result.err);
  EXPECT_TRUE(result.err.ends_with("mf: warning: " + table + ": " + std::string(PRESSURE_WARNING) + "\n"))
      << result.err;
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 2U) << result.err;
  const std::vector<result_row> rows = rows_of(result.out);
  ASSERT_EQ(rows.size(), 1U);
  expect_no_flags(rows[0]);
}

TEST(cli, Run_PressureColumn_Mf52Wording) {
  const run_result result = run_files("inputs_pressure.csv", "mf52_S52.tir");
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  const std::string table = data_file("inputs_pressure.csv").string();
  EXPECT_TRUE(result.err.contains(table + ": " + std::string(PRESSURE_WARNING_MF52) + "\n")) << result.err;
  EXPECT_EQ(count_of(result.err, PRESSURE_WARNING_MF52), 1U) << result.err;
  EXPECT_EQ(count_of(result.err, PRESSURE_WARNING), 0U) << result.err;
  // The other warnings are the load warning of the file (PAC2002-only camber factors) and the identification warning.
  expect_mixed_warning(result.err);
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 3U) << result.err;
  const std::vector<result_row> rows = rows_of(result.out);
  ASSERT_EQ(rows.size(), 1U);
  expect_no_flags(rows[0]);
}

TEST(cli, Run_PressureColumnAndMixedFz_IgnoredColumnWarningBeforeLoadSideWarning) {
  const run_result result = run_files("inputs_pressure_mixed_fz.csv", "pac2002_S.tir");
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  const std::string table = data_file("inputs_pressure_mixed_fz.csv").string();
  const std::string pressure = "mf: warning: " + table + ": " + std::string(PRESSURE_WARNING) + "\n";
  const std::string load_side = mixed_iso_fz_warning("inputs_pressure_mixed_fz.csv", 5);
  const std::size_t identification = result.err.find(MIXED_WARNING_START);
  const std::size_t pressure_at = result.err.find(pressure);
  const std::size_t load_side_at = result.err.find(load_side);
  ASSERT_NE(identification, std::string::npos) << result.err;
  ASSERT_NE(pressure_at, std::string::npos) << result.err;
  ASSERT_NE(load_side_at, std::string::npos) << result.err;
  EXPECT_LT(identification, pressure_at) << result.err;
  EXPECT_LT(pressure_at, load_side_at) << result.err;
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 3U) << result.err;
}

TEST(cli, Run_NegativeVx_DomainErrorAtLine_NoStdout) {
  const run_result result = run_files("inputs_negative_vx.csv", "pac2002_S.tir");
  EXPECT_EQ(result.code, mf::cli::EXIT_ERROR);
  EXPECT_TRUE(result.out.empty()) << result.out;
  const std::string table = data_file("inputs_negative_vx.csv").string();
  EXPECT_EQ(result.err, "mf: error: " + table + ":5: VX = -1 m/s is outside the PAC2002 domain (VX >= 0)\n");
}

TEST(cli, Run_Mf52ZeroVx_DomainErrorAtLine_NoStdout) {
  const run_result result = run_files("inputs_low_speed.csv", "mf52_S52.tir");
  EXPECT_EQ(result.code, mf::cli::EXIT_ERROR);
  EXPECT_TRUE(result.out.empty()) << result.out;
  const std::string table = data_file("inputs_low_speed.csv").string();
  EXPECT_EQ(result.err, "mf: error: " + table + ":5: VX = 0 m/s is outside the MF 5.2 domain (VX > 0)\n");
}

TEST(cli, Run_OverflowFz_NumericalErrorAtLine) {
  const run_result result = run_files("inputs_overflow.csv", "pac2002_S.tir");
  expect_failed_run(result);
  const std::string table = data_file("inputs_overflow.csv").string();
  // Data row 2 is file line 5.
  const std::string prefix = "mf: error: " + table + ":5: non-finite ";
  EXPECT_TRUE(result.err.starts_with(prefix)) << result.err;
  // The message names the first non-finite output channel.
  constexpr std::array<std::string_view, 5> channels{"FX", "FY", "MZ", "MX", "MY"};
  const auto names_channel = [&result, &prefix](std::string_view channel) {
    return result.err.starts_with(std::string(prefix).append(channel).append(" output ("));
  };
  EXPECT_TRUE(std::ranges::any_of(channels, names_channel)) << result.err;
}

TEST(cli, Run_Uncombined_PureSlipValuesAndFlags) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S.tir", {"--uncombined"});
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  expect_mixed_warning(result.err);
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 2U) << result.err;
  const std::vector<result_row> rows = rows_of(result.out);
  ASSERT_EQ(rows.size(), 6U);
  // Pure Fx0 of V2 and pure Fy0 / Mz0 of V3, whatever the other slip of the row.
  EXPECT_NEAR(rows[0].fx, 3935.48, printed_tol(2));
  EXPECT_NEAR(rows[1].fy, -2376.40, printed_tol(2));
  EXPECT_NEAR(rows[1].mz, 60.46, printed_tol(2));
  EXPECT_NEAR(rows[2].fx, 3935.48, printed_tol(2));
  EXPECT_NEAR(rows[3].fy, -2376.40, printed_tol(2));
  // Fz <= 0 still gives zero outputs.
  expect_zero_outputs(rows[4]);
  expect_zero_outputs(rows[5]);
  // The loaded rows are inside every range and above VXLOW.
  for (std::size_t index = 0; index < 4; ++index) {
    expect_no_flags(rows[index]);
  }
}

TEST(cli, Run_Verbose_InfoBeforeIgnoredColumnWarning) {
  const run_result result = run_files("inputs_pressure.csv", "pac2002_S.tir", {"--verbose"});
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  const std::size_t last_info = result.err.rfind("mf: info: ");
  const std::size_t column_warning = result.err.find(std::string(PRESSURE_WARNING));
  ASSERT_NE(last_info, std::string::npos) << result.err;
  ASSERT_NE(column_warning, std::string::npos) << result.err;
  // The identification warning precedes the first info line.
  EXPECT_TRUE(result.err.contains("\nmf: info: model PAC2002\n")) << result.err;
  EXPECT_LT(last_info, column_warning) << result.err;
  EXPECT_EQ(count_of(result.err, "mf: info: "), 3U + 1U + 29U + 1U) << result.err;
  // The identification warning and the pressure warning.
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 2U) << result.err;
}

TEST(cli_run, Run_SaeModelSaeTable_NoCoordinateOrLoadSideWarnings) {
  const run_result result = run_files("inputs_sae.csv", "pac2002_S_sae.tir");
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  EXPECT_FALSE(result.err.contains("coordinate system:")) << result.err;
  EXPECT_FALSE(result.err.contains("lift-off")) << result.err;
  EXPECT_TRUE(result.err.empty()) << result.err;
  const std::vector<result_row> rows = rows_of(result.out);
  ASSERT_EQ(rows.size(), 4U);
  // The table is echoed as read and the loaded SAE tire produces forces.
  expect_inputs(rows[0], -4000.0, 0.0, 0.1, 0.0, 10.0);
  expect_inputs(rows[1], -4000.0, 0.05, 0.0, 0.0, 10.0);
  EXPECT_NE(bits(rows[0].fx), bits(0.0));
  EXPECT_NE(bits(rows[1].fy), bits(0.0));
}

TEST(cli_run, Run_SaeModelIsoTable_WarnsLoadSideSaeAtFirstPositiveLine) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S_sae.tir");
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  EXPECT_EQ(result.err, "mf: warning: " + data_file("inputs_s_vectors.csv").string() +
                            ":5: FZ > 0 in 4 row(s) of an SAE table: a loaded tire has FZ < 0 in SAE; these rows are "
                            "lift-off (zero forces and moments) (the table mixes FZ signs)\n");
  const std::vector<result_row> rows = rows_of(result.out);
  ASSERT_EQ(rows.size(), 6U);
  // The four positive rows and the zero row are lift-offs of an SAE model; FZ = -100 is loaded.
  for (std::size_t index = 0; index < 5; ++index) {
    expect_zero_outputs(rows[index]);
  }
  EXPECT_EQ(bits(rows[5].fz), bits(-100.0));
  EXPECT_NE(bits(std::abs(rows[5].fy)), bits(0.0));
}

TEST(cli_run, Run_NonStdExceptionFromOutputStream_CaughtExit1) {
  non_std_throwing_buffer buffer;
  std::ostream out(&buffer);
  out.exceptions(std::ios::badbit);
  std::ostringstream err;
  const std::vector<std::string> arguments = file_arguments("inputs_sae.csv", "pac2002_S_sae.tir");
  std::vector<std::string_view> argv{"mf"};
  argv.insert(argv.end(), arguments.begin(), arguments.end());
  const int code = mf::cli::run(argv, out, err);
  EXPECT_EQ(code, mf::cli::EXIT_ERROR);
  EXPECT_EQ(err.str(), "mf: error: unknown exception\n");
}

namespace {
  /// Scale of the force columns of the S-family fixtures [N] for the scaled comparisons.
  constexpr double FORCE_SCALE_N = 4000.0;

  /// Scale of the moment columns [N*m].
  constexpr double MOMENT_SCALE_NM = 4000.0 * 0.3;

  /// `got` equals `want` with the vertical transform applied by `vertical_sign` (+1: same system, -1: the other one):
  /// FZ, FY, MZ and MY are multiplied by it, SA, SR, IA, VX, FX, MX and the flags are equal. Inputs and flags compare
  /// with ==, outputs within the scaled batch tolerance (the sign of an exact zero may differ between the paths).
  void expect_row_matches(const result_row &got, const result_row &want, double vertical_sign, std::size_t index) {
    constexpr double tol = 1e-12;
    EXPECT_EQ(got.fz, vertical_sign * want.fz) << "FZ row " << index;
    EXPECT_EQ(got.sa, want.sa) << "SA row " << index;
    EXPECT_EQ(got.sr, want.sr) << "SR row " << index;
    EXPECT_EQ(got.ia, want.ia) << "IA row " << index;
    EXPECT_EQ(got.vx, want.vx) << "VX row " << index;
    EXPECT_TRUE(near_scaled(got.fx, want.fx, tol, FORCE_SCALE_N)) << "FX row " << index;
    EXPECT_TRUE(near_scaled(got.fy, vertical_sign * want.fy, tol, FORCE_SCALE_N)) << "FY row " << index;
    EXPECT_TRUE(near_scaled(got.mz, vertical_sign * want.mz, tol, MOMENT_SCALE_NM)) << "MZ row " << index;
    EXPECT_TRUE(near_scaled(got.mx, want.mx, tol, MOMENT_SCALE_NM)) << "MX row " << index;
    EXPECT_TRUE(near_scaled(got.my, vertical_sign * want.my, tol, MOMENT_SCALE_NM)) << "MY row " << index;
    EXPECT_EQ(got.fz_clamped, want.fz_clamped) << "FZ_CLAMPED row " << index;
    EXPECT_EQ(got.sr_clamped, want.sr_clamped) << "SR_CLAMPED row " << index;
    EXPECT_EQ(got.sa_clamped, want.sa_clamped) << "SA_CLAMPED row " << index;
    EXPECT_EQ(got.ia_clamped, want.ia_clamped) << "IA_CLAMPED row " << index;
    EXPECT_EQ(got.below_vxlow, want.below_vxlow) << "BELOW_VXLOW row " << index;
  }

  /// The first `count` data rows of both runs match as `expect_row_matches` defines.
  void expect_runs_match(const run_result &got, const run_result &want, double vertical_sign, std::size_t count) {
    const std::vector<result_row> got_rows = rows_of(got.out);
    const std::vector<result_row> want_rows = rows_of(want.out);
    ASSERT_GE(got_rows.size(), count);
    ASSERT_GE(want_rows.size(), count);
    for (std::size_t index = 0; index < count; ++index) {
      expect_row_matches(got_rows[index], want_rows[index], vertical_sign, index);
    }
  }

  /// Bit pattern of `value` with -0 folded to +0 (adding +0.0 turns -0 into +0 and changes nothing else).
  std::uint64_t zero_normalised_bits(double value) {
    return std::bit_cast<std::uint64_t>(value + 0.0);
  }

  /// The first `count` data rows of both runs are bit-equal field by field, +0 and -0 counting as equal.
  void expect_runs_bit_equal(const run_result &got, const run_result &want, std::size_t count) {
    const std::vector<result_row> got_rows = rows_of(got.out);
    const std::vector<result_row> want_rows = rows_of(want.out);
    ASSERT_GE(got_rows.size(), count);
    ASSERT_GE(want_rows.size(), count);
    for (std::size_t index = 0; index < count; ++index) {
      const result_row &g = got_rows[index];
      const result_row &w = want_rows[index];
      EXPECT_EQ(zero_normalised_bits(g.fz), zero_normalised_bits(w.fz)) << "FZ row " << index;
      EXPECT_EQ(zero_normalised_bits(g.sa), zero_normalised_bits(w.sa)) << "SA row " << index;
      EXPECT_EQ(zero_normalised_bits(g.sr), zero_normalised_bits(w.sr)) << "SR row " << index;
      EXPECT_EQ(zero_normalised_bits(g.ia), zero_normalised_bits(w.ia)) << "IA row " << index;
      EXPECT_EQ(zero_normalised_bits(g.vx), zero_normalised_bits(w.vx)) << "VX row " << index;
      EXPECT_EQ(zero_normalised_bits(g.fx), zero_normalised_bits(w.fx)) << "FX row " << index;
      EXPECT_EQ(zero_normalised_bits(g.fy), zero_normalised_bits(w.fy)) << "FY row " << index;
      EXPECT_EQ(zero_normalised_bits(g.mz), zero_normalised_bits(w.mz)) << "MZ row " << index;
      EXPECT_EQ(zero_normalised_bits(g.mx), zero_normalised_bits(w.mx)) << "MX row " << index;
      EXPECT_EQ(zero_normalised_bits(g.my), zero_normalised_bits(w.my)) << "MY row " << index;
      EXPECT_EQ(g.fz_clamped, w.fz_clamped) << "FZ_CLAMPED row " << index;
      EXPECT_EQ(g.sr_clamped, w.sr_clamped) << "SR_CLAMPED row " << index;
      EXPECT_EQ(g.sa_clamped, w.sa_clamped) << "SA_CLAMPED row " << index;
      EXPECT_EQ(g.ia_clamped, w.ia_clamped) << "IA_CLAMPED row " << index;
      EXPECT_EQ(g.below_vxlow, w.below_vxlow) << "BELOW_VXLOW row " << index;
    }
  }
} // namespace

TEST(cli_run, Run_IsoModelSaeTable_ThreeWaysAgree) {
  const run_result sae_model = run_files("inputs_sae.csv", "pac2002_S_sae.tir");
  const run_result converted = run_files("inputs_sae.csv", "pac2002_S_iso.tir", {"--system=sae"});
  const run_result table_converted = run_files("inputs_sae.csv", "pac2002_S_iso.tir", {"--table-system=sae"});
  for (const run_result *result : {&sae_model, &converted, &table_converted}) {
    ASSERT_EQ(result->code, mf::cli::EXIT_OK) << result->err;
    EXPECT_FALSE(result->err.contains("coordinate system:")) << result->err;
    EXPECT_FALSE(result->err.contains("lift-off")) << result->err;
    EXPECT_EQ(lines_of(result->out).size(), 5U);
    EXPECT_EQ(lines_of(result->out).front(), lines_of(sae_model.out).front());
  }
  EXPECT_EQ(converted.out, sae_model.out);
  expect_runs_bit_equal(table_converted, sae_model, 4);
}

TEST(cli_run, Run_SaeModel_FlipsFyMzMyVsIsoModel) {
  const run_result sae = run_files("inputs_sae.csv", "pac2002_S_sae.tir");
  const run_result iso = run_files("inputs_s_vectors.csv", "pac2002_S_iso.tir");
  ASSERT_EQ(sae.code, mf::cli::EXIT_OK) << sae.err;
  ASSERT_EQ(iso.code, mf::cli::EXIT_OK) << iso.err;
  expect_runs_match(sae, iso, -1.0, 4);
}

TEST(cli_run, Run_MixedModel_WarnsMixed) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S.tir");
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  EXPECT_EQ(count_of(result.err, MIXED_WARNING), 1U) << result.err;
}

TEST(cli_run, Run_MixedModelSystemIso_NoMixedWarning) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S.tir", {"--system=iso"});
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  EXPECT_FALSE(result.err.contains("mix conventions")) << result.err;
  const std::vector<result_row> rows = rows_of(result.out);
  ASSERT_EQ(rows.size(), 6U);
  EXPECT_EQ(rows[1].sa, 0.05);
  EXPECT_EQ(rows[1].fz, 4000.0);
  EXPECT_GT(rows[1].fy, 0.0);
}

TEST(cli_run, Run_IsoModelNegativeFz_WarnsLoadSideAtLine) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S_iso.tir");
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  EXPECT_TRUE(result.err.contains(mixed_iso_fz_warning("inputs_s_vectors.csv", 10))) << result.err;
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 1U) << result.err;
}

TEST(cli_run, Run_Verbose_PrintsIdentificationAndReadings) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S_iso.tir", {"--verbose"});
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  EXPECT_TRUE(result.err.contains("mf: info: coordinate system of the model: ISO; vertical POSITIVE (DECIDED, "
                                  "confidence 1.00); lateral POSITIVE (DECIDED, confidence 1.00)\n"))
      << result.err;
  EXPECT_EQ(count_of(result.err, "mf: info: coordinate indicator "), 29U) << result.err;
  EXPECT_TRUE(result.err.contains("mf: info: coordinate indicator FZ0 at 1 x F'z0 (4000 N): 4000 N, normalised 1, "
                                  "VOTE, vote +1, weight 1\n"))
      << result.err;
  EXPECT_TRUE(result.err.contains("mf: info: table in ISO; the model is loaded at FZ > 0\n")) << result.err;
  EXPECT_FALSE(result.err.contains("model converted")) << result.err;
}

TEST(cli_run, Run_VerboseSystemSae_PrintsConversionAndTableLines) {
  const run_result result = run_files("inputs_sae.csv", "pac2002_S_iso.tir", {"--verbose", "--system=sae"});
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  EXPECT_TRUE(
      result.err.contains("mf: info: model converted to SAE (vertical transform: yes, lateral transform: no)\n"))
      << result.err;
  EXPECT_TRUE(result.err.contains("mf: info: table in SAE; the model is loaded at FZ < 0\n")) << result.err;
}

TEST(cli_run, Run_SystemSameAsModel_EmptyPlanByteIdenticalStdout) {
  const run_result plain = run_files("inputs_s_vectors.csv", "pac2002_S_iso.tir");
  const run_result same = run_files("inputs_s_vectors.csv", "pac2002_S_iso.tir", {"--verbose", "--system=iso"});
  ASSERT_EQ(plain.code, mf::cli::EXIT_OK) << plain.err;
  ASSERT_EQ(same.code, mf::cli::EXIT_OK) << same.err;
  EXPECT_TRUE(same.err.contains("model converted to ISO (vertical transform: no, lateral transform: no)")) << same.err;
  EXPECT_EQ(same.out, plain.out);
}

TEST(cli_run, Run_MixedModelSystemSae_BothTransforms) {
  const run_result result = run_files("inputs_sae.csv", "pac2002_S.tir", {"--verbose", "--system=sae"});
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  EXPECT_TRUE(result.err.contains("(vertical transform: yes, lateral transform: yes)")) << result.err;
}

TEST(cli_run, Run_TableSystemIsoOnSaeModel_ConvertsData) {
  const run_result converted = run_files("inputs_s_vectors.csv", "pac2002_S_sae.tir", {"--table-system=iso"});
  const run_result iso = run_files("inputs_s_vectors.csv", "pac2002_S_iso.tir");
  ASSERT_EQ(converted.code, mf::cli::EXIT_OK) << converted.err;
  ASSERT_EQ(iso.code, mf::cli::EXIT_OK) << iso.err;
  expect_runs_match(converted, iso, 1.0, 6);
  EXPECT_TRUE(converted.err.contains(mixed_iso_fz_warning("inputs_s_vectors.csv", 10))) << converted.err;
  EXPECT_TRUE(iso.err.contains(mixed_iso_fz_warning("inputs_s_vectors.csv", 10))) << iso.err;
}

TEST(cli_run, Run_InvalidSystem_Exit2) {
  const run_result result = run_cli({"--system=xyz", "a.csv", "b.tir"});
  expect_usage_run(result, "invalid value 'xyz' for --system (expected iso or sae)\n");
}

TEST(cli_run, Run_InvalidTableSystem_Exit2) {
  const run_result result = run_cli({"--table-system=xyz", "a.csv", "b.tir"});
  expect_usage_run(result, "invalid value 'xyz' for --table-system (expected iso or sae)\n");
}

namespace {
  /// The eight printed characteristics of a row, as the digits of the reference tables ("nan" for an undefined peak).
  using printed_row = std::array<std::string_view, CHARACTERISTIC_COUNT>;

  /// A row without contact: six zeros, then two undefined peaks.
  constexpr printed_row NO_CONTACT{"0", "0", "0", "0", "0", "0", "nan", "nan"};

  /// The four loaded rows of inputs_s_vectors.csv (V2, V3, V12, V13) followed by the two lift-off rows, for one run.
  using printed_run = std::array<printed_row, 6>;

  /// Position of a characteristic in a result line.
  constexpr std::size_t field_index(std::size_t characteristic) {
    return FIRST_CHARACTERISTIC + characteristic;
  }

  /// `actual` equals the printed reference `text` within half a unit of its last digit plus a 5e-10 relative allowance
  /// for the iterated peak slips; "nan" needs a NaN and "0" an exact zero (of either sign).
  void expect_printed(double actual, std::string_view text, const std::string &label) {
    if (text == "nan") {
      EXPECT_TRUE(std::isnan(actual)) << label << ": expected nan, got " << actual;
      return;
    }
    const std::optional<double> parsed = mf::io::parse_number(text);
    ASSERT_TRUE(parsed.has_value()) << label << ": bad reference '" << text << "'";
    const double expected = parsed.value_or(0.0);
    if (expected == 0.0) {
      EXPECT_EQ(actual, 0.0) << label;
      return;
    }
    // A reference without a decimal point is an exactly representable integer such as 4000.
    const std::size_t point = text.find('.');
    const int decimals = point == std::string_view::npos ? 9 : static_cast<int>(text.size() - point - 1);
    const double tolerance = printed_tol(decimals) + (5e-10 * std::abs(expected));
    EXPECT_NEAR(actual, expected, tolerance) << label;
  }

  /// The eight characteristics of `row` equal the printed reference `want`.
  void expect_printed_row(const result_row &row, const printed_row &want, const std::string &label) {
    for (std::size_t index = 0; index < CHARACTERISTIC_COUNT; ++index) {
      expect_printed(row.characteristics.at(index), want.at(index), label + " characteristic " + std::to_string(index));
    }
  }

  /// A successful run whose six rows have the printed characteristics `want`; "-nan" never appears.
  void expect_printed_run(const run_result &result, const printed_run &want, const std::string &label) {
    ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
    EXPECT_FALSE(result.out.contains("-nan")) << result.out;
    const std::vector<result_row> rows = rows_of(result.out);
    ASSERT_EQ(rows.size(), want.size());
    for (std::size_t index = 0; index < want.size(); ++index) {
      expect_printed_row(rows[index], want.at(index), label + " row " + std::to_string(index));
    }
  }

  /// The characteristics of `got` equal those of `want` multiplied by `signs`, within a 1e-10 scaled tolerance; a NaN
  /// of `want` needs a NaN in `got`.
  void expect_characteristics_match(const result_row &got, const result_row &want,
                                    const std::array<double, CHARACTERISTIC_COUNT> &signs, std::size_t row) {
    constexpr double tol = 1e-10;
    constexpr std::array<double, CHARACTERISTIC_COUNT> scales{5e4, 1e5, 5e4, 1e5, 4000.0, 4000.0, 1.0, 1.0};
    for (std::size_t index = 0; index < CHARACTERISTIC_COUNT; ++index) {
      const double expected = signs.at(index) * want.characteristics.at(index);
      if (std::isnan(expected)) {
        EXPECT_TRUE(std::isnan(got.characteristics.at(index))) << "row " << row << " characteristic " << index;
      } else {
        EXPECT_TRUE(near_scaled(got.characteristics.at(index), expected, tol, scales.at(index)))
            << "row " << row << " characteristic " << index;
      }
    }
  }

  /// Every row of `got` matches the same-index row of `want` as `expect_characteristics_match` defines (`got` may be
  /// the shorter list).
  void expect_all_characteristics_match(const std::vector<result_row> &got, const std::vector<result_row> &want,
                                        const std::array<double, CHARACTERISTIC_COUNT> &signs) {
    for (std::size_t index = 0; index < got.size(); ++index) {
      expect_characteristics_match(got[index], want[index], signs, index);
    }
  }

  /// The secant FY_OVER_SA and FX_OVER_SR of a no-contact line print as "0" and the peak slips as "nan".
  void expect_no_contact_fields(const std::string &line) {
    const std::vector<std::string> fields = fields_of(line);
    EXPECT_EQ(fields[field_index(mf::mf5x::FY_OVER_SA)], "0") << line;
    EXPECT_EQ(fields[field_index(mf::mf5x::FX_OVER_SR)], "0") << line;
    EXPECT_EQ(fields[field_index(mf::mf5x::SA_AT_PEAK_FY)], "nan") << line;
    EXPECT_EQ(fields[field_index(mf::mf5x::SR_AT_PEAK_FX)], "nan") << line;
  }

  /// In the lines of a zero-slip run, the secant of the SA = 0 row (line 1) and of the SR = 0 row (line 2) is the slope
  /// of that row, field for field.
  void expect_secants_equal_slopes_at_zero_slip(const std::vector<std::string> &lines) {
    const std::vector<std::string> v2 = fields_of(lines[1]);
    EXPECT_EQ(v2[field_index(mf::mf5x::FY_OVER_SA)], v2[field_index(mf::mf5x::DFY_DSA)]);
    const std::vector<std::string> v3 = fields_of(lines[2]);
    EXPECT_EQ(v3[field_index(mf::mf5x::FX_OVER_SR)], v3[field_index(mf::mf5x::DFX_DSR)]);
  }

  /// The text of the characteristic fields of every data line of `out`, joined by ';'.
  std::vector<std::string> characteristic_texts(const std::string &out) {
    const std::vector<std::string> lines = lines_of(out);
    std::vector<std::string> texts;
    for (std::size_t index = 1; index < lines.size(); ++index) {
      const std::vector<std::string> fields = fields_of(lines[index]);
      std::string joined;
      for (std::size_t field = FIRST_CHARACTERISTIC; field < fields.size(); ++field) {
        joined += fields[field] + ";";
      }
      texts.push_back(joined);
    }
    return texts;
  }
} // namespace

TEST(cli_run, Run_PacSVectors_CharacteristicColumnsMatchVectors) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S.tir");
  const printed_run want{printed_row{"-39162.83710", "5151.900624", "-39162.83710", "39354.79640", "4000", "4000",
                                     "0.2475646539", "0.1299038106"},
                         printed_row{"-34707.69218", "71554.17527", "-47527.96305", "71554.17527", "4000", "4000",
                                     "0.2475646539", "0.1299038106"},
                         printed_row{"-8938.282121", "3642.943867", "-24701.24502", "27828.04341", "4000", "4000",
                                     "0.2475646539", "0.1299038106"},
                         printed_row{"-24542.04450", "4608.000003", "-33607.34497", "35200.00000", "4000", "4000",
                                     "0.2475646539", "0.1299038106"},
                         NO_CONTACT,
                         NO_CONTACT};
  expect_printed_run(result, want, "S combined");

  const std::vector<std::string> lines = lines_of(result.out);
  ASSERT_EQ(lines.size(), 7U);
  // V2 (SA 0): FY_OVER_SA is the slope DFY_DSA of the row, V3 (SR 0): FX_OVER_SR is DFX_DSR.
  expect_secants_equal_slopes_at_zero_slip(lines);
  // V22: no contact gives exactly +0 secants, with no sign.
  expect_no_contact_fields(lines[5]);
  expect_no_contact_fields(lines[6]);
}

TEST(cli_run, Run_Mf52Vectors_CharacteristicColumnsEqualPac2002) {
  const run_result pac = run_files("inputs_s_vectors.csv", "pac2002_S.tir");
  const run_result mf52 = run_files("inputs_s_vectors.csv", "mf52_S52.tir");
  ASSERT_EQ(pac.code, mf::cli::EXIT_OK) << pac.err;
  ASSERT_EQ(mf52.code, mf::cli::EXIT_OK) << mf52.err;
  const std::vector<std::string> expected = characteristic_texts(pac.out);
  const std::vector<std::string> actual = characteristic_texts(mf52.out);
  ASSERT_EQ(expected.size(), 6U);
  EXPECT_EQ(actual, expected);
}

TEST(cli_run, Run_Uncombined_CharacteristicsOfPureCurves) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S.tir", {"--uncombined"});
  const printed_run want{printed_row{"-55384.61537", "5151.900624", "-55384.61537", "39354.79640", "4000", "4000",
                                     "0.2475646539", "0.1299038106"},
                         printed_row{"-34707.69218", "79999.99999", "-47527.96305", "79999.99999", "4000", "4000",
                                     "0.2475646539", "0.1299038106"},
                         printed_row{"-12640.63980", "5151.900624", "-34932.83571", "39354.79640", "4000", "4000",
                                     "0.2475646539", "0.1299038106"},
                         printed_row{"-34707.69218", "5151.900624", "-47527.96305", "39354.79640", "4000", "4000",
                                     "0.2475646539", "0.1299038106"},
                         NO_CONTACT,
                         NO_CONTACT};
  expect_printed_run(result, want, "S uncombined");

  const std::vector<std::string> lines = lines_of(result.out);
  ASSERT_EQ(lines.size(), 7U);
  // V2 (SA 0): FY_OVER_SA = DFY_DSA = 5200*By; V3 (SR 0): FX_OVER_SR = DFX_DSR = 6000*Bx.
  expect_secants_equal_slopes_at_zero_slip(lines);
}

TEST(cli_run, Run_EllipseFixture_ZeroSlipRowsUseDirectionalLimit) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S_ell.tir");
  const printed_run want{printed_row{"-32427.16913", "5151.900624", "-32427.16913", "39354.79640", "4000", "4000",
                                     "0.4281129082", "0.1299038106"},
                         printed_row{"-34707.69218", "56580.32736", "-47527.96305", "56580.32736", "4000", "4000",
                                     "0.2475646539", "0.1675533315"},
                         printed_row{"-16749.59182", "15657.19503", "-26292.52881", "28076.37066", "4000", "4000",
                                     "0.4281129082", "0.2445156130"},
                         printed_row{"-27161.23002", "10599.59198", "-30596.45195", "35316.04991", "4000", "4000",
                                     "0.4281129082", "0.1675533315"},
                         NO_CONTACT,
                         NO_CONTACT};
  expect_printed_run(result, want, "S_ell combined");

  const std::vector<std::string> lines = lines_of(result.out);
  ASSERT_EQ(lines.size(), 7U);
  // V2 (SA 0): the secant is the directional-limit slope of the row; V3 (SR 0) likewise.
  expect_secants_equal_slopes_at_zero_slip(lines);
}

TEST(cli_run, Run_TableSystemIsoOnSaeModel_CharacteristicsConverted) {
  const run_result converted = run_files("inputs_s_vectors.csv", "pac2002_S_sae.tir", {"--table-system=iso"});
  const run_result iso = run_files("inputs_s_vectors.csv", "pac2002_S_iso.tir");
  ASSERT_EQ(converted.code, mf::cli::EXIT_OK) << converted.err;
  ASSERT_EQ(iso.code, mf::cli::EXIT_OK) << iso.err;
  EXPECT_FALSE(converted.out.contains("-nan")) << converted.out;
  const std::vector<result_row> converted_rows = rows_of(converted.out);
  const std::vector<result_row> iso_rows = rows_of(iso.out);
  ASSERT_EQ(converted_rows.size(), 6U);
  ASSERT_EQ(iso_rows.size(), 6U);
  // The table is in ISO for both runs: the converted SAE model prints what the ISO model prints.
  constexpr std::array<double, CHARACTERISTIC_COUNT> same{1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
  expect_all_characteristics_match(converted_rows, iso_rows, same);

  // Without conversion the SAE model on its own SAE table: DFY_DSA, FY_OVER_SA and DY have the other sign than the ISO
  // model on the ISO table, the other five characteristics are equal.
  const run_result sae = run_files("inputs_sae.csv", "pac2002_S_sae.tir");
  ASSERT_EQ(sae.code, mf::cli::EXIT_OK) << sae.err;
  EXPECT_FALSE(sae.out.contains("-nan")) << sae.out;
  const std::vector<result_row> sae_rows = rows_of(sae.out);
  ASSERT_EQ(sae_rows.size(), 4U);
  constexpr std::array<double, CHARACTERISTIC_COUNT> negated{-1.0, 1.0, -1.0, 1.0, -1.0, 1.0, 1.0, 1.0};
  expect_all_characteristics_match(sae_rows, iso_rows, negated);
}

namespace {
  /// The slope of row `index` is an exact zero (of either sign) when its slip was clamped, and non-zero otherwise.
  void expect_zero_slope_iff_clamped(double clamped_flag, double slope, std::size_t index) {
    if (clamped_flag == 1.0) {
      EXPECT_EQ(slope, 0.0) << "row " << index;
    } else {
      EXPECT_NE(slope, 0.0) << "row " << index;
    }
  }
} // namespace

TEST(cli, Run_RangesFixture_ClampedRowsZeroSlope) {
  const run_result result = run_files("inputs_ranges.csv", "pac2002_S_rng.tir");
  ASSERT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  const std::vector<result_row> rows = rows_of(result.out);
  ASSERT_EQ(rows.size(), 6U);
  for (std::size_t index = 0; index < rows.size(); ++index) {
    expect_zero_slope_iff_clamped(rows[index].sa_clamped, rows[index].characteristics[mf::mf5x::DFY_DSA], index);
    expect_zero_slope_iff_clamped(rows[index].sr_clamped, rows[index].characteristics[mf::mf5x::DFX_DSR], index);
  }
  const auto clamped_sa = [](const result_row &row) { return row.sa_clamped == 1.0; };
  const auto clamped_sr = [](const result_row &row) { return row.sr_clamped == 1.0; };
  EXPECT_EQ(std::ranges::count_if(rows, clamped_sa), 1);
  EXPECT_EQ(std::ranges::count_if(rows, clamped_sr), 1);
}

TEST(cli_run, Run_Help_UsageDescribesCharacteristicsAndNan) {
  const run_result result = run_cli({"--help"});
  EXPECT_EQ(result.code, mf::cli::EXIT_OK);
  EXPECT_TRUE(result.out.contains(EXPECTED_HEADER)) << result.out;
  EXPECT_TRUE(result.out.contains(mf::io::results_header(mf::mf5x::PAC2002_LAYOUT))) << result.out;
  EXPECT_TRUE(result.out.contains(mf::io::results_header(mf::mf5x::MF52_LAYOUT))) << result.out;
  EXPECT_TRUE(result.out.contains("nan appears only in these last two")) << result.out;
  EXPECT_TRUE(result.out.contains("is a value, not an error.")) << result.out;
  EXPECT_TRUE(result.out.contains("(at SA = 0 or SR = 0 the slope DFY_DSA or")) << result.out;
}
