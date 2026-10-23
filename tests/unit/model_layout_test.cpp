#include <Eigen/Core>
#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/error.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/model_layout.hpp>
#include <string>
#include <vector>

namespace {
  constexpr double INF = std::numeric_limits<double>::infinity();

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
