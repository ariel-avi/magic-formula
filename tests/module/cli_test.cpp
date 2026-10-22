#include "../support/fixtures.hpp"

#include <cmath>
#include <cstddef>
#include <expected>
#include <gtest/gtest.h>
#include <ios>
#include <magic_formula/cli/cli.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/number.hpp>
#include <magic_formula/io/result_writer.hpp>
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
  using mf::test::printed_tol;
  using mf::test::rel_near_result;

  /// Number of comma-separated fields on every result line (5 inputs + 5 outputs).
  constexpr std::size_t FIELD_COUNT = 10;

  /// Exit status and both streams of one in-process run.
  struct run_result {
    int code = -1;
    std::string out;
    std::string err;
  };

  /// One parsed result line: the five echoed inputs, then the five outputs.
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

  /// The ten numbers of one result line; a field that is not a number makes the test fail and reads as NaN.
  result_row row_of(const std::string &line) {
    std::vector<double> values;
    std::istringstream stream(line);
    for (std::string field; std::getline(stream, field, ',');) {
      const std::optional<double> parsed = mf::io::parse_number(field);
      EXPECT_TRUE(parsed.has_value()) << "not a number: '" << field << "' in " << line;
      values.push_back(parsed.value_or(std::nan("")));
    }
    if (values.size() != FIELD_COUNT) {
      ADD_FAILURE() << "expected " << FIELD_COUNT << " fields, got " << values.size() << " in " << line;
      return result_row{};
    }
    return result_row{.fz = values[0],
                      .sa = values[1],
                      .sr = values[2],
                      .ia = values[3],
                      .vx = values[4],
                      .fx = values[5],
                      .fy = values[6],
                      .mz = values[7],
                      .mx = values[8],
                      .my = values[9]};
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

  /// V22: Fz <= 0 gives exactly zero for all five outputs.
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

  /// Fixed text of the pressure warning.
  constexpr std::string_view PRESSURE_WARNING = "column P is ignored: PAC2002 and MF 5.2 have no pressure input";
} // namespace

TEST(cli_run, Run_PacSVectors_PrintsExpectedRows) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S.tir");
  EXPECT_EQ(result.code, mf::cli::EXIT_OK);
  EXPECT_TRUE(result.err.empty()) << result.err;

  const std::vector<std::string> lines = lines_of(result.out);
  ASSERT_EQ(lines.size(), 7U);
  EXPECT_EQ(lines[0], std::string(mf::io::RESULTS_HEADER));
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
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 1U) << result.err;

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
  EXPECT_TRUE(user.err.empty()) << user.err;

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
  EXPECT_TRUE(degrees.err.empty()) << degrees.err;
  EXPECT_EQ(lines_of(degrees.out).front(), std::string(mf::io::RESULTS_HEADER));

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
  }
}

TEST(cli_run, Run_Uncombined_PrintsPureForces) {
  const run_result result = run_files("inputs_s_vectors.csv", "pac2002_S.tir", {"--uncombined"});
  EXPECT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  EXPECT_TRUE(result.err.empty()) << result.err;
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
  EXPECT_EQ(count_of(result.err, "mf: info: "), 3U) << result.err;
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 0U) << result.err;
  EXPECT_EQ(lines_of(result.out).size(), 7U);
}

TEST(cli_run, Run_PressureColumn_WarnsOnceAndSucceeds) {
  const run_result result = run_files("inputs_pressure.csv", "pac2002_S.tir");
  EXPECT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 1U) << result.err;
  EXPECT_EQ(count_of(result.err, PRESSURE_WARNING), 1U) << result.err;
  EXPECT_TRUE(result.err.contains(data_file("inputs_pressure.csv").string() + ": " + std::string(PRESSURE_WARNING)))
      << result.err;

  const std::vector<std::string> lines = lines_of(result.out);
  ASSERT_EQ(lines.size(), 2U);
  EXPECT_EQ(lines[0], std::string(mf::io::RESULTS_HEADER));
  const result_row row = row_of(lines[1]);
  // The pressure is not an output column and does not change the result: same point as row 4 of the vectors table.
  expect_inputs(row, 4000.0, 0.05, 0.1, 0.0, 10.0);
  EXPECT_NEAR(row.fy, -1680.37, printed_tol(2));
}

