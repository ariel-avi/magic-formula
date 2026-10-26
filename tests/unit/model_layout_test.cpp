#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <array>
#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/error.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/sign_transform.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
  constexpr double INF = std::numeric_limits<double>::infinity();
  constexpr double QUIET_NAN = std::numeric_limits<double>::quiet_NaN();

  using mf::sign_transform;
  using mf::sign_transforms;
  using mf::test::bits;

  // The four conversion plans: identity, VERTICAL, LATERAL and both.
  constexpr std::array<sign_transforms, 4> ALL_SETS{{
      {.vertical = false, .lateral = false},
      {.vertical = true, .lateral = false},
      {.vertical = false, .lateral = true},
      {.vertical = true, .lateral = true},
  }};

  // Four channels: A never negated, B and D VERTICAL, C LATERAL.
  constexpr std::array<mf::input_channel, 4> SIGN_INPUTS{{
      {.name = "A", .unit = "-"},
      {.name = "B", .unit = "-", .negated_by = sign_transform::VERTICAL},
      {.name = "C", .unit = "-", .negated_by = sign_transform::LATERAL},
      {.name = "D", .unit = "-", .negated_by = sign_transform::VERTICAL},
  }};

  constexpr std::array<mf::output_channel, 4> SIGN_OUTPUTS{{
      {.name = "A", .unit = "-"},
      {.name = "B", .unit = "-", .negated_by = sign_transform::VERTICAL},
      {.name = "C", .unit = "-", .negated_by = sign_transform::LATERAL},
      {.name = "D", .unit = "-", .negated_by = sign_transform::VERTICAL},
  }};

  // Three characteristic channels: A never negated, B negated by VERTICAL and LATERAL together, C by VERTICAL only.
  constexpr std::array<mf::characteristic_channel, 3> SIGN_CHARACTERISTICS{{
      {.name = "A", .unit = "-", .negated_by = {.vertical = false, .lateral = false}, .nan_allowed = false},
      {.name = "B", .unit = "-", .negated_by = {.vertical = true, .lateral = true}, .nan_allowed = false},
      {.name = "C", .unit = "-", .negated_by = {.vertical = true, .lateral = false}, .nan_allowed = false},
  }};

  // Finite values, a NaN, +0.0 and -0.0 in every column.
  Eigen::MatrixXd sign_sample() {
    const std::array<double, 12> flat{1.5, -2.5, 3.25, QUIET_NAN, QUIET_NAN, 0.0, -0.0, 7.0, 0.0, -0.0, 8.5, -9.0};
    Eigen::MatrixXd values(3, 4);
    for (Eigen::Index i = 0; i < 12; ++i) {
      values(i / 4, i % 4) = flat[static_cast<std::size_t>(i)];
    }
    return values;
  }

  // Same bit pattern, or both NaN (the payload of a NaN is not part of the contract).
  void expect_same_value_bits(double actual, double expected) {
    if (std::isnan(expected)) {
      EXPECT_TRUE(std::isnan(actual));
    } else {
      EXPECT_EQ(bits(actual), bits(expected));
    }
  }

  template <typename Channel_T>
  void expect_flips_declared_columns(std::span<const Channel_T> channels, const sign_transforms &set) {
    const Eigen::MatrixXd original = sign_sample();
    Eigen::MatrixXd values = original;
    mf::apply_sign_transforms(channels, set, values);
    for (Eigen::Index c = 0; c < values.cols(); ++c) {
      const bool flipped = mf::contains(set, channels[static_cast<std::size_t>(c)].negated_by);
      for (Eigen::Index r = 0; r < values.rows(); ++r) {
        SCOPED_TRACE(testing::Message() << "row " << r << " column " << c);
        expect_same_value_bits(values(r, c), flipped ? -original(r, c) : original(r, c));
      }
    }
    mf::apply_sign_transforms(channels, set, values);
    for (Eigen::Index c = 0; c < values.cols(); ++c) {
      for (Eigen::Index r = 0; r < values.rows(); ++r) {
        SCOPED_TRACE(testing::Message() << "row " << r << " column " << c);
        expect_same_value_bits(values(r, c), original(r, c));
      }
    }
  }

  struct sign_row {
    mf::limit_role role;
    sign_transform negated_by;
  };

  bool signs_ok(const std::vector<sign_row> &rows) {
    std::vector<mf::limit_channel> channels;
    for (const sign_row &row : rows) {
      channels.push_back(mf::limit_channel{.key = "K", .role = row.role, .negated_by = row.negated_by});
    }
    return mf::limit_signs_well_formed(channels);
  }

  // P: a VERTICAL pair, Q: a LATERAL pair, V: a plain value.
  constexpr std::array<mf::limit_channel, 5> PAIR_CHANNELS{{
      {.key = "P_LO", .role = mf::limit_role::LOWER, .negated_by = sign_transform::VERTICAL},
      {.key = "P_HI", .role = mf::limit_role::UPPER, .negated_by = sign_transform::VERTICAL},
      {.key = "Q_LO", .role = mf::limit_role::LOWER, .negated_by = sign_transform::LATERAL},
      {.key = "Q_HI", .role = mf::limit_role::UPPER, .negated_by = sign_transform::LATERAL},
      {.key = "V", .role = mf::limit_role::VALUE},
  }};

  Eigen::VectorXd limits_of(const std::array<double, 5> &values) {
    Eigen::VectorXd limits(5);
    for (Eigen::Index i = 0; i < 5; ++i) {
      limits(i) = values[static_cast<std::size_t>(i)];
    }
    return limits;
  }

  // Built at run time on purpose, so that the coverage tools see the constexpr function being executed.
  std::vector<mf::limit_channel> make_channels(const std::vector<mf::limit_role> &roles) {
    std::vector<mf::limit_channel> channels;
    for (const mf::limit_role role : roles) {
      channels.push_back(mf::limit_channel{.key = "K", .role = role, .default_value = 0.0});
    }
    return channels;
  }

  bool well_formed(const std::vector<mf::limit_role> &roles) {
    const std::vector<mf::limit_channel> channels = make_channels(roles);
    return mf::limit_pairs_well_formed(channels);
  }

  struct pair_names {
    mf::mf5x::limit_index lower;
    mf::mf5x::limit_index upper;
  };
} // namespace

