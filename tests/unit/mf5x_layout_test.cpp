#include <array>
#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/sign_transform.hpp>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {
  namespace layout = mf::mf5x;

  void expect_inputs(std::span<const mf::input_channel> channels) {
    const std::vector<std::string_view> names{"FZ", "SA", "SR", "IA", "VX"};
    const std::vector<std::string_view> units{"N", "rad", "-", "rad", "m/s"};
    ASSERT_EQ(channels.size(), names.size());
    for (std::size_t i = 0; i < names.size(); ++i) {
      EXPECT_EQ(channels[i].name, names[i]);
      EXPECT_EQ(channels[i].unit, units[i]);
    }
  }

  void expect_output(std::size_t index, std::string_view name, std::string_view unit) {
    EXPECT_EQ(layout::OUTPUTS.at(index).name, name);
    EXPECT_EQ(layout::OUTPUTS.at(index).unit, unit);
  }

  bool is_negative_infinity(double value) {
    return std::isinf(value) && value < 0.0;
  }

  bool is_positive_infinity(double value) {
    return std::isinf(value) && value > 0.0;
  }

  void expect_limit_pair(std::size_t lower_index) {
    EXPECT_EQ(layout::LIMITS.at(lower_index).role, mf::limit_role::LOWER);
    EXPECT_TRUE(is_negative_infinity(layout::LIMITS.at(lower_index).default_value));
    EXPECT_EQ(layout::LIMITS.at(lower_index + 1).role, mf::limit_role::UPPER);
    EXPECT_TRUE(is_positive_infinity(layout::LIMITS.at(lower_index + 1).default_value));
  }
} // namespace

TEST(mf5x_layout, Lengths_EqualTableSizes) {
  EXPECT_EQ(static_cast<std::size_t>(layout::INPUT_LENGTH), layout::PAC2002_INPUTS.size());
  EXPECT_EQ(static_cast<std::size_t>(layout::INPUT_LENGTH), layout::MF52_INPUTS.size());
  EXPECT_EQ(static_cast<std::size_t>(layout::OUTPUT_LENGTH), layout::OUTPUTS.size());
  EXPECT_EQ(static_cast<std::size_t>(layout::LIMITS_LENGTH), layout::LIMITS.size());
  EXPECT_EQ(static_cast<std::size_t>(layout::INPUT_LENGTH), layout::PAC2002_LAYOUT.inputs.size());
  EXPECT_EQ(static_cast<std::size_t>(layout::INPUT_LENGTH), layout::MF52_LAYOUT.inputs.size());
  EXPECT_EQ(static_cast<std::size_t>(layout::OUTPUT_LENGTH), layout::PAC2002_LAYOUT.outputs.size());
  EXPECT_EQ(static_cast<std::size_t>(layout::LIMITS_LENGTH), layout::MF52_LAYOUT.limits.size());
}

TEST(mf5x_layout, InputChannels_NamesAndUnitsInIndexOrder) {
  expect_inputs(layout::PAC2002_INPUTS);
  expect_inputs(layout::MF52_INPUTS);
}

TEST(mf5x_layout, OutputChannels_NamesAndUnitsInIndexOrder) {
  ASSERT_EQ(layout::OUTPUTS.size(), static_cast<std::size_t>(layout::OUTPUT_LENGTH));
  expect_output(layout::FX, "FX", "N");
  expect_output(layout::FY, "FY", "N");
  expect_output(layout::MZ, "MZ", "N*m");
  expect_output(layout::MX, "MX", "N*m");
  expect_output(layout::MY, "MY", "N*m");
  expect_output(layout::FZ_CLAMPED, "FZ_CLAMPED", "-");
  expect_output(layout::SR_CLAMPED, "SR_CLAMPED", "-");
  expect_output(layout::SA_CLAMPED, "SA_CLAMPED", "-");
  expect_output(layout::IA_CLAMPED, "IA_CLAMPED", "-");
  expect_output(layout::BELOW_VXLOW, "BELOW_VXLOW", "-");
}

TEST(mf5x_layout, Limits_Keys) {
  ASSERT_EQ(layout::LIMITS.size(), static_cast<std::size_t>(layout::LIMITS_LENGTH));
  const std::vector<std::pair<std::size_t, std::string_view>> keys{
      {layout::FZMIN, "FZMIN"},   {layout::FZMAX, "FZMAX"},   {layout::KPUMIN, "KPUMIN"},
      {layout::KPUMAX, "KPUMAX"}, {layout::ALPMIN, "ALPMIN"}, {layout::ALPMAX, "ALPMAX"},
      {layout::CAMMIN, "CAMMIN"}, {layout::CAMMAX, "CAMMAX"}, {layout::VXLOW, "VXLOW"}};
  for (const auto &[index, key] : keys) {
    EXPECT_EQ(layout::LIMITS.at(index).key, key);
  }
}

