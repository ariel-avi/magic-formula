#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <array>
#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <initializer_list>
#include <limits>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/sign_transform.hpp>
#include <optional>
#include <string>

namespace {
  using mf::coordinate_system;
  using mf::test::bits;

  constexpr double QNAN = std::numeric_limits<double>::quiet_NaN();

  const std::array<mf::model_layout, 2> LAYOUTS{mf::mf5x::PAC2002_LAYOUT, mf::mf5x::MF52_LAYOUT};

  Eigen::MatrixXd sample_inputs() {
    Eigen::MatrixXd inputs(3, 5);
    inputs << 4000.0, 0.1, 0.05, 0.02, 16.6, //
        -0.0, -0.2, QNAN, -1e-300, 20.0,     //
        0.0, 0.0, -0.3, 0.0, 1.0;
    return inputs;
  }

  Eigen::MatrixXd sample_outputs() {
    Eigen::MatrixXd outputs(2, 10);
    outputs << 100.0, 200.0, 30.0, 4.0, 5.0, 1.0, 0.0, 1.0, 0.0, 1.0, //
        -0.0, -7.0, 3.0, QNAN, -2.0, 0.0, 1.0, 0.0, 1.0, 0.0;
    return outputs;
  }

  void expect_same_bits(const Eigen::MatrixXd &actual, const Eigen::MatrixXd &expected) {
    ASSERT_EQ(actual.rows(), expected.rows());
    ASSERT_EQ(actual.cols(), expected.cols());
    for (Eigen::Index c = 0; c < actual.cols(); ++c) {
      for (Eigen::Index r = 0; r < actual.rows(); ++r) {
        EXPECT_EQ(bits(actual(r, c)), bits(expected(r, c))) << "row " << r << " column " << c;
      }
    }
  }

  /// Fills a table whose FZ column (index 0 of the PAC2002 layout) holds `fz`.
  Eigen::MatrixXd table_with_fz(const std::initializer_list<double> &fz) {
    Eigen::MatrixXd inputs = Eigen::MatrixXd::Zero(static_cast<Eigen::Index>(fz.size()), 5);
    Eigen::Index row = 0;
    for (const double value : fz) {
      inputs(row, 0) = value;
      ++row;
    }
    return inputs;
  }
} // namespace

TEST(coordinate_system, Name_EachSystem_Spelling) {
  EXPECT_EQ(mf::coordinate_system_name(coordinate_system::ISO), "ISO");
  EXPECT_EQ(mf::coordinate_system_name(coordinate_system::SAE), "SAE");
}

TEST(coordinate_system, Parse_IsoSaeAnyCase_Accepted) {
  for (const char *text : {"iso", "ISO", "Iso"}) {
    const auto parsed = mf::parse_coordinate_system(text);
    ASSERT_TRUE(parsed.has_value()) << text;
    EXPECT_EQ(*parsed, coordinate_system::ISO) << text;
  }
  for (const char *text : {"SAE", "sae", "Sae"}) {
    const auto parsed = mf::parse_coordinate_system(text);
    ASSERT_TRUE(parsed.has_value()) << text;
    EXPECT_EQ(*parsed, coordinate_system::SAE) << text;
  }
}

TEST(coordinate_system, Parse_IsoWAutoEmpty_Rejected) {
  for (const char *text : {"ISO-W", "iso_w", "auto", "", "isox", "ISO W", "SAEJ"}) {
    EXPECT_FALSE(mf::parse_coordinate_system(text).has_value()) << text;
  }
}

TEST(coordinate_system, DataTransforms_AllPairs_VerticalOnlyWhenDifferent) {
  for (const coordinate_system from : {coordinate_system::ISO, coordinate_system::SAE}) {
    for (const coordinate_system to : {coordinate_system::ISO, coordinate_system::SAE}) {
      const mf::sign_transforms transforms = mf::data_transforms(from, to);
      EXPECT_EQ(transforms.vertical, from != to);
      EXPECT_FALSE(transforms.lateral);
    }
  }
}

