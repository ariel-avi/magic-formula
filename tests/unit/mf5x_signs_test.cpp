#include "../support/fixtures.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <magic_formula/io/mf5x_bindings.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_signs.hpp>
#include <magic_formula/sign_transform.hpp>
#include <optional>
#include <span>
#include <vector>

namespace {
  using coeffs = mf::mf5x_coefficients;
  using member_ptr = double coeffs::*;
  using mf::sign_transform;
  using mf::sign_transforms;
  using mf::test::bits;
  using mf::test::random_profile;

  constexpr std::size_t SEED_COUNT = 20;

  // The four conversion plans: identity, VERTICAL, LATERAL and both.
  constexpr std::array<sign_transforms, 4> ALL_SETS{{
      {.vertical = false, .lateral = false},
      {.vertical = true, .lateral = false},
      {.vertical = false, .lateral = true},
      {.vertical = true, .lateral = true},
  }};

  // The table row of `member`, if it has one.
  std::optional<mf::mf5x::coefficient_sign> row_of(member_ptr member) {
    for (const mf::mf5x::coefficient_sign &row : mf::mf5x::COEFFICIENT_SIGNS) {
      if (row.member == member) {
        return row;
      }
    }
    return std::nullopt;
  }

  coeffs wild(std::uint64_t seed) {
    return mf::test::random_mf5x_parameters(seed, random_profile::WILD).coefficients;
  }
} // namespace

TEST(mf5x_signs, Table_Has30Rows15Vertical15LateralNoNone) {
  const std::span<const mf::mf5x::coefficient_sign> rows = mf::mf5x::COEFFICIENT_SIGNS;
  EXPECT_EQ(rows.size(), 30U);
  EXPECT_EQ(mf::mf5x::count_negated_by(rows, sign_transform::VERTICAL), 15U);
  EXPECT_EQ(mf::mf5x::count_negated_by(rows, sign_transform::LATERAL), 15U);
  EXPECT_EQ(mf::mf5x::count_negated_by(rows, sign_transform::NONE), 0U);
}

TEST(mf5x_signs, Table_KeysUnique) {
  EXPECT_TRUE(mf::mf5x::coefficient_keys_unique(mf::mf5x::COEFFICIENT_SIGNS));
}

TEST(mf5x_signs, Table_KeyEqualsBindingKeyOfSameMember) {
  for (const mf::mf5x::coefficient_sign &row : mf::mf5x::COEFFICIENT_SIGNS) {
    bool found = false;
    for (const mf::io::coefficient_binding &binding : mf::io::mf5x_common_bindings()) {
      if (binding.member == row.member) {
        found = true;
        EXPECT_EQ(binding.key, row.key);
      }
    }
    EXPECT_TRUE(found) << row.key;
  }
}

TEST(mf5x_signs, Table_NoScalingFactor) {
  for (const mf::mf5x::coefficient_sign &row : mf::mf5x::COEFFICIENT_SIGNS) {
    for (const mf::io::coefficient_binding &binding : mf::io::mf5x_common_bindings()) {
      if (binding.member == row.member) {
        EXPECT_NE(binding.role, mf::io::binding_role::SCALING) << row.key;
      }
    }
    for (const mf::io::coefficient_binding &binding : mf::io::pac2002_camber_bindings()) {
      EXPECT_TRUE(binding.member != row.member) << row.key;
    }
  }
}

TEST(mf5x_signs, TransformCoefficients_EachSet_NegatesExactlyTableMembers) {
  const coeffs original = wild(1);
  const auto members = mf::test::all_coefficient_members();
  ASSERT_EQ(members.size(), 117U);
  for (const sign_transforms &set : ALL_SETS) {
    const coeffs result = mf::mf5x::transform_coefficients(original, set);
    for (const auto &[key, member] : members) {
      const std::optional<mf::mf5x::coefficient_sign> row = row_of(member);
      const bool negated = row.has_value() && mf::contains(set, row->negated_by);
      const double expected = negated ? -(original.*member) : original.*member;
      EXPECT_EQ(bits(result.*member), bits(expected))
          << key << " (vertical " << set.vertical << ", lateral " << set.lateral << ")";
    }
  }
}

TEST(mf5x_signs, TransformCoefficients_AppliedTwice_BitIdentical) {
  for (const sign_transforms &set : ALL_SETS) {
    for (std::uint64_t seed = 1; seed <= SEED_COUNT; ++seed) {
      const coeffs original = wild(seed);
      const coeffs twice = mf::mf5x::transform_coefficients(mf::mf5x::transform_coefficients(original, set), set);
      EXPECT_TRUE(mf::test::coefficients_bitwise_equal(twice, original)) << "seed " << seed;
    }
  }
}

TEST(mf5x_signs, TransformCoefficients_VerticalThenLateral_EqualsLateralThenVertical) {
  const sign_transforms vertical{.vertical = true, .lateral = false};
  const sign_transforms lateral{.vertical = false, .lateral = true};
  for (std::uint64_t seed = 1; seed <= SEED_COUNT; ++seed) {
    const coeffs original = wild(seed);
    const coeffs vertical_first =
        mf::mf5x::transform_coefficients(mf::mf5x::transform_coefficients(original, vertical), lateral);
    const coeffs lateral_first =
        mf::mf5x::transform_coefficients(mf::mf5x::transform_coefficients(original, lateral), vertical);
    EXPECT_TRUE(mf::test::coefficients_bitwise_equal(vertical_first, lateral_first)) << "seed " << seed;
  }
}

TEST(mf5x_signs, TransformCoefficients_BothAtOnce_EqualsSequential) {
  const sign_transforms vertical{.vertical = true, .lateral = false};
  const sign_transforms lateral{.vertical = false, .lateral = true};
  const sign_transforms both{.vertical = true, .lateral = true};
  for (std::uint64_t seed = 1; seed <= SEED_COUNT; ++seed) {
    const coeffs original = wild(seed);
    const coeffs at_once = mf::mf5x::transform_coefficients(original, both);
    const coeffs sequential =
        mf::mf5x::transform_coefficients(mf::mf5x::transform_coefficients(original, vertical), lateral);
    EXPECT_TRUE(mf::test::coefficients_bitwise_equal(at_once, sequential)) << "seed " << seed;
  }
}

TEST(mf5x_signs, CountAndUniqueHelpers_CraftedTables_DetectDuplicatesAndCounts) {
  const std::vector<mf::mf5x::coefficient_sign> duplicate{
      {.key = "A", .member = &coeffs::FNOMIN, .negated_by = sign_transform::VERTICAL},
      {.key = "B", .member = &coeffs::PKY1, .negated_by = sign_transform::LATERAL},
      {.key = "A", .member = &coeffs::PDX1, .negated_by = sign_transform::VERTICAL},
  };
  EXPECT_EQ(mf::mf5x::count_negated_by(duplicate, sign_transform::VERTICAL), 2U);
  EXPECT_EQ(mf::mf5x::count_negated_by(duplicate, sign_transform::LATERAL), 1U);
  EXPECT_EQ(mf::mf5x::count_negated_by(duplicate, sign_transform::NONE), 0U);
  EXPECT_FALSE(mf::mf5x::coefficient_keys_unique(duplicate));

  const std::vector<mf::mf5x::coefficient_sign> distinct{duplicate[0], duplicate[1]};
  EXPECT_TRUE(mf::mf5x::coefficient_keys_unique(distinct));
  EXPECT_TRUE(mf::mf5x::coefficient_keys_unique(std::span<const mf::mf5x::coefficient_sign>{}));
}
