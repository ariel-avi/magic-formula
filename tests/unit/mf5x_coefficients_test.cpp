#include "../support/fixtures.hpp"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/error.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
  using coeffs = mf::mf5x_coefficients;
  using member_ptr = double coeffs::*;

  struct member_ref {
    member_ptr member;
    std::string_view name;
  };

  // The 25 scaling factors (default 1.0). Kept one entry per line: clang-format would repack this table into
  // padded columns, which makes the diff of a future coefficient addition noisy.
  // clang-format off
  constexpr std::array<member_ref, 25> SCALING_MEMBERS{
      member_ref{.member = &coeffs::LFZO, .name = "LFZO"},
      member_ref{.member = &coeffs::LCX, .name = "LCX"},
      member_ref{.member = &coeffs::LMUX, .name = "LMUX"},
      member_ref{.member = &coeffs::LEX, .name = "LEX"},
      member_ref{.member = &coeffs::LKX, .name = "LKX"},
      member_ref{.member = &coeffs::LHX, .name = "LHX"},
      member_ref{.member = &coeffs::LVX, .name = "LVX"},
      member_ref{.member = &coeffs::LGAX, .name = "LGAX"},
      member_ref{.member = &coeffs::LCY, .name = "LCY"},
      member_ref{.member = &coeffs::LMUY, .name = "LMUY"},
      member_ref{.member = &coeffs::LEY, .name = "LEY"},
      member_ref{.member = &coeffs::LKY, .name = "LKY"},
      member_ref{.member = &coeffs::LHY, .name = "LHY"},
      member_ref{.member = &coeffs::LVY, .name = "LVY"},
      member_ref{.member = &coeffs::LGAY, .name = "LGAY"},
      member_ref{.member = &coeffs::LTR, .name = "LTR"},
      member_ref{.member = &coeffs::LRES, .name = "LRES"},
      member_ref{.member = &coeffs::LGAZ, .name = "LGAZ"},
      member_ref{.member = &coeffs::LXAL, .name = "LXAL"},
      member_ref{.member = &coeffs::LYKA, .name = "LYKA"},
      member_ref{.member = &coeffs::LVYKA, .name = "LVYKA"},
      member_ref{.member = &coeffs::LS, .name = "LS"},
      member_ref{.member = &coeffs::LMX, .name = "LMX"},
      member_ref{.member = &coeffs::LVMX, .name = "LVMX"},
      member_ref{.member = &coeffs::LMY, .name = "LMY"},
  };
  // clang-format on

  // FNOMIN, UNLOADED_RADIUS and the 89 P/Q/R/S coefficients (default 0.0).
  constexpr std::array<member_ref, 91> ZERO_MEMBERS{
      member_ref{.member = &coeffs::FNOMIN, .name = "FNOMIN"},
      member_ref{.member = &coeffs::UNLOADED_RADIUS, .name = "UNLOADED_RADIUS"},
      member_ref{.member = &coeffs::PCX1, .name = "PCX1"},
      member_ref{.member = &coeffs::PDX1, .name = "PDX1"},
      member_ref{.member = &coeffs::PDX2, .name = "PDX2"},
      member_ref{.member = &coeffs::PDX3, .name = "PDX3"},
      member_ref{.member = &coeffs::PEX1, .name = "PEX1"},
      member_ref{.member = &coeffs::PEX2, .name = "PEX2"},
      member_ref{.member = &coeffs::PEX3, .name = "PEX3"},
      member_ref{.member = &coeffs::PEX4, .name = "PEX4"},
      member_ref{.member = &coeffs::PKX1, .name = "PKX1"},
      member_ref{.member = &coeffs::PKX2, .name = "PKX2"},
      member_ref{.member = &coeffs::PKX3, .name = "PKX3"},
      member_ref{.member = &coeffs::PHX1, .name = "PHX1"},
      member_ref{.member = &coeffs::PHX2, .name = "PHX2"},
      member_ref{.member = &coeffs::PVX1, .name = "PVX1"},
      member_ref{.member = &coeffs::PVX2, .name = "PVX2"},
      member_ref{.member = &coeffs::RBX1, .name = "RBX1"},
      member_ref{.member = &coeffs::RBX2, .name = "RBX2"},
      member_ref{.member = &coeffs::RCX1, .name = "RCX1"},
      member_ref{.member = &coeffs::REX1, .name = "REX1"},
      member_ref{.member = &coeffs::REX2, .name = "REX2"},
      member_ref{.member = &coeffs::RHX1, .name = "RHX1"},
      member_ref{.member = &coeffs::PCY1, .name = "PCY1"},
      member_ref{.member = &coeffs::PDY1, .name = "PDY1"},
      member_ref{.member = &coeffs::PDY2, .name = "PDY2"},
      member_ref{.member = &coeffs::PDY3, .name = "PDY3"},
      member_ref{.member = &coeffs::PEY1, .name = "PEY1"},
      member_ref{.member = &coeffs::PEY2, .name = "PEY2"},
      member_ref{.member = &coeffs::PEY3, .name = "PEY3"},
      member_ref{.member = &coeffs::PEY4, .name = "PEY4"},
      member_ref{.member = &coeffs::PKY1, .name = "PKY1"},
      member_ref{.member = &coeffs::PKY2, .name = "PKY2"},
      member_ref{.member = &coeffs::PKY3, .name = "PKY3"},
      member_ref{.member = &coeffs::PHY1, .name = "PHY1"},
      member_ref{.member = &coeffs::PHY2, .name = "PHY2"},
      member_ref{.member = &coeffs::PHY3, .name = "PHY3"},
      member_ref{.member = &coeffs::PVY1, .name = "PVY1"},
      member_ref{.member = &coeffs::PVY2, .name = "PVY2"},
      member_ref{.member = &coeffs::PVY3, .name = "PVY3"},
      member_ref{.member = &coeffs::PVY4, .name = "PVY4"},
      member_ref{.member = &coeffs::RBY1, .name = "RBY1"},
      member_ref{.member = &coeffs::RBY2, .name = "RBY2"},
      member_ref{.member = &coeffs::RBY3, .name = "RBY3"},
      member_ref{.member = &coeffs::RCY1, .name = "RCY1"},
      member_ref{.member = &coeffs::REY1, .name = "REY1"},
      member_ref{.member = &coeffs::REY2, .name = "REY2"},
      member_ref{.member = &coeffs::RHY1, .name = "RHY1"},
      member_ref{.member = &coeffs::RHY2, .name = "RHY2"},
      member_ref{.member = &coeffs::RVY1, .name = "RVY1"},
      member_ref{.member = &coeffs::RVY2, .name = "RVY2"},
      member_ref{.member = &coeffs::RVY3, .name = "RVY3"},
      member_ref{.member = &coeffs::RVY4, .name = "RVY4"},
      member_ref{.member = &coeffs::RVY5, .name = "RVY5"},
      member_ref{.member = &coeffs::RVY6, .name = "RVY6"},
      member_ref{.member = &coeffs::QSX1, .name = "QSX1"},
      member_ref{.member = &coeffs::QSX2, .name = "QSX2"},
      member_ref{.member = &coeffs::QSX3, .name = "QSX3"},
      member_ref{.member = &coeffs::QSY1, .name = "QSY1"},
      member_ref{.member = &coeffs::QSY2, .name = "QSY2"},
      member_ref{.member = &coeffs::QSY3, .name = "QSY3"},
      member_ref{.member = &coeffs::QSY4, .name = "QSY4"},
      member_ref{.member = &coeffs::QBZ1, .name = "QBZ1"},
      member_ref{.member = &coeffs::QBZ2, .name = "QBZ2"},
      member_ref{.member = &coeffs::QBZ3, .name = "QBZ3"},
      member_ref{.member = &coeffs::QBZ4, .name = "QBZ4"},
      member_ref{.member = &coeffs::QBZ5, .name = "QBZ5"},
      member_ref{.member = &coeffs::QBZ9, .name = "QBZ9"},
      member_ref{.member = &coeffs::QBZ10, .name = "QBZ10"},
      member_ref{.member = &coeffs::QCZ1, .name = "QCZ1"},
      member_ref{.member = &coeffs::QDZ1, .name = "QDZ1"},
      member_ref{.member = &coeffs::QDZ2, .name = "QDZ2"},
      member_ref{.member = &coeffs::QDZ3, .name = "QDZ3"},
      member_ref{.member = &coeffs::QDZ4, .name = "QDZ4"},
      member_ref{.member = &coeffs::QDZ6, .name = "QDZ6"},
      member_ref{.member = &coeffs::QDZ7, .name = "QDZ7"},
      member_ref{.member = &coeffs::QDZ8, .name = "QDZ8"},
      member_ref{.member = &coeffs::QDZ9, .name = "QDZ9"},
      member_ref{.member = &coeffs::QEZ1, .name = "QEZ1"},
      member_ref{.member = &coeffs::QEZ2, .name = "QEZ2"},
      member_ref{.member = &coeffs::QEZ3, .name = "QEZ3"},
      member_ref{.member = &coeffs::QEZ4, .name = "QEZ4"},
      member_ref{.member = &coeffs::QEZ5, .name = "QEZ5"},
      member_ref{.member = &coeffs::QHZ1, .name = "QHZ1"},
      member_ref{.member = &coeffs::QHZ2, .name = "QHZ2"},
      member_ref{.member = &coeffs::QHZ3, .name = "QHZ3"},
      member_ref{.member = &coeffs::QHZ4, .name = "QHZ4"},
      member_ref{.member = &coeffs::SSZ1, .name = "SSZ1"},
      member_ref{.member = &coeffs::SSZ2, .name = "SSZ2"},
      member_ref{.member = &coeffs::SSZ3, .name = "SSZ3"},
      member_ref{.member = &coeffs::SSZ4, .name = "SSZ4"},
  };

  // One invalid coefficient value and the key the error must name.
  struct invalid_case {
    member_ptr member;
    std::string_view key;
    double value;
  };

  constexpr double QUIET_NAN = std::numeric_limits<double>::quiet_NaN();

  constexpr std::array<invalid_case, 12> INVALID_CASES{
      invalid_case{.member = &coeffs::FNOMIN, .key = "FNOMIN", .value = 0.0},
      invalid_case{.member = &coeffs::FNOMIN, .key = "FNOMIN", .value = -1.0},
      invalid_case{.member = &coeffs::FNOMIN, .key = "FNOMIN", .value = QUIET_NAN},
      invalid_case{.member = &coeffs::UNLOADED_RADIUS, .key = "UNLOADED_RADIUS", .value = 0.0},
      invalid_case{.member = &coeffs::UNLOADED_RADIUS, .key = "UNLOADED_RADIUS", .value = -0.3},
      invalid_case{.member = &coeffs::LFZO, .key = "LFZO", .value = 0.0},
      invalid_case{.member = &coeffs::LFZO, .key = "LFZO", .value = QUIET_NAN},
      invalid_case{.member = &coeffs::PKY2, .key = "PKY2", .value = 0.0},
      invalid_case{.member = &coeffs::LMUY, .key = "LMUY", .value = 0.0},
      invalid_case{.member = &coeffs::LONGVL, .key = "LONGVL", .value = 0.0},
      invalid_case{.member = &coeffs::LONGVL, .key = "LONGVL", .value = -16.6},
      invalid_case{.member = &coeffs::LONGVL, .key = "LONGVL", .value = QUIET_NAN},
  };

  class validate_coefficients_invalid : public ::testing::TestWithParam<invalid_case> {};

  // Every listed member of `values` must equal `expected` (defaults are passed through unchanged, so exact).
  void expect_members_equal(const coeffs &values, std::span<const member_ref> refs, double expected) {
    for (const member_ref &ref : refs) {
      ASSERT_TRUE(ref.member != nullptr);
      EXPECT_DOUBLE_EQ(values.*(ref.member), expected) << ref.name;
    }
  }
} // namespace