TEST(coordinate_system, ConvertInputs_IsoToSae_FlipsOnlyFz) {
  const Eigen::MatrixXd original = sample_inputs();
  for (const mf::model_layout &layout : LAYOUTS) {
    Eigen::MatrixXd inputs = original;
    mf::convert_inputs(layout, coordinate_system::ISO, coordinate_system::SAE, inputs);
    for (Eigen::Index r = 0; r < inputs.rows(); ++r) {
      EXPECT_EQ(bits(inputs(r, 0)), bits(-original(r, 0))) << "row " << r;
      for (Eigen::Index c = 1; c < inputs.cols(); ++c) {
        EXPECT_EQ(bits(inputs(r, c)), bits(original(r, c))) << "row " << r << " column " << c;
      }
    }
  }
}

TEST(coordinate_system, ConvertOutputs_IsoToSae_FlipsFyMzMy) {
  const Eigen::MatrixXd original = sample_outputs();
  const std::array<bool, 10> flipped{false, true, true, false, true, false, false, false, false, false};
  for (const mf::model_layout &layout : LAYOUTS) {
    Eigen::MatrixXd outputs = original;
    mf::convert_outputs(layout, coordinate_system::ISO, coordinate_system::SAE, outputs);
    for (Eigen::Index c = 0; c < outputs.cols(); ++c) {
      for (Eigen::Index r = 0; r < outputs.rows(); ++r) {
        const double expected = flipped[static_cast<std::size_t>(c)] ? -original(r, c) : original(r, c);
        EXPECT_EQ(bits(outputs(r, c)), bits(expected)) << "row " << r << " column " << c;
      }
    }
  }
}

TEST(coordinate_system, ConvertInputs_SameSystem_BitIdentical) {
  const Eigen::MatrixXd original = sample_inputs();
  for (const coordinate_system system : {coordinate_system::ISO, coordinate_system::SAE}) {
    Eigen::MatrixXd inputs = original;
    mf::convert_inputs(mf::mf5x::PAC2002_LAYOUT, system, system, inputs);
    expect_same_bits(inputs, original);
  }
}

TEST(coordinate_system, ConvertOutputs_SameSystem_BitIdentical) {
  const Eigen::MatrixXd original = sample_outputs();
  for (const coordinate_system system : {coordinate_system::ISO, coordinate_system::SAE}) {
    Eigen::MatrixXd outputs = original;
    mf::convert_outputs(mf::mf5x::MF52_LAYOUT, system, system, outputs);
    expect_same_bits(outputs, original);
  }
}

TEST(coordinate_system, ConvertInputsOutputs_RoundTrip_BitExact) {
  const Eigen::MatrixXd original_inputs = sample_inputs();
  const Eigen::MatrixXd original_outputs = sample_outputs();
  for (const mf::model_layout &layout : LAYOUTS) {
    Eigen::MatrixXd inputs = original_inputs;
    Eigen::MatrixXd outputs = original_outputs;
    mf::convert_inputs(layout, coordinate_system::ISO, coordinate_system::SAE, inputs);
    mf::convert_outputs(layout, coordinate_system::ISO, coordinate_system::SAE, outputs);
    mf::convert_inputs(layout, coordinate_system::SAE, coordinate_system::ISO, inputs);
    mf::convert_outputs(layout, coordinate_system::SAE, coordinate_system::ISO, outputs);
    expect_same_bits(inputs, original_inputs);
    expect_same_bits(outputs, original_outputs);
  }
}

TEST(coordinate_system, CensusFz_LayoutWithoutFz_IsAllZero) {
  static constexpr std::array<mf::input_channel, 2> INPUTS{{{.name = "A", .unit = "N"}, {.name = "B", .unit = "N"}}};
  const mf::model_layout layout{.inputs = INPUTS, .outputs = {}, .limits = {}};
  Eigen::MatrixXd table(2, 2);
  table << -1.0, 1.0, 1.0, -1.0;
  const mf::fz_sign_census census = mf::census_fz(layout, table);
  EXPECT_EQ(census.positive, 0U);
  EXPECT_EQ(census.negative, 0U);
  EXPECT_EQ(census.first_positive, 0U);
  EXPECT_EQ(census.first_negative, 0U);
}

TEST(coordinate_system, CensusFz_EmptyTable_IsAllZero) {
  const Eigen::MatrixXd table(0, 5);
  const mf::fz_sign_census census = mf::census_fz(mf::mf5x::PAC2002_LAYOUT, table);
  EXPECT_EQ(census.positive, 0U);
  EXPECT_EQ(census.negative, 0U);
}

