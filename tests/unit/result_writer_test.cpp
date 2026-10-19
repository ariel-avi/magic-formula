#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/number.hpp>
#include <magic_formula/io/result_writer.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
  /// Number of comma-separated fields on every result line (5 inputs + 5 outputs).
  constexpr std::size_t FIELD_COUNT = 10;

  std::uint64_t bits(double value) {
    return std::bit_cast<std::uint64_t>(value);
  }

  /// Text written by `write_results`.
  std::string written(const std::vector<mf::operating_point> &inputs, const std::vector<mf::tire_outputs> &outputs) {
    std::ostringstream out;
    mf::io::write_results(out, inputs, outputs);
    return out.str();
  }

  /// Splits `text` at `separator`; a trailing separator does not produce an empty last piece.
  std::vector<std::string> split(const std::string &text, char separator) {
    std::istringstream stream(text);
    std::vector<std::string> parts;
    for (std::string part; std::getline(stream, part, separator);) {
      parts.push_back(part);
    }
    return parts;
  }

  /// The ten numeric values of one result line, parsed back exactly (NaN-free by construction of the tests).
  std::vector<double> numbers_of(const std::string &line) {
    std::vector<double> values;
    for (const std::string &field : split(line, ',')) {
      const auto parsed = mf::io::parse_number(field);
      EXPECT_TRUE(parsed.has_value()) << field;
      values.push_back(parsed.value_or(0.0));
    }
    return values;
  }

  /// Bit-exact comparison of a parsed-back value with the double that was written.
  void expect_same(double actual, double expected) {
    EXPECT_EQ(bits(actual), bits(expected)) << actual << " vs " << expected;
  }

  mf::operating_point make_point(double fz, double alpha, double kappa, double gamma, double vx) {
    return mf::operating_point{.fz_n = fz, .kappa = kappa, .alpha_rad = alpha, .gamma_rad = gamma, .vx_mps = vx};
  }

  mf::tire_outputs make_outputs(double fx, double fy, double mz, double mx, double my) {
    return mf::tire_outputs{.fx_n = fx, .fy_n = fy, .mz_nm = mz, .mx_nm = mx, .my_nm = my, .flags = {}};
  }
} // namespace

TEST(result_writer, Header_Constant_MatchesSpecification) {
  EXPECT_EQ(mf::io::RESULTS_HEADER, "FZ[N],SA[rad],SR[-],IA[rad],VX[m/s],FX[N],FY[N],MZ[N*m],MX[N*m],MY[N*m]");
}

TEST(result_writer, Write_NoRows_PrintsHeaderOnly) {
  EXPECT_EQ(written({}, {}), std::string(mf::io::RESULTS_HEADER) + "\n");
}

TEST(result_writer, Write_Rows_InInputOrder) {
  // Every value differs, so a swapped column or row shows up.
  const std::vector<mf::operating_point> inputs{
      make_point(4000.0, 0.05, 0.1, 0.02, 10.0),
      make_point(3000.0, -0.25, -0.5, 0.0, 20.0),
      make_point(500.0, 0.0, 1.0, -0.125, 0.5),
  };
  const std::vector<mf::tire_outputs> outputs{
      make_outputs(11.0, 12.0, 13.0, 14.0, 15.0),
      make_outputs(-21.5, 22.5, -23.5, 24.5, -25.5),
      make_outputs(31.25, -32.25, 33.25, -34.25, 35.25),
  };
  const std::string text = written(inputs, outputs);
  ASSERT_FALSE(text.empty());
  EXPECT_EQ(text.back(), '\n');
  const std::vector<std::string> lines = split(text, '\n');
  ASSERT_EQ(lines.size(), 4U);
  EXPECT_EQ(lines[0], std::string(mf::io::RESULTS_HEADER));

  // Field order: FZ, SA, SR, IA, VX, FX, FY, MZ, MX, MY.
  const std::array<std::array<double, FIELD_COUNT>, 3> expected{{
      {4000.0, 0.05, 0.1, 0.02, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0},
      {3000.0, -0.25, -0.5, 0.0, 20.0, -21.5, 22.5, -23.5, 24.5, -25.5},
      {500.0, 0.0, 1.0, -0.125, 0.5, 31.25, -32.25, 33.25, -34.25, 35.25},
  }};
  for (std::size_t row = 0; row < expected.size(); ++row) {
    const std::vector<double> values = numbers_of(lines[row + 1]);
    ASSERT_EQ(values.size(), FIELD_COUNT) << lines[row + 1];
    for (std::size_t column = 0; column < FIELD_COUNT; ++column) {
      // Bit-exact: the printed text must parse back to the written double.
      expect_same(values[column], expected[row][column]);
    }
  }
}