TEST(mf5x_coefficients, Defaults_Constructed_ScalingOneOthersZeroLongvlOne) {
  const coeffs defaults{};
  expect_members_equal(defaults, SCALING_MEMBERS, 1.0);
  expect_members_equal(defaults, ZERO_MEMBERS, 0.0);
  EXPECT_DOUBLE_EQ(defaults.LONGVL, 1.0);
}

TEST(mf5x_coefficients, Layout_Members_AreAllListedExactlyOnce) {
  std::vector<member_ptr> members;
  members.reserve(SCALING_MEMBERS.size() + ZERO_MEMBERS.size() + 1U);
  for (const member_ref &ref : SCALING_MEMBERS) {
    members.push_back(ref.member);
  }
  for (const member_ref &ref : ZERO_MEMBERS) {
    members.push_back(ref.member);
  }
  members.push_back(&coeffs::LONGVL);
  ASSERT_EQ(members.size(), 117U);
  for (std::size_t i = 0; i < members.size(); ++i) {
    for (std::size_t j = i + 1; j < members.size(); ++j) {
      EXPECT_TRUE(members[i] != members[j]) << "duplicate member at " << i << " and " << j;
    }
  }
}

TEST(mf5x_coefficients, Layout_Size_Is117Doubles) {
  EXPECT_EQ(sizeof(coeffs), std::size_t{117} * sizeof(double));
}