TEST(coordinate_system, CensusFz_SignedZeroAndNaN_CountAsNeither) {
  const Eigen::MatrixXd table = table_with_fz({0.0, -0.0, QNAN});
  const mf::fz_sign_census census = mf::census_fz(mf::mf5x::PAC2002_LAYOUT, table);
  EXPECT_EQ(census.positive, 0U);
  EXPECT_EQ(census.negative, 0U);
}

TEST(coordinate_system, CensusFz_MixedTable_CountsAndFirstRows) {
  const Eigen::MatrixXd table = table_with_fz({0.0, 4000.0, -100.0, 3000.0, -200.0});
  const mf::fz_sign_census census = mf::census_fz(mf::mf5x::PAC2002_LAYOUT, table);
  EXPECT_EQ(census.positive, 2U);
  EXPECT_EQ(census.first_positive, 1U);
  EXPECT_EQ(census.negative, 2U);
  EXPECT_EQ(census.first_negative, 2U);
}

TEST(coordinate_system, LoadSideWarning_NothingOffends_Empty) {
  const mf::fz_sign_census positive_only{.positive = 4, .first_positive = 1};
  const mf::fz_sign_census negative_only{.negative = 4, .first_negative = 1};
  EXPECT_FALSE(mf::load_side_warning(positive_only, coordinate_system::ISO).has_value());
  EXPECT_FALSE(mf::load_side_warning(negative_only, coordinate_system::SAE).has_value());
  EXPECT_FALSE(mf::load_side_warning(mf::fz_sign_census{}, coordinate_system::ISO).has_value());
  EXPECT_FALSE(mf::load_side_warning(mf::fz_sign_census{}, coordinate_system::SAE).has_value());
}

TEST(coordinate_system, LoadSideWarning_IsoNegatives_ExactText) {
  const mf::fz_sign_census census{.positive = 4, .negative = 1, .first_positive = 0, .first_negative = 5};
  const auto warning = mf::load_side_warning(census, coordinate_system::ISO);
  ASSERT_TRUE(warning.has_value());
  EXPECT_EQ(warning->index, 5U);
  EXPECT_EQ(warning->cause.where.key, "FZ");
  EXPECT_EQ(warning->cause.where.file, "");
  EXPECT_EQ(warning->cause.where.line, 0U);
  EXPECT_EQ(warning->cause.message,
            "FZ < 0 in 1 row(s) of an ISO table: a loaded tire has FZ > 0 in ISO; these rows are lift-off (zero forces "
            "and moments) (the table mixes FZ signs)");
}

TEST(coordinate_system, LoadSideWarning_IsoOnlyNegatives_NoSuffix) {
  const mf::fz_sign_census census{.negative = 2, .first_negative = 3};
  const auto warning = mf::load_side_warning(census, coordinate_system::ISO);
  ASSERT_TRUE(warning.has_value());
  EXPECT_EQ(warning->index, 3U);
  EXPECT_EQ(warning->cause.message,
            "FZ < 0 in 2 row(s) of an ISO table: a loaded tire has FZ > 0 in ISO; these rows are lift-off (zero forces "
            "and moments)");
}

TEST(coordinate_system, LoadSideWarning_SaePositives_ExactText) {
  const mf::fz_sign_census census{.positive = 2, .first_positive = 1};
  const auto warning = mf::load_side_warning(census, coordinate_system::SAE);
  ASSERT_TRUE(warning.has_value());
  EXPECT_EQ(warning->index, 1U);
  EXPECT_EQ(warning->cause.where.key, "FZ");
  EXPECT_EQ(warning->cause.message,
            "FZ > 0 in 2 row(s) of an SAE table: a loaded tire has FZ < 0 in SAE; these rows are lift-off (zero forces "
            "and moments)");
}

TEST(coordinate_system, LoadSideWarning_SaeMixed_Suffix) {
  const mf::fz_sign_census census{.positive = 1, .negative = 3, .first_positive = 2, .first_negative = 0};
  const auto warning = mf::load_side_warning(census, coordinate_system::SAE);
  ASSERT_TRUE(warning.has_value());
  EXPECT_EQ(warning->index, 2U);
  EXPECT_EQ(warning->cause.message,
            "FZ > 0 in 1 row(s) of an SAE table: a loaded tire has FZ < 0 in SAE; these rows are lift-off (zero forces "
            "and moments) (the table mixes FZ signs)");
}