TEST(cli_run, Run_Ranges_OneWarningPerVariable) {
  const run_result result = run_files("inputs_ranges.csv", "pac2002_S_rng.tir");
  EXPECT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  const std::string table = data_file("inputs_ranges.csv").string();
  EXPECT_TRUE(result.err.contains(table + ":5: SR outside the property-file range in 1 row(s); "
                                          "evaluated at the limit\n"))
      << result.err;
  EXPECT_TRUE(result.err.contains(table + ":6: SA outside the property-file range in 1 row(s); "
                                          "evaluated at the limit\n"))
      << result.err;
  EXPECT_TRUE(result.err.contains(table + ":7: IA outside the property-file range in 1 row(s); "
                                          "evaluated at the limit\n"))
      << result.err;
  EXPECT_TRUE(result.err.contains(table + ":8: FZ outside the property-file range in 2 row(s); "
                                          "evaluated at the limit\n"))
      << result.err;
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 4U) << result.err;

  const std::expected<mf::io::loaded_model, mf::error> loaded =
      mf::io::load_tire_model_file(data_file("pac2002_S_rng.tir"));
  ASSERT_TRUE(loaded.has_value()) << mf::to_string(loaded.error());
  const mf::operating_limits &limits = loaded->model->limits();
  ASSERT_TRUE(limits.fz_n.has_value());
  ASSERT_TRUE(limits.kappa.has_value());
  ASSERT_TRUE(limits.alpha_rad.has_value());
  ASSERT_TRUE(limits.gamma_rad.has_value());

  // Rows 2..6 of the output are the table lines 5..9; every one is evaluated at the limit it exceeds (V19).
  const std::vector<mf::operating_point> at_limits{
      mf::operating_point{
          .fz_n = 4000.0, .kappa = limits.kappa->upper, .alpha_rad = 0.0, .gamma_rad = 0.0, .vx_mps = 10.0},
      mf::operating_point{
          .fz_n = 4000.0, .kappa = 0.0, .alpha_rad = limits.alpha_rad->upper, .gamma_rad = 0.0, .vx_mps = 10.0},
      mf::operating_point{
          .fz_n = 4000.0, .kappa = 0.0, .alpha_rad = 0.0, .gamma_rad = limits.gamma_rad->upper, .vx_mps = 10.0},
      mf::operating_point{.fz_n = limits.fz_n->upper, .kappa = 0.1, .alpha_rad = 0.0, .gamma_rad = 0.0, .vx_mps = 10.0},
      mf::operating_point{.fz_n = limits.fz_n->lower, .kappa = 0.1, .alpha_rad = 0.0, .gamma_rad = 0.0, .vx_mps = 10.0},
  };
  const std::vector<result_row> rows = rows_of(result.out);
  ASSERT_EQ(rows.size(), 6U);
  // The inputs are echoed unclamped.
  expect_inputs(rows[1], 4000.0, 0.0, 10.0, 0.0, 10.0);
  expect_inputs(rows[2], 4000.0, 2.0, 0.0, 0.0, 10.0);
  expect_inputs(rows[3], 4000.0, 0.0, 0.0, 0.5, 10.0);
  expect_inputs(rows[4], 20000.0, 0.0, 0.1, 0.0, 10.0);
  expect_inputs(rows[5], 1e-9, 0.0, 0.1, 0.0, 10.0);
  for (std::size_t index = 0; index < at_limits.size(); ++index) {
    const std::expected<mf::tire_outputs, mf::error> expected = loaded->model->evaluate(at_limits[index]);
    ASSERT_TRUE(expected.has_value()) << "row " << index;
    const result_row &row = rows[index + 1];
    // Bit-equal: the printed text parses back to the evaluated double (identity vector V19).
    EXPECT_EQ(bits(row.fx), bits(expected->fx_n)) << "FX row " << index;
    EXPECT_EQ(bits(row.fy), bits(expected->fy_n)) << "FY row " << index;
    EXPECT_EQ(bits(row.mz), bits(expected->mz_nm)) << "MZ row " << index;
    EXPECT_EQ(bits(row.mx), bits(expected->mx_nm)) << "MX row " << index;
    EXPECT_EQ(bits(row.my), bits(expected->my_nm)) << "MY row " << index;
  }
}