TEST(mf5x_coefficients, Parameters_DefaultConstructed_NoLimitsNoMetadataVxlowOne) {
  const mf::mf5x_parameters parameters{};
  EXPECT_FALSE(parameters.limits.fz_n.has_value());
  EXPECT_FALSE(parameters.limits.kappa.has_value());
  EXPECT_FALSE(parameters.limits.alpha_rad.has_value());
  EXPECT_FALSE(parameters.limits.gamma_rad.has_value());
  EXPECT_DOUBLE_EQ(parameters.limits.vxlow_mps, 1.0);
  EXPECT_FALSE(parameters.metadata.fittyp.has_value());
  EXPECT_FALSE(parameters.metadata.use_mode.has_value());
  EXPECT_EQ(parameters.metadata.side, mf::tyre_side::LEFT);
}

TEST(validate_coefficients, ValidateCoefficients_FixtureS_Succeeds) {
  const mf::mf5x_parameters parameters = mf::test::fixture_s();
  EXPECT_TRUE(mf::validate_coefficients(parameters.coefficients).has_value());
}

TEST(validate_coefficients, ValidateCoefficients_Defaults_FailsOnFnominFirst) {
  const auto result = mf::validate_coefficients(coeffs{});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().where.key, "FNOMIN");
}

TEST(validate_coefficients, ValidateCoefficients_SeveralInvalid_ReportsFirstInDocumentedOrder) {
  coeffs c = mf::test::fixture_s().coefficients;
  c.LONGVL = 0.0;
  c.LMUY = 0.0;
  c.PKY2 = 0.0;
  auto result = mf::validate_coefficients(c);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().where.key, "PKY2");
  c.PKY2 = 1.5;
  result = mf::validate_coefficients(c);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().where.key, "LMUY");
  c.LMUY = 1.0;
  result = mf::validate_coefficients(c);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().where.key, "LONGVL");
}