TEST(result_writer, Write_Rows_PrintsShortestText) {
  const std::vector<mf::operating_point> inputs{make_point(4000.0, 0.05, 0.1, 0.0, 10.0)};
  const std::vector<mf::tire_outputs> outputs{make_outputs(1.5, -2.25, 0.0, 1000.0, -12.0)};
  const std::vector<std::string> lines = split(written(inputs, outputs), '\n');
  ASSERT_EQ(lines.size(), 2U);
  EXPECT_EQ(lines[1], "4000,0.05,0.1,0,10,1.5,-2.25,0,1000,-12");
}

TEST(result_writer, Write_RoundTrip_ParsesBackExactly) {
  const std::vector<mf::operating_point> inputs{make_point(0.1, 1e-300, 3935.4812345, -2376.4, 1e22)};
  const std::vector<mf::tire_outputs> outputs{make_outputs(1.0 / 3.0, -2.0 / 3.0, 1e300, -1e-5, 123456789.123456789)};
  const std::vector<std::string> lines = split(written(inputs, outputs), '\n');
  ASSERT_EQ(lines.size(), 2U);
  const std::vector<double> values = numbers_of(lines[1]);
  ASSERT_EQ(values.size(), FIELD_COUNT);
  // FZ, SA, SR, IA, VX, then FX, FY, MZ, MX, MY.
  const std::array<double, FIELD_COUNT> expected{
      0.1, 1e-300, 3935.4812345, -2376.4, 1e22, 1.0 / 3.0, -2.0 / 3.0, 1e300, -1e-5, 123456789.123456789,
  };
  for (std::size_t column = 0; column < FIELD_COUNT; ++column) {
    expect_same(values[column], expected[column]);
  }
}

TEST(result_writer, Write_NegativeZero_PrintsMinusZero) {
  const std::vector<mf::operating_point> inputs{make_point(-0.0, -0.0, -0.0, -0.0, -0.0),
                                                make_point(0.0, 0.0, 0.0, 0.0, 0.0)};
  const std::vector<mf::tire_outputs> outputs{make_outputs(-0.0, -0.0, -0.0, -0.0, -0.0),
                                              make_outputs(0.0, 0.0, 0.0, 0.0, 0.0)};
  const std::vector<std::string> lines = split(written(inputs, outputs), '\n');
  ASSERT_EQ(lines.size(), 3U);
  EXPECT_EQ(lines[1], "-0,-0,-0,-0,-0,-0,-0,-0,-0,-0");
  EXPECT_EQ(lines[2], "0,0,0,0,0,0,0,0,0,0");
  // The sign of zero also survives the parse-back.
  for (const double value : numbers_of(lines[1])) {
    expect_same(value, -0.0);
  }
}

TEST(result_writer, Write_InputsEchoedUnclamped_FlagsNotPrinted) {
  // Flags describe clamping done by the model; they are not part of the CSV.
  mf::tire_outputs clamped = make_outputs(1.0, 2.0, 3.0, 4.0, 5.0);
  clamped.flags.fz_clamped = true;
  clamped.flags.below_vxlow = true;
  const std::vector<mf::operating_point> inputs{make_point(12500.0, 9.0, 99.0, 0.0, 0.0)};
  const std::vector<mf::tire_outputs> outputs{clamped};
  const std::vector<std::string> lines = split(written(inputs, outputs), '\n');
  ASSERT_EQ(lines.size(), 2U);
  EXPECT_EQ(lines[1], "12500,9,99,0,0,1,2,3,4,5");
}
