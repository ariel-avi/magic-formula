#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <gtest/gtest.h>
#include <magic_formula/error.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/number.hpp>
#include <magic_formula/io/text_file.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// Regression test against committed MFeval 4.0 numbers (tests/data/reference/README.md). It states that our MF 5.2
// model "agrees with the MFeval 4.0 FITTYP-6 path" on the synthetic fixture mf52_S52.tir. It is not a validation
// against TNO MF-Tyre 5.2: both our equations and MFeval's FITTYP-6 branches are implementation-derived.
namespace {
  using mf::test::data_file;

  /// The 1e-10 relative tolerance documented in tests/data/reference/README.md, applied to the scale term and the
  /// magnitude term alike. The agreement on this fixture is about 1e-11 relative at worst (Mz) and 2.5e-15 (Fx, Fy),
  /// and every committed row passes at 1e-10. A looser 1e-6 bound is deliberately not used: it would let a 1e-7
  /// relative perturbation (for example the denominator epsilon applied twice) pass, so the committed reference would
  /// not pin the implementation.
  constexpr double MFEVAL_REL_TOL = 1e-10;

  /// FNOMIN of mf52_S52.tir [N]: the force scale of the tolerance, an absolute floor for near-zero forces.
  constexpr double NOMINAL_LOAD_N = 4000.0;

  /// UNLOADED_RADIUS of mf52_S52.tir [m]: with FNOMIN it gives the moment scale, an absolute floor for near-zero
  /// moments.
  constexpr double UNLOADED_RADIUS_M = 0.3;

  /// Data rows of mf52_mfeval.csv (tests/data/reference/README.md).
  constexpr std::size_t REFERENCE_ROWS = 190;

  /// Columns of mf52_mfeval.csv: FZ, SA, SR, IA, VX, FX, FY, MZ, MX, MY.
  constexpr std::size_t FIELD_COUNT = 10;

  constexpr std::string_view REFERENCE_FILE = "reference/mf52_mfeval.csv";
  constexpr std::string_view REFERENCE_HEADER = "FZ,SA,SR,IA,VX,FX,FY,MZ,MX,MY";

  /// One data row of the reference CSV: the operating point, the MFeval outputs and the 1-based file line.
  struct reference_row {
    mf::test::point point;
    /// MFeval outputs in `mf::mf5x::output_index` order FX, FY, MZ, MX, MY.
    std::array<double, 5> expected{};
    std::size_t line = 0;
  };

  /// Splits `line` at ',' (a trailing ',' yields an empty last field).
  std::vector<std::string_view> split_commas(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t begin = 0;
    while (true) {
      const std::size_t end = line.find(',', begin);
      fields.push_back(line.substr(begin, end - begin));
      if (end == std::string_view::npos) {
        return fields;
      }
      begin = end + 1;
    }
  }

  /// All lines of the reference file; a read failure is a test failure and yields no lines.
  std::vector<std::string> reference_lines() {
    const std::expected<std::string, mf::error> text = mf::io::read_text_file(data_file(REFERENCE_FILE));
    if (!text.has_value()) {
      ADD_FAILURE() << mf::to_string(text.error());
      return {};
    }
    std::istringstream stream(*text);
    std::vector<std::string> lines;
    for (std::string line; std::getline(stream, line);) {
      lines.push_back(line);
    }
    return lines;
  }

  /// Converts one data line; a malformed line is a test failure and yields nothing.
  std::optional<reference_row> parse_row(std::string_view line, std::size_t line_number) {
    const std::vector<std::string_view> fields = split_commas(line);
    if (fields.size() != FIELD_COUNT) {
      ADD_FAILURE() << "line " << line_number << ": expected " << FIELD_COUNT << " fields, got " << fields.size();
      return std::nullopt;
    }
    std::vector<double> values;
    for (const std::string_view field : fields) {
      const std::optional<double> parsed = mf::io::parse_number(field);
      if (!parsed.has_value()) {
        ADD_FAILURE() << "line " << line_number << ": not a number: '" << field << "'";
        return std::nullopt;
      }
      values.push_back(*parsed);
    }
    reference_row row;
    row.line = line_number;
    row.point.fz_n = values[0];
    row.point.alpha_rad = values[1];
    row.point.kappa = values[2];
    row.point.gamma_rad = values[3];
    row.point.vx_mps = values[4];
    row.expected[mf::mf5x::FX] = values[5];
    row.expected[mf::mf5x::FY] = values[6];
    row.expected[mf::mf5x::MZ] = values[7];
    row.expected[mf::mf5x::MX] = values[8];
    row.expected[mf::mf5x::MY] = values[9];
    return row;
  }

  /// All data rows of the reference CSV (the header line is skipped).
  std::vector<reference_row> reference_rows() {
    const std::vector<std::string> lines = reference_lines();
    std::vector<reference_row> rows;
    for (std::size_t index = 1; index < lines.size(); ++index) {
      const std::optional<reference_row> row = parse_row(lines[index], index + 1U);
      if (row.has_value()) {
        rows.push_back(*row);
      }
    }
    return rows;
  }