TEST(validate_coefficients, ValidateCoefficients_NegativeFnomin_MessageNamesKeyValueAndDecision) {
  coeffs c = mf::test::fixture_s().coefficients;
  c.FNOMIN = -1.0;
  const auto result = mf::validate_coefficients(c);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message, "FNOMIN must be > 0 (got -1) (AD-12)");
}

TEST(validate_coefficients, ValidateCoefficients_NegativeLmuy_IsAcceptedAsNonZero) {
  coeffs c = mf::test::fixture_s().coefficients;
  c.LMUY = -1.0;
  c.PKY2 = -1.5;
  EXPECT_TRUE(mf::validate_coefficients(c).has_value());
}

TEST_P(validate_coefficients_invalid, ValidateCoefficients_InvalidValue_ReturnsInvalidValueWithKey) {
  const invalid_case &param = GetParam();
  coeffs c = mf::test::fixture_s().coefficients;
  c.*(param.member) = param.value;
  const auto result = mf::validate_coefficients(c);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_VALUE);
  EXPECT_EQ(result.error().where.key, param.key);
  EXPECT_NE(result.error().message.find(std::string{param.key}), std::string::npos);
  EXPECT_NE(result.error().message.find("AD-12"), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(cases, validate_coefficients_invalid, ::testing::ValuesIn(INVALID_CASES));