TEST(default_limits, MixedChannels_ReturnsDefaultsInOrder) {
  const std::vector<mf::limit_channel> channels{
      {.key = "LO", .role = mf::limit_role::LOWER, .default_value = -INF},
      {.key = "HI", .role = mf::limit_role::UPPER, .default_value = INF},
      {.key = "V", .role = mf::limit_role::VALUE, .default_value = 1.0},
  };
  const Eigen::VectorXd values = mf::default_limits(channels);
  ASSERT_EQ(values.size(), 3);
  EXPECT_TRUE(std::isinf(values(0)) && values(0) < 0.0);
  EXPECT_TRUE(std::isinf(values(1)) && values(1) > 0.0);
  EXPECT_EQ(values(2), 1.0);
}

TEST(default_limits, EmptyChannels_ReturnsEmptyVector) {
  const std::vector<mf::limit_channel> channels;
  EXPECT_EQ(mf::default_limits(channels).size(), 0);
}

TEST(limit_pairs_well_formed, LowerUpperValue_ReturnsTrue) {
  EXPECT_TRUE(well_formed({mf::limit_role::LOWER, mf::limit_role::UPPER, mf::limit_role::VALUE}));
}

TEST(limit_pairs_well_formed, Empty_ReturnsTrue) {
  EXPECT_TRUE(well_formed({}));
}

TEST(limit_pairs_well_formed, LowerLast_ReturnsFalse) {
  EXPECT_FALSE(well_formed({mf::limit_role::VALUE, mf::limit_role::LOWER}));
}

TEST(limit_pairs_well_formed, LowerThenValue_ReturnsFalse) {
  EXPECT_FALSE(well_formed({mf::limit_role::LOWER, mf::limit_role::VALUE}));
}

TEST(limit_pairs_well_formed, UpperFirst_ReturnsFalse) {
  EXPECT_FALSE(well_formed({mf::limit_role::UPPER, mf::limit_role::LOWER}));
}

TEST(limit_pairs_well_formed, UpperAfterValue_ReturnsFalse) {
  EXPECT_FALSE(well_formed({mf::limit_role::VALUE, mf::limit_role::UPPER}));
}

TEST(validate_limits, SizeMismatch_ReturnsInvalidArgument) {
  const Eigen::VectorXd limits = Eigen::VectorXd::Zero(8);
  const auto result = mf::validate_limits(limits, mf::mf5x::LIMITS);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_ARGUMENT);
  EXPECT_NE(result.error().message.find('8'), std::string::npos);
  EXPECT_NE(result.error().message.find('9'), std::string::npos);
}