TEST(mf5x_layout, Limits_RolesAndDefaults) {
  for (std::size_t i = 0; i + 1 < layout::LIMITS.size(); i += 2) {
    expect_limit_pair(i);
  }
  EXPECT_EQ(layout::LIMITS.at(layout::VXLOW).role, mf::limit_role::VALUE);
  EXPECT_EQ(layout::LIMITS.at(layout::VXLOW).default_value, 1.0);
}

TEST(mf5x_layout, Pac2002Vx_MinimumZeroInclusive) {
  const mf::input_channel &vx = layout::PAC2002_INPUTS.at(layout::VX);
  EXPECT_EQ(vx.minimum, 0.0);
  EXPECT_TRUE(vx.minimum_inclusive);
}

TEST(mf5x_layout, Mf52Vx_MinimumZeroExclusive) {
  const mf::input_channel &vx = layout::MF52_INPUTS.at(layout::VX);
  EXPECT_EQ(vx.minimum, 0.0);
  EXPECT_FALSE(vx.minimum_inclusive);
}

TEST(mf5x_layout, NonVxInputs_MinusInfinityInclusive) {
  const std::array<layout::input_index, 4> others{layout::FZ, layout::SA, layout::SR, layout::IA};
  for (const layout::input_index index : others) {
    EXPECT_TRUE(is_negative_infinity(layout::PAC2002_INPUTS.at(index).minimum));
    EXPECT_TRUE(layout::PAC2002_INPUTS.at(index).minimum_inclusive);
    EXPECT_TRUE(is_negative_infinity(layout::MF52_INPUTS.at(index).minimum));
    EXPECT_TRUE(layout::MF52_INPUTS.at(index).minimum_inclusive);
  }
}

TEST(mf5x_layout, Layouts_ReferenceTheTables) {
  EXPECT_EQ(layout::PAC2002_LAYOUT.inputs.data(), layout::PAC2002_INPUTS.data());
  EXPECT_EQ(layout::PAC2002_LAYOUT.outputs.data(), layout::OUTPUTS.data());
  EXPECT_EQ(layout::PAC2002_LAYOUT.limits.data(), layout::LIMITS.data());
  EXPECT_EQ(layout::MF52_LAYOUT.inputs.data(), layout::MF52_INPUTS.data());
  EXPECT_EQ(layout::MF52_LAYOUT.outputs.data(), layout::OUTPUTS.data());
  EXPECT_EQ(layout::MF52_LAYOUT.limits.data(), layout::LIMITS.data());
}

TEST(mf5x_layout, Characteristics_EightChannelsInEnumOrder_NamesAndUnits) {
  const std::vector<std::string_view> names{"DFY_DSA", "DFX_DSR", "FY_OVER_SA",    "FX_OVER_SR",
                                            "DY",      "DX",      "SA_AT_PEAK_FY", "SR_AT_PEAK_FX"};
  const std::vector<std::string_view> units{"N/rad", "N", "N/rad", "N", "N", "N", "rad", "-"};
  ASSERT_EQ(layout::CHARACTERISTICS.size(), static_cast<std::size_t>(layout::CHARACTERISTICS_LENGTH));
  for (std::size_t i = 0; i < names.size(); ++i) {
    EXPECT_EQ(layout::CHARACTERISTICS.at(i).name, names[i]);
    EXPECT_EQ(layout::CHARACTERISTICS.at(i).unit, units[i]);
  }
  EXPECT_EQ(static_cast<int>(layout::DFY_DSA), 0);
  EXPECT_EQ(static_cast<int>(layout::SR_AT_PEAK_FX), 7);
}

TEST(mf5x_layout, Characteristics_Parity_MatchesTable) {
  // Columns: vertical, lateral.
  const std::array<std::array<bool, 2>, 8> expected{{{true, true},
                                                     {false, false},
                                                     {true, true},
                                                     {false, false},
                                                     {true, false},
                                                     {false, false},
                                                     {false, true},
                                                     {false, false}}};
  for (std::size_t i = 0; i < layout::CHARACTERISTICS.size(); ++i) {
    EXPECT_EQ(layout::CHARACTERISTICS.at(i).negated_by.vertical, expected.at(i).at(0))
        << layout::CHARACTERISTICS.at(i).name;
    EXPECT_EQ(layout::CHARACTERISTICS.at(i).negated_by.lateral, expected.at(i).at(1))
        << layout::CHARACTERISTICS.at(i).name;
  }
}

