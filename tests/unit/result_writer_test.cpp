#include <Eigen/Core>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/io/result_writer.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/model_layout.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
  /// The PAC2002 / MF 5.2 header: five inputs, the forces, moments and flags, then the eight characteristics.
  constexpr std::string_view EXPECTED_HEADER =
      "FZ[N],SA[rad],SR[-],IA[rad],VX[m/s],FX[N],FY[N],MZ[N*m],MX[N*m],MY[N*m],"
      "FZ_CLAMPED[-],SR_CLAMPED[-],SA_CLAMPED[-],IA_CLAMPED[-],BELOW_VXLOW[-],"
      "DFY_DSA[N/rad],DFX_DSR[N],FY_OVER_SA[N/rad],FX_OVER_SR[N],DY[N],DX[N],SA_AT_PEAK_FY[rad],SR_AT_PEAK_FX[-]";

  /// Columns of the characteristics of the MF 5.x layouts.
  constexpr Eigen::Index CHARACTERISTIC_COLUMNS = 8;

  /// A layout with one input and one output.
  constexpr std::array<mf::input_channel, 1> CUSTOM_INPUTS{{{.name = "A", .unit = "x"}}};
  constexpr std::array<mf::output_channel, 1> CUSTOM_OUTPUTS{{{.name = "B", .unit = "y"}}};
  constexpr std::array<mf::characteristic_channel, 1> CUSTOM_CHARACTERISTICS{{{.name = "C", .unit = "z"}}};
  constexpr mf::model_layout CUSTOM_LAYOUT{.inputs = CUSTOM_INPUTS, .outputs = CUSTOM_OUTPUTS, .limits = {}};
  constexpr mf::model_layout CUSTOM_LAYOUT_WITH_CHARACTERISTICS{
      .inputs = CUSTOM_INPUTS, .outputs = CUSTOM_OUTPUTS, .limits = {}, .characteristics = CUSTOM_CHARACTERISTICS};

  /// Text written by `write_results` with the PAC2002 layout.
  std::string written(const Eigen::MatrixXd &inputs, const Eigen::MatrixXd &outputs,
                      const Eigen::MatrixXd &characteristics) {
    std::ostringstream out;
    mf::io::write_results(out, mf::mf5x::PAC2002_LAYOUT, inputs, outputs, characteristics);
    return out.str();
  }

  /// Splits `text` at '\n'; a trailing newline does not produce an empty last piece.
  std::vector<std::string> split_lines(const std::string &text) {
    std::istringstream stream(text);
    std::vector<std::string> parts;
    for (std::string part; std::getline(stream, part);) {
      parts.push_back(part);
    }
    return parts;
  }

  /// A rows x cols matrix filled with `value`.
  Eigen::MatrixXd filled(Eigen::Index rows, Eigen::Index cols, double value) {
    return Eigen::MatrixXd::Constant(rows, cols, value);
  }
} // namespace

TEST(results_header, Pac2002Layout_ExactString) {
  EXPECT_EQ(mf::io::results_header(mf::mf5x::PAC2002_LAYOUT), EXPECTED_HEADER);
}

TEST(results_header, Mf52Layout_SameString) {
  EXPECT_EQ(mf::io::results_header(mf::mf5x::MF52_LAYOUT), EXPECTED_HEADER);
}

TEST(results_header, CustomLayout_InputsThenOutputs) {
  EXPECT_EQ(mf::io::results_header(CUSTOM_LAYOUT), "A[x],B[y]");
}

TEST(results_header, CustomLayoutWithCharacteristics_AppendsFields) {
  EXPECT_EQ(mf::io::results_header(CUSTOM_LAYOUT_WITH_CHARACTERISTICS), "A[x],B[y],C[z]");
}

TEST(write_results, HeaderThenOneLinePerRow) {
  // Every value differs, so a swapped column or row shows up.
  Eigen::MatrixXd inputs(3, 5);
  inputs << 4000.0, 0.05, 0.1, 0.02, 10.0, //
      3000.0, -0.25, -0.5, 0.0, 20.0,      //
      500.0, 0.0, 1.0, -0.125, 0.5;
  Eigen::MatrixXd outputs(3, 10);
  outputs << 11.0, 12.0, 13.0, 14.0, 15.0, 0.0, 0.0, 0.0, 0.0, 0.0, //
      -21.5, 22.5, -23.5, 24.5, -25.5, 1.0, 0.0, 0.0, 0.0, 0.0,     //
      31.25, -32.25, 33.25, -34.25, 35.25, 0.0, 1.0, 0.0, 1.0, 1.0;
  Eigen::MatrixXd characteristics(3, CHARACTERISTIC_COLUMNS);
  characteristics << 101.0, 102.0, 103.0, 104.0, 105.0, 106.0, 107.0, 108.0, //
      -201.5, 202.5, -203.5, 204.5, -205.5, 206.5, -207.5, 208.5,            //
      301.25, -302.25, 303.25, -304.25, 305.25, -306.25, 307.25, -308.25;
  const std::string text = written(inputs, outputs, characteristics);
  ASSERT_FALSE(text.empty());
  EXPECT_EQ(text.back(), '\n');
  const std::vector<std::string> lines = split_lines(text);
  ASSERT_EQ(lines.size(), 4U);
  EXPECT_EQ(lines[0], EXPECTED_HEADER);
  EXPECT_EQ(lines[1], "4000,0.05,0.1,0.02,10,11,12,13,14,15,0,0,0,0,0,101,102,103,104,105,106,107,108");
  EXPECT_EQ(lines[2], "3000,-0.25,-0.5,0,20,-21.5,22.5,-23.5,24.5,-25.5,1,0,0,0,0,"
                      "-201.5,202.5,-203.5,204.5,-205.5,206.5,-207.5,208.5");
  EXPECT_EQ(lines[3], "500,0,1,-0.125,0.5,31.25,-32.25,33.25,-34.25,35.25,0,1,0,1,1,"
                      "301.25,-302.25,303.25,-304.25,305.25,-306.25,307.25,-308.25");
}