TEST(validate_limits, MalformedChannels_ReturnsInvalidArgument) {
  const std::vector<mf::limit_channel> channels = make_channels({mf::limit_role::UPPER, mf::limit_role::LOWER});
  const Eigen::VectorXd limits = Eigen::VectorXd::Zero(2);
  const auto result = mf::validate_limits(limits, channels);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_ARGUMENT);
}

TEST(validate_limits, LowerAboveUpper_EachPair_ReturnsInvalidValueWithLowerKey) {
  using namespace mf::mf5x;
  const pair_names pairs[] = {{FZMIN, FZMAX}, {KPUMIN, KPUMAX}, {ALPMIN, ALPMAX}, {CAMMIN, CAMMAX}};
  for (const pair_names &pair : pairs) {
    Eigen::VectorXd limits = mf::default_limits(LIMITS);
    limits(pair.lower) = 2.0;
    limits(pair.upper) = 1.0;
    const auto result = mf::validate_limits(limits, LIMITS);
    ASSERT_FALSE(result.has_value());
    const std::string lower_key(LIMITS[pair.lower].key);
    const std::string upper_key(LIMITS[pair.upper].key);
    EXPECT_EQ(result.error().code, mf::error_code::INVALID_VALUE);
    EXPECT_EQ(result.error().where.key, lower_key);
    EXPECT_EQ(result.error().message, lower_key + " 2 > " + upper_key + " 1: lower limit above upper limit");
  }
}

TEST(validate_limits, NanLowerBound_ReturnsInvalidValue) {
  Eigen::VectorXd limits = mf::default_limits(mf::mf5x::LIMITS);
  limits(mf::mf5x::ALPMIN) = std::numeric_limits<double>::quiet_NaN();
  const auto result = mf::validate_limits(limits, mf::mf5x::LIMITS);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_VALUE);
  EXPECT_EQ(result.error().where.key, "ALPMIN");
}

TEST(validate_limits, NanUpperBound_ReturnsInvalidValue) {
  Eigen::VectorXd limits = mf::default_limits(mf::mf5x::LIMITS);
  limits(mf::mf5x::CAMMAX) = std::numeric_limits<double>::quiet_NaN();
  const auto result = mf::validate_limits(limits, mf::mf5x::LIMITS);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_VALUE);
  EXPECT_EQ(result.error().where.key, "CAMMIN");
}

TEST(validate_limits, EqualBounds_Succeeds) {
  Eigen::VectorXd limits = mf::default_limits(mf::mf5x::LIMITS);
  limits(mf::mf5x::FZMIN) = 500.0;
  limits(mf::mf5x::FZMAX) = 500.0;
  EXPECT_TRUE(mf::validate_limits(limits, mf::mf5x::LIMITS).has_value());
}

TEST(validate_limits, DefaultInfiniteBounds_Succeeds) {
  const Eigen::VectorXd limits = mf::default_limits(mf::mf5x::LIMITS);
  EXPECT_TRUE(mf::validate_limits(limits, mf::mf5x::LIMITS).has_value());
}

TEST(validate_limits, EmptyChannelsEmptyVector_Succeeds) {
  const std::vector<mf::limit_channel> channels;
  const Eigen::VectorXd limits;
  EXPECT_TRUE(mf::validate_limits(limits, channels).has_value());
}

TEST(find_channel, InputPresent_ReturnsIndex) {
  const std::span<const mf::input_channel> inputs = mf::mf5x::PAC2002_INPUTS;
  const std::optional<std::size_t> index = mf::find_channel(inputs, "IA");
  ASSERT_TRUE(index.has_value());
  EXPECT_EQ(*index, 3U);
}

TEST(find_channel, OutputPresent_ReturnsIndex) {
  const std::span<const mf::output_channel> outputs = mf::mf5x::OUTPUTS;
  const std::optional<std::size_t> index = mf::find_channel(outputs, "MZ");
  ASSERT_TRUE(index.has_value());
  EXPECT_EQ(*index, 2U);
}

TEST(find_channel, Absent_ReturnsEmpty) {
  const std::span<const mf::input_channel> inputs = mf::mf5x::PAC2002_INPUTS;
  EXPECT_FALSE(mf::find_channel(inputs, "P").has_value());
  EXPECT_FALSE(mf::find_channel(inputs, "fz").has_value());
  EXPECT_FALSE(mf::find_channel(std::span<const mf::input_channel>{}, "FZ").has_value());
}