  /// |ours - reference| <= tol*scale + tol*max(|ours|, |reference|), with `tol` = MFEVAL_REL_TOL (1e-10).
  ::testing::AssertionResult agrees(double ours, double reference, double scale) {
    const double bound = MFEVAL_REL_TOL * (scale + std::max(std::abs(ours), std::abs(reference)));
    if (std::abs(ours - reference) <= bound) {
      return ::testing::AssertionSuccess();
    }
    const std::string message = std::format("ours {:.17g}, MFeval {:.17g}, bound {:.3g}", ours, reference, bound);
    return ::testing::AssertionFailure() << message;
  }

  /// Loads the MF 5.2 fixture; a failure is a test failure and yields no model.
  std::expected<mf::io::loaded_model, mf::error> load_fixture() {
    std::expected<mf::io::loaded_model, mf::error> loaded = mf::io::load_tire_model_file(data_file("mf52_S52.tir"));
    if (!loaded.has_value()) {
      ADD_FAILURE() << mf::to_string(loaded.error());
    }
    return loaded;
  }

  /// Compares the five outputs of one reference row with `output` (indexed by `mf::mf5x::output_index`).
  void expect_row_agrees(const reference_row &row, const Eigen::Ref<const Eigen::VectorXd> &output) {
    constexpr double force_scale_n = NOMINAL_LOAD_N;
    constexpr double moment_scale_nm = NOMINAL_LOAD_N * UNLOADED_RADIUS_M;
    EXPECT_TRUE(agrees(output(mf::mf5x::FX), row.expected[mf::mf5x::FX], force_scale_n)) << "FX, line " << row.line;
    EXPECT_TRUE(agrees(output(mf::mf5x::FY), row.expected[mf::mf5x::FY], force_scale_n)) << "FY, line " << row.line;
    EXPECT_TRUE(agrees(output(mf::mf5x::MZ), row.expected[mf::mf5x::MZ], moment_scale_nm)) << "MZ, line " << row.line;
    EXPECT_TRUE(agrees(output(mf::mf5x::MX), row.expected[mf::mf5x::MX], moment_scale_nm)) << "MX, line " << row.line;
    EXPECT_TRUE(agrees(output(mf::mf5x::MY), row.expected[mf::mf5x::MY], moment_scale_nm)) << "MY, line " << row.line;
  }
} // namespace

TEST(mfeval_reference, Reference_Mf52Csv_HasHeaderAndRows) {
  const std::vector<std::string> lines = reference_lines();
  ASSERT_FALSE(lines.empty());
  EXPECT_EQ(lines.front(), REFERENCE_HEADER);
  EXPECT_EQ(lines.size(), REFERENCE_ROWS + 1U);
  EXPECT_EQ(reference_rows().size(), REFERENCE_ROWS);
}

TEST(mfeval_reference, Reference_Mf52Csv_IsNotTrivial) {
  // The reference rows must exercise every channel that carries evidence: a vacuous (all-zero) file would pass.
  constexpr double minimum_peak_n = 100.0;
  double peak_fx = 0.0;
  double peak_fy = 0.0;
  double peak_mz = 0.0;
  for (const reference_row &row : reference_rows()) {
    peak_fx = std::max(peak_fx, std::abs(row.expected[mf::mf5x::FX]));
    peak_fy = std::max(peak_fy, std::abs(row.expected[mf::mf5x::FY]));
    peak_mz = std::max(peak_mz, std::abs(row.expected[mf::mf5x::MZ]));
  }
  EXPECT_GT(peak_fx, minimum_peak_n);
  EXPECT_GT(peak_fy, minimum_peak_n);
  EXPECT_GT(peak_mz, 1.0);
}

TEST(mfeval_reference, Mf52S52_CombinedSlip_AgreesWithMfevalFittyp6Path) {
  const std::expected<mf::io::loaded_model, mf::error> loaded = load_fixture();
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->model->name(), "MF 5.2");
  const std::vector<reference_row> rows = reference_rows();
  ASSERT_EQ(rows.size(), REFERENCE_ROWS);
  EXPECT_EQ(loaded->model->output_length(), mf::mf5x::OUTPUT_LENGTH);
  for (const reference_row &row : rows) {
    const Eigen::VectorXd output = loaded->model->evaluate(mf::test::input_of(row.point));
    ASSERT_EQ(output.size(), mf::mf5x::OUTPUT_LENGTH) << "line " << row.line;
    expect_row_agrees(row, output);
  }
}

TEST(mfeval_reference, Mf52_BatchEvaluate_MatchesWithin1e10) {
  const std::expected<mf::io::loaded_model, mf::error> loaded = load_fixture();
  ASSERT_TRUE(loaded.has_value());
  const std::vector<reference_row> rows = reference_rows();
  ASSERT_EQ(rows.size(), REFERENCE_ROWS);
  std::vector<mf::test::point> points;
  for (const reference_row &row : rows) {
    points.push_back(row.point);
  }
  const Eigen::MatrixXd outputs = loaded->model->evaluate_batch(mf::test::inputs_of(points));
  ASSERT_EQ(outputs.rows(), static_cast<Eigen::Index>(rows.size()));
  ASSERT_EQ(outputs.cols(), mf::mf5x::OUTPUT_LENGTH);
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const Eigen::VectorXd output = outputs.row(static_cast<Eigen::Index>(index)).transpose();
    expect_row_agrees(rows[index], output);
  }
}
