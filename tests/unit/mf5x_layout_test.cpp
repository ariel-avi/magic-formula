#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/model_layout.hpp>
#include <span>
#include <string_view>
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
    EXPECT_EQ(layout::OUTPUTS[index].name, name);
    EXPECT_EQ(layout::OUTPUTS[index].unit, unit);
  }

  bool is_negative_infinity(double value) {
    return std::isinf(value) && value < 0.0;
  }

  bool is_positive_infinity(double value) {
    return std::isinf(value) && value > 0.0;
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

TEST(mf5x_layout, Limits_KeysRolesDefaults) {
  const std::span<const mf::limit_channel> channels = layout::LIMITS;
  ASSERT_EQ(channels.size(), static_cast<std::size_t>(layout::LIMITS_LENGTH));
  EXPECT_EQ(channels[layout::FZMIN].key, "FZMIN");
  EXPECT_EQ(channels[layout::FZMAX].key, "FZMAX");
  EXPECT_EQ(channels[layout::KPUMIN].key, "KPUMIN");
  EXPECT_EQ(channels[layout::KPUMAX].key, "KPUMAX");
  EXPECT_EQ(channels[layout::ALPMIN].key, "ALPMIN");
  EXPECT_EQ(channels[layout::ALPMAX].key, "ALPMAX");
  EXPECT_EQ(channels[layout::CAMMIN].key, "CAMMIN");
  EXPECT_EQ(channels[layout::CAMMAX].key, "CAMMAX");
  EXPECT_EQ(channels[layout::VXLOW].key, "VXLOW");
  for (std::size_t i = 0; i + 1 < channels.size(); i += 2) {
    EXPECT_EQ(channels[i].role, mf::limit_role::LOWER);
    EXPECT_TRUE(is_negative_infinity(channels[i].default_value));
    EXPECT_EQ(channels[i + 1].role, mf::limit_role::UPPER);
    EXPECT_TRUE(is_positive_infinity(channels[i + 1].default_value));
  }
  EXPECT_EQ(channels[layout::VXLOW].role, mf::limit_role::VALUE);
  EXPECT_EQ(channels[layout::VXLOW].default_value, 1.0);
}

TEST(mf5x_layout, Pac2002Vx_MinimumZeroInclusive) {
  const mf::input_channel &vx = layout::PAC2002_INPUTS[layout::VX];
  EXPECT_EQ(vx.minimum, 0.0);
  EXPECT_TRUE(vx.minimum_inclusive);
}

TEST(mf5x_layout, Mf52Vx_MinimumZeroExclusive) {
  const mf::input_channel &vx = layout::MF52_INPUTS[layout::VX];
  EXPECT_EQ(vx.minimum, 0.0);
  EXPECT_FALSE(vx.minimum_inclusive);
}

TEST(mf5x_layout, NonVxInputs_MinusInfinityInclusive) {
  const layout::input_index others[] = {layout::FZ, layout::SA, layout::SR, layout::IA};
  for (const layout::input_index index : others) {
    EXPECT_TRUE(is_negative_infinity(layout::PAC2002_INPUTS[index].minimum));
    EXPECT_TRUE(layout::PAC2002_INPUTS[index].minimum_inclusive);
    EXPECT_TRUE(is_negative_infinity(layout::MF52_INPUTS[index].minimum));
    EXPECT_TRUE(layout::MF52_INPUTS[index].minimum_inclusive);
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

TEST(mf5x_layout, Enumerators_AreVectorPositions) {
  EXPECT_EQ(static_cast<int>(layout::FZ), 0);
  EXPECT_EQ(static_cast<int>(layout::VX), 4);
  EXPECT_EQ(static_cast<int>(layout::FX), 0);
  EXPECT_EQ(static_cast<int>(layout::BELOW_VXLOW), 9);
  EXPECT_EQ(static_cast<int>(layout::VXLOW), 8);
}

TEST(mf5x_layout, LimitPairs_WellFormedAtRuntime) {
  const std::vector<mf::limit_channel> channels(layout::LIMITS.begin(), layout::LIMITS.end());
  EXPECT_TRUE(mf::limit_pairs_well_formed(channels));
}