TEST(write_results, NaNEitherSign_PrintsNan) {
  constexpr double quiet_nan = std::numeric_limits<double>::quiet_NaN();
  const double negative_nan = std::copysign(quiet_nan, -1.0);
  ASSERT_TRUE(std::signbit(negative_nan));
  Eigen::MatrixXd characteristics = filled(1, CHARACTERISTIC_COLUMNS, 1.0);
  characteristics(0, 6) = quiet_nan;
  characteristics(0, 7) = negative_nan;
  const std::vector<std::string> lines = split_lines(written(filled(1, 5, 2.0), filled(1, 10, 3.0), characteristics));
  ASSERT_EQ(lines.size(), 2U);
  EXPECT_EQ(lines[1], "2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,1,1,1,1,1,1,nan,nan");
  EXPECT_EQ(lines[1].find("-nan"), std::string::npos);
}

TEST(write_results, FiniteValue_PrintsShortestRoundTrip) {
  Eigen::MatrixXd inputs = filled(1, 5, 0.1);
  inputs(0, 1) = 1e-300;
  Eigen::MatrixXd characteristics = filled(1, CHARACTERISTIC_COLUMNS, 0.1);
  characteristics(0, 0) = -1e300;
  const std::vector<std::string> lines = split_lines(written(inputs, filled(1, 10, 0.1), characteristics));
  ASSERT_EQ(lines.size(), 2U);
  EXPECT_EQ(lines[1],
            "0.1,1e-300,0.1,0.1,0.1,0.1,0.1,0.1,0.1,0.1,0.1,0.1,0.1,0.1,0.1,-1e+300,0.1,0.1,0.1,0.1,0.1,0.1,0.1");
}

TEST(write_results, NegativeZeroPrintsMinusZero_FlagsPrintZeroOne) {
  Eigen::MatrixXd inputs = filled(2, 5, 0.0);
  inputs.row(0).setConstant(-0.0);
  Eigen::MatrixXd outputs = filled(2, 10, 0.0);
  outputs.row(0).setConstant(-0.0);
  outputs(1, 5) = 1.0;
  outputs(1, 9) = 1.0;
  Eigen::MatrixXd characteristics = filled(2, CHARACTERISTIC_COLUMNS, 0.0);
  characteristics.row(0).setConstant(-0.0);
  const std::vector<std::string> lines = split_lines(written(inputs, outputs, characteristics));
  ASSERT_EQ(lines.size(), 3U);
  EXPECT_EQ(lines[1], "-0,-0,-0,-0,-0,-0,-0,-0,-0,-0,-0,-0,-0,-0,-0,-0,-0,-0,-0,-0,-0,-0,-0");
  EXPECT_EQ(lines[2], "0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0");
}

TEST(write_results, ZeroRows_HeaderOnly) {
  EXPECT_EQ(written(filled(0, 5, 0.0), filled(0, 10, 0.0), filled(0, CHARACTERISTIC_COLUMNS, 0.0)),
            std::string(EXPECTED_HEADER) + "\n");
}

TEST(write_results, UnequalRowCounts_WritesMinimumOfThree) {
  const Eigen::MatrixXd inputs = filled(3, 5, 1.0);
  const Eigen::MatrixXd outputs = filled(3, 10, 2.0);
  const Eigen::MatrixXd characteristics = filled(3, CHARACTERISTIC_COLUMNS, 3.0);
  const std::vector<std::string> fewer_characteristics =
      split_lines(written(inputs, outputs, filled(1, CHARACTERISTIC_COLUMNS, 3.0)));
  EXPECT_EQ(fewer_characteristics.size(), 2U);
  const std::vector<std::string> fewer_outputs = split_lines(written(inputs, filled(2, 10, 2.0), characteristics));
  EXPECT_EQ(fewer_outputs.size(), 3U);
  const std::vector<std::string> fewer_inputs =
      split_lines(written(filled(1, 5, 1.0), filled(4, 10, 2.0), filled(4, CHARACTERISTIC_COLUMNS, 3.0)));
  EXPECT_EQ(fewer_inputs.size(), 2U);
  // Outputs fewer than the characteristics.
  const std::vector<std::string> outputs_below_characteristics =
      split_lines(written(filled(5, 5, 1.0), filled(2, 10, 2.0), filled(4, CHARACTERISTIC_COLUMNS, 3.0)));
  EXPECT_EQ(outputs_below_characteristics.size(), 3U);
}

TEST(write_results, LayoutWithoutCharacteristics_WritesInputsAndOutputsOnly) {
  std::ostringstream out;
  mf::io::write_results(out, CUSTOM_LAYOUT, filled(1, 1, 1.5), filled(1, 1, 2.5), filled(1, 0, 0.0));
  EXPECT_EQ(out.str(), "A[x],B[y]\n1.5,2.5\n");
}