TEST(find_channel, OutputAbsent_ReturnsEmpty) {
  const std::span<const mf::output_channel> outputs = mf::mf5x::OUTPUTS;
  EXPECT_FALSE(mf::find_channel(outputs, "FZ").has_value());
  EXPECT_FALSE(mf::find_channel(outputs, "mz").has_value());
  EXPECT_FALSE(mf::find_channel(std::span<const mf::output_channel>{}, "MZ").has_value());
}

TEST(find_channel, LastOutput_ReturnsLastIndex) {
  const std::span<const mf::output_channel> outputs = mf::mf5x::OUTPUTS;
  const std::optional<std::size_t> index = mf::find_channel(outputs, "BELOW_VXLOW");
  ASSERT_TRUE(index.has_value());
  EXPECT_EQ(*index, 9U);
}

TEST(sign_transform, Contains_EachSetAndTransform_MatchesMembership) {
  for (const sign_transforms &set : ALL_SETS) {
    SCOPED_TRACE(testing::Message() << "vertical " << set.vertical << " lateral " << set.lateral);
    EXPECT_FALSE(mf::contains(set, sign_transform::NONE));
    EXPECT_EQ(mf::contains(set, sign_transform::VERTICAL), set.vertical);
    EXPECT_EQ(mf::contains(set, sign_transform::LATERAL), set.lateral);
  }
}

TEST(sign_transform, Name_EachEnumerator_ReturnsSpelling) {
  EXPECT_EQ(mf::sign_transform_name(sign_transform::NONE), "NONE");
  EXPECT_EQ(mf::sign_transform_name(sign_transform::VERTICAL), "VERTICAL");
  EXPECT_EQ(mf::sign_transform_name(sign_transform::LATERAL), "LATERAL");
}

TEST(model_layout, NegatedBy_DefaultChannels_AreNone) {
  EXPECT_EQ(mf::input_channel{}.negated_by, sign_transform::NONE);
  EXPECT_EQ(mf::output_channel{}.negated_by, sign_transform::NONE);
  EXPECT_EQ(mf::limit_channel{}.negated_by, sign_transform::NONE);
}

TEST(sign_transform, Negates_AllSixteenCombinations_OddParity) {
  for (const sign_transforms &negators : ALL_SETS) {
    for (const sign_transforms &plan : ALL_SETS) {
      const int count = ((negators.vertical && plan.vertical) ? 1 : 0) + ((negators.lateral && plan.lateral) ? 1 : 0);
      SCOPED_TRACE(testing::Message() << "negators " << negators.vertical << negators.lateral << " plan "
                                      << plan.vertical << plan.lateral);
      EXPECT_EQ(mf::negates(negators, plan), count % 2 == 1);
    }
  }
}

TEST(model_layout, Layout_WithoutCharacteristics_EmptySpan) {
  const mf::model_layout layout{.inputs = SIGN_INPUTS, .outputs = SIGN_OUTPUTS, .limits = {}};
  EXPECT_TRUE(layout.characteristics.empty());
}

TEST(model_layout, ApplySignTransformsCharacteristics_EachPlan_NegatesOddParityColumns) {
  const Eigen::MatrixXd original = sign_sample().leftCols(3);
  const std::array<bool, 4> expected_vertical{false, true, true, false};
  const std::array<bool, 4> expected_lateral{false, true, false, false};
  const std::array<bool, 4> expected_both{false, false, true, false};
  const std::array<bool, 4> expected_none{false, false, false, false};
  const std::array<std::array<bool, 4>, 4> expected{expected_none, expected_vertical, expected_lateral, expected_both};
  for (std::size_t k = 0; k < ALL_SETS.size(); ++k) {
    SCOPED_TRACE(k);
    Eigen::MatrixXd values = original;
    mf::apply_sign_transforms(SIGN_CHARACTERISTICS, ALL_SETS[k], values);
    for (Eigen::Index c = 0; c < values.cols(); ++c) {
      const bool flipped = expected[k][static_cast<std::size_t>(c)];
      for (Eigen::Index r = 0; r < values.rows(); ++r) {
        expect_same_value_bits(values(r, c), flipped ? -original(r, c) : original(r, c));
      }
    }
  }
}

TEST(model_layout, ApplySignTransformsCharacteristics_EmptyPlan_BitIdentity) {
  const Eigen::MatrixXd original = sign_sample().leftCols(3);
  Eigen::MatrixXd values = original;
  mf::apply_sign_transforms(SIGN_CHARACTERISTICS, ALL_SETS[0], values);
  for (Eigen::Index c = 0; c < values.cols(); ++c) {
    for (Eigen::Index r = 0; r < values.rows(); ++r) {
      EXPECT_EQ(bits(values(r, c)), bits(original(r, c)));
    }
  }
}