TEST(mf5x_layout, Characteristics_NanAllowed_PeaksOnly) {
  for (std::size_t i = 0; i < layout::CHARACTERISTICS.size(); ++i) {
    const bool expected = i == std::size_t{layout::SA_AT_PEAK_FY} || i == std::size_t{layout::SR_AT_PEAK_FX};
    EXPECT_EQ(layout::CHARACTERISTICS.at(i).nan_allowed, expected) << layout::CHARACTERISTICS.at(i).name;
  }
}

TEST(mf5x_layout, Layouts_BothReferenceCharacteristicsTable) {
  EXPECT_EQ(layout::PAC2002_LAYOUT.characteristics.data(), layout::CHARACTERISTICS.data());
  EXPECT_EQ(layout::MF52_LAYOUT.characteristics.data(), layout::CHARACTERISTICS.data());
  EXPECT_EQ(layout::PAC2002_LAYOUT.characteristics.size(), 8U);
  EXPECT_EQ(layout::MF52_LAYOUT.characteristics.size(), 8U);
}

TEST(mf5x_layout, Enumerators_AreVectorPositions) {
  EXPECT_EQ(static_cast<int>(layout::FZ), 0);
  EXPECT_EQ(static_cast<int>(layout::VX), 4);
  EXPECT_EQ(static_cast<int>(layout::FX), 0);
  EXPECT_EQ(static_cast<int>(layout::BELOW_VXLOW), 9);
  EXPECT_EQ(static_cast<int>(layout::VXLOW), 8);
}

TEST(mf5x_layout, NegatedBy_Pac2002Inputs_FzVerticalSaLateralRestNone) {
  for (std::size_t i = 0; i < layout::PAC2002_INPUTS.size(); ++i) {
    mf::sign_transform expected = mf::sign_transform::NONE;
    if (i == std::size_t{layout::FZ}) {
      expected = mf::sign_transform::VERTICAL;
    }
    if (i == std::size_t{layout::SA}) {
      expected = mf::sign_transform::LATERAL;
    }
    EXPECT_EQ(layout::PAC2002_INPUTS.at(i).negated_by, expected) << layout::PAC2002_INPUTS.at(i).name;
  }
}

TEST(mf5x_layout, NegatedBy_Mf52Inputs_FzVerticalSaLateralRestNone) {
  for (std::size_t i = 0; i < layout::MF52_INPUTS.size(); ++i) {
    mf::sign_transform expected = mf::sign_transform::NONE;
    if (i == std::size_t{layout::FZ}) {
      expected = mf::sign_transform::VERTICAL;
    }
    if (i == std::size_t{layout::SA}) {
      expected = mf::sign_transform::LATERAL;
    }
    EXPECT_EQ(layout::MF52_INPUTS.at(i).negated_by, expected) << layout::MF52_INPUTS.at(i).name;
  }
}

TEST(mf5x_layout, NegatedBy_Outputs_FyMzMyVerticalRestNone) {
  for (std::size_t i = 0; i < layout::OUTPUTS.size(); ++i) {
    const bool vertical = i == std::size_t{layout::FY} || i == std::size_t{layout::MZ} || i == std::size_t{layout::MY};
    const mf::sign_transform expected = vertical ? mf::sign_transform::VERTICAL : mf::sign_transform::NONE;
    EXPECT_EQ(layout::OUTPUTS.at(i).negated_by, expected) << layout::OUTPUTS.at(i).name;
  }
}

TEST(mf5x_layout, NegatedBy_Limits_FzVerticalAlpLateralRestNone) {
  for (std::size_t i = 0; i < layout::LIMITS.size(); ++i) {
    mf::sign_transform expected = mf::sign_transform::NONE;
    if (i == std::size_t{layout::FZMIN} || i == std::size_t{layout::FZMAX}) {
      expected = mf::sign_transform::VERTICAL;
    }
    if (i == std::size_t{layout::ALPMIN} || i == std::size_t{layout::ALPMAX}) {
      expected = mf::sign_transform::LATERAL;
    }
    EXPECT_EQ(layout::LIMITS.at(i).negated_by, expected) << layout::LIMITS.at(i).key;
  }
}

TEST(mf5x_layout, LimitSignsWellFormed_Limits_True) {
  const std::vector<mf::limit_channel> channels(layout::LIMITS.begin(), layout::LIMITS.end());
  EXPECT_TRUE(mf::limit_signs_well_formed(channels));
}

TEST(mf5x_layout, LimitPairs_WellFormedAtRuntime) {
  const std::vector<mf::limit_channel> channels(layout::LIMITS.begin(), layout::LIMITS.end());
  EXPECT_TRUE(mf::limit_pairs_well_formed(channels));
}