TEST(cli_run, Run_LowSpeedPac2002_WarnsVxlowOnce) {
  const run_result result = run_files("inputs_low_speed.csv", "pac2002_S.tir");
  EXPECT_EQ(result.code, mf::cli::EXIT_OK) << result.err;
  const std::string table = data_file("inputs_low_speed.csv").string();
  EXPECT_TRUE(result.err.contains(table + ":3: VX below VXLOW in 2 row(s); VXLOW is not applied\n")) << result.err;
  EXPECT_EQ(count_of(result.err, "mf: warning: "), 1U) << result.err;
  EXPECT_EQ(rows_of(result.out).size(), 2U);
}

TEST(cli_run, Run_LowSpeedMf52_ErrorAtLine4) {
  const run_result result = run_files("inputs_low_speed.csv", "mf52_S52.tir");
  expect_failed_run(result);
  const std::string table = data_file("inputs_low_speed.csv").string();
  EXPECT_TRUE(result.err.contains(table + ":4")) << result.err;
  EXPECT_TRUE(result.err.contains("Vx = 0 m/s")) << result.err;
}

TEST(cli_run, Run_NegativeVx_ErrorNamesLine4) {
  const run_result result = run_files("inputs_negative_vx.csv", "pac2002_S.tir");
  expect_failed_run(result);
  EXPECT_TRUE(result.err.contains(data_file("inputs_negative_vx.csv").string() + ":4")) << result.err;
  EXPECT_TRUE(result.err.contains("Vx = -1 m/s")) << result.err;
}

TEST(cli_run, Run_MalformedTable_ErrorNamesLine4) {
  const run_result result = run_files("inputs_malformed.csv", "pac2002_S.tir");
  expect_failed_run(result);
  EXPECT_TRUE(result.err.contains(data_file("inputs_malformed.csv").string() + ":4")) << result.err;
}

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
  const std::vector<std::string> arguments = file_arguments("inputs_s_vectors.csv", "pac2002_S.tir");
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
  const std::vector<std::string> arguments = file_arguments("inputs_s_vectors.csv", "pac2002_S.tir");
  std::vector<std::string_view> argv{"mf"};
  argv.insert(argv.end(), arguments.begin(), arguments.end());
  const int code = mf::cli::run(argv, out, err);
  EXPECT_EQ(code, mf::cli::EXIT_ERROR);
  EXPECT_TRUE(err.str().starts_with("mf: error: ")) << err.str();
  EXPECT_NE(err.str(), "mf: error: cannot write results\n");
}

TEST(cli_run, Run_NonStdExceptionFromOutputStream_CaughtExit1) {
  non_std_throwing_buffer buffer;
  std::ostream out(&buffer);
  out.exceptions(std::ios::badbit);
  std::ostringstream err;
  const std::vector<std::string> arguments = file_arguments("inputs_s_vectors.csv", "pac2002_S.tir");
  std::vector<std::string_view> argv{"mf"};
  argv.insert(argv.end(), arguments.begin(), arguments.end());
  const int code = mf::cli::run(argv, out, err);
  EXPECT_EQ(code, mf::cli::EXIT_ERROR);
  EXPECT_EQ(err.str(), "mf: error: unknown exception\n");
}