TEST(model_layout, ApplySignTransformsCharacteristics_NaN_FlipsSignBitOnly) {
  Eigen::MatrixXd values(1, 3);
  values << QUIET_NAN, QUIET_NAN, QUIET_NAN;
  const bool before = std::signbit(values(0, 1));
  mf::apply_sign_transforms(SIGN_CHARACTERISTICS, sign_transforms{.vertical = true, .lateral = false}, values);
  EXPECT_TRUE(std::isnan(values(0, 1)));
  EXPECT_NE(std::signbit(values(0, 1)), before);
  EXPECT_TRUE(std::isnan(values(0, 0)));
  EXPECT_EQ(std::signbit(values(0, 0)), before);
}

TEST(model_layout, ApplySignTransformsCharacteristics_AppliedTwice_RestoresBits) {
  const Eigen::MatrixXd original = sign_sample().leftCols(3);
  for (const sign_transforms &set : ALL_SETS) {
    Eigen::MatrixXd values = original;
    mf::apply_sign_transforms(SIGN_CHARACTERISTICS, set, values);
    mf::apply_sign_transforms(SIGN_CHARACTERISTICS, set, values);
    for (Eigen::Index c = 0; c < values.cols(); ++c) {
      for (Eigen::Index r = 0; r < values.rows(); ++r) {
        expect_same_value_bits(values(r, c), original(r, c));
      }
    }
  }
}

TEST(model_layout, ApplySignTransforms_InputsEachSet_FlipsExactlyDeclaredColumns) {
  for (const sign_transforms &set : ALL_SETS) {
    expect_flips_declared_columns<mf::input_channel>(SIGN_INPUTS, set);
  }
}

TEST(model_layout, ApplySignTransforms_OutputsEachSet_FlipsExactlyDeclaredColumns) {
  for (const sign_transforms &set : ALL_SETS) {
    expect_flips_declared_columns<mf::output_channel>(SIGN_OUTPUTS, set);
  }
}

TEST(model_layout, TransformLimits_EachSet_NegatesAndSwapsDeclaredPairs) {
  const Eigen::VectorXd original = limits_of({200.0, 10000.0, -INF, 0.5, 1.0});
  const Eigen::VectorXd vertical_only = limits_of({-10000.0, -200.0, -INF, 0.5, 1.0});
  const Eigen::VectorXd lateral_only = limits_of({200.0, 10000.0, -0.5, INF, 1.0});
  const Eigen::VectorXd both = limits_of({-10000.0, -200.0, -0.5, INF, 1.0});
  const std::array<Eigen::VectorXd, 4> expected{original, vertical_only, lateral_only, both};
  for (std::size_t k = 0; k < ALL_SETS.size(); ++k) {
    SCOPED_TRACE(k);
    const Eigen::VectorXd result = mf::transform_limits(original, PAIR_CHANNELS, ALL_SETS[k]);
    EXPECT_TRUE(mf::test::limits_bitwise_equal(result, expected[k]));
    EXPECT_LE(result(0), result(1));
    EXPECT_LE(result(2), result(3));
    const Eigen::VectorXd twice = mf::transform_limits(result, PAIR_CHANNELS, ALL_SETS[k]);
    EXPECT_TRUE(mf::test::limits_bitwise_equal(twice, original));
  }
}

TEST(model_layout, LimitSignsWellFormed_Cases_DetectsMismatch) {
  constexpr mf::limit_role lower = mf::limit_role::LOWER;
  constexpr mf::limit_role upper = mf::limit_role::UPPER;
  constexpr mf::limit_role value = mf::limit_role::VALUE;
  constexpr sign_transform vertical = sign_transform::VERTICAL;
  constexpr sign_transform lateral = sign_transform::LATERAL;
  constexpr sign_transform none = sign_transform::NONE;
  EXPECT_TRUE(signs_ok({}));
  EXPECT_TRUE(signs_ok({{lower, vertical}, {upper, vertical}, {lower, lateral}, {upper, lateral}, {value, none}}));
  EXPECT_FALSE(signs_ok({{lower, vertical}, {upper, lateral}}));
  EXPECT_FALSE(signs_ok({{value, vertical}}));
  EXPECT_FALSE(signs_ok({{value, none}, {lower, none}}));
}
