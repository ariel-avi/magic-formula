#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/detail/identification_probes.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/pac2002_model.hpp>
#include <span>
#include <string_view>
#include <vector>

namespace {
  using mf::detail::identification_probes;
  using mf::detail::probe_slot;
  using mf::test::bits;

  constexpr double QNAN = std::numeric_limits<double>::quiet_NaN();

  constexpr mf::input_channel CHANNEL_FZ{.name = "FZ", .unit = "N"};
  constexpr mf::input_channel CHANNEL_A{.name = "A", .unit = "-"};
  constexpr mf::input_channel CHANNEL_B{.name = "B", .unit = "-"};
  constexpr mf::output_channel CHANNEL_Y{.name = "Y", .unit = "N"};
  constexpr mf::output_channel CHANNEL_Y_CLAMPED{.name = "Y_CLAMPED", .unit = "-"};
  constexpr mf::output_channel CHANNEL_BELOW_VXLOW{.name = "BELOW_VXLOW", .unit = "-"};
  constexpr std::array<mf::input_channel, 3> INPUTS{CHANNEL_FZ, CHANNEL_A, CHANNEL_B};
  constexpr std::array<mf::input_channel, 2> INPUTS_NO_FZ{CHANNEL_A, CHANNEL_B};
  constexpr std::array<mf::output_channel, 3> OUTPUTS{CHANNEL_Y, CHANNEL_Y_CLAMPED, CHANNEL_BELOW_VXLOW};
  constexpr mf::model_layout LAYOUT{.inputs = INPUTS, .outputs = OUTPUTS, .limits = {}};
  constexpr mf::model_layout LAYOUT_NO_FZ{.inputs = INPUTS_NO_FZ, .outputs = OUTPUTS, .limits = {}};

  constexpr double STEP = 0.25;

  Eigen::VectorXd reference_point() {
    Eigen::VectorXd reference(3);
    reference << 4000.0, 0.5, 0.25;
    return reference;
  }

  mf::indicator_definition definition(mf::indicator_form form, std::string_view channel, std::string_view response,
                                      bool sweep) {
    return mf::indicator_definition{.name = "probe",
                                    .meaning = "crafted",
                                    .unit = "N",
                                    .axis = mf::identification_axis::VERTICAL,
                                    .form = form,
                                    .channel = channel,
                                    .response = response,
                                    .step = STEP,
                                    .polarity = 1.0,
                                    .weight = 1.0,
                                    .threshold = 1e-6,
                                    .load_sweep = sweep};
  }

  identification_probes plan(const std::vector<mf::indicator_definition> &table) {
    return mf::detail::plan_probes(LAYOUT, reference_point(), table);
  }

  std::vector<mf::indicator_reading> readings_of(const std::vector<mf::indicator_definition> &table,
                                                 const identification_probes &probes, const Eigen::MatrixXd &outputs) {
    return mf::detail::read_probes(LAYOUT, table, probes, outputs);
  }
} // namespace

TEST(identification_probes, PlanProbes_InputLevel_NoRowsInputValueAtProbe) {
  const identification_probes probes = plan({definition(mf::indicator_form::INPUT_LEVEL, "A", "", false)});
  EXPECT_EQ(probes.inputs.rows(), 0);
  EXPECT_EQ(probes.inputs.cols(), 3);
  ASSERT_EQ(probes.slots.size(), 1U);
  const probe_slot &slot = probes.slots[0];
  EXPECT_EQ(slot.definition, 0U);
  EXPECT_EQ(bits(slot.load_scale), bits(1.0));
  EXPECT_EQ(bits(slot.load_n), bits(4000.0));
  EXPECT_EQ(bits(slot.input_value), bits(0.5));
  EXPECT_EQ(slot.plus_row, -1);
  EXPECT_EQ(slot.minus_row, -1);
  EXPECT_EQ(slot.output, -1);
  EXPECT_FALSE(slot.missing_channel);
}

TEST(identification_probes, PlanProbes_InputLevelFz_SweptValuesAreSignedLoads) {
  const identification_probes probes = plan({definition(mf::indicator_form::INPUT_LEVEL, "FZ", "", true)});
  ASSERT_EQ(probes.slots.size(), 3U);
  EXPECT_EQ(bits(probes.slots[0].input_value), bits(2000.0));
  EXPECT_EQ(bits(probes.slots[1].input_value), bits(4000.0));
  EXPECT_EQ(bits(probes.slots[2].input_value), bits(6000.0));
}

TEST(identification_probes, PlanProbes_OutputLevel_OneRowAtProbeLoad) {
  const identification_probes probes = plan({definition(mf::indicator_form::OUTPUT_LEVEL, "", "Y", false)});
  ASSERT_EQ(probes.inputs.rows(), 1);
  ASSERT_EQ(probes.slots.size(), 1U);
  EXPECT_EQ(probes.slots[0].plus_row, 0);
  EXPECT_EQ(probes.slots[0].minus_row, -1);
  EXPECT_EQ(probes.slots[0].output, 0);
  EXPECT_EQ(bits(probes.inputs(0, 0)), bits(4000.0));
  EXPECT_EQ(bits(probes.inputs(0, 1)), bits(0.5));
  EXPECT_EQ(bits(probes.inputs(0, 2)), bits(0.25));
}

TEST(identification_probes, PlanProbes_CentralDifference_TwoRowsPlusMinusStep) {
  const identification_probes probes = plan({definition(mf::indicator_form::CENTRAL_DIFFERENCE, "B", "Y", false)});
  ASSERT_EQ(probes.inputs.rows(), 2);
  ASSERT_EQ(probes.slots.size(), 1U);
  EXPECT_EQ(probes.slots[0].plus_row, 0);
  EXPECT_EQ(probes.slots[0].minus_row, 1);
  EXPECT_EQ(probes.slots[0].output, 0);
  EXPECT_EQ(bits(probes.inputs(0, 2)), bits(0.25 + STEP));
  EXPECT_EQ(bits(probes.inputs(1, 2)), bits(0.25 - STEP));
  for (Eigen::Index r = 0; r < 2; ++r) {
    EXPECT_EQ(bits(probes.inputs(r, 0)), bits(4000.0));
    EXPECT_EQ(bits(probes.inputs(r, 1)), bits(0.5));
  }
}

TEST(identification_probes, PlanProbes_LoadSweep_ThreeSlotsInScaleOrder) {
  const identification_probes probes = plan({definition(mf::indicator_form::CENTRAL_DIFFERENCE, "A", "Y", true)});
  ASSERT_EQ(probes.slots.size(), 3U);
  ASSERT_EQ(probes.inputs.rows(), 6);
  const std::array<double, 3> scales{0.5, 1.0, 1.5};
  const std::array<double, 3> loads{2000.0, 4000.0, 6000.0};
  for (std::size_t i = 0; i < 3; ++i) {
    const probe_slot &slot = probes.slots[i];
    const auto row = static_cast<Eigen::Index>(2 * i);
    EXPECT_EQ(bits(slot.load_scale), bits(scales[i]));
    EXPECT_EQ(bits(slot.load_n), bits(loads[i]));
    EXPECT_EQ(slot.plus_row, row);
    EXPECT_EQ(slot.minus_row, row + 1);
    EXPECT_EQ(bits(probes.inputs(row, 0)), bits(loads[i]));
    EXPECT_EQ(bits(probes.inputs(row + 1, 0)), bits(loads[i]));
    EXPECT_EQ(bits(probes.inputs(row, 1)), bits(0.5 + STEP));
    EXPECT_EQ(bits(probes.inputs(row + 1, 1)), bits(0.5 - STEP));
  }
}

TEST(identification_probes, PlanProbes_NoSweep_OneSlotAtScaleOne) {
  std::vector<mf::indicator_definition> table;
  table.push_back(definition(mf::indicator_form::OUTPUT_LEVEL, "", "Y", false));
  table.push_back(definition(mf::indicator_form::OUTPUT_LEVEL, "", "Y", true));
  const identification_probes probes = plan(table);
  ASSERT_EQ(probes.slots.size(), 4U);
  EXPECT_EQ(probes.slots[0].definition, 0U);
  EXPECT_EQ(bits(probes.slots[0].load_scale), bits(1.0));
  for (std::size_t i = 1; i < 4; ++i) {
    EXPECT_EQ(probes.slots[i].definition, 1U);
  }
  EXPECT_EQ(probes.inputs.rows(), 4);
}

TEST(identification_probes, PlanProbes_MissingFz_EverySlotMissingNoRows) {
  Eigen::VectorXd reference(2);
  reference << 0.5, 0.25;
  std::vector<mf::indicator_definition> table;
  table.push_back(definition(mf::indicator_form::INPUT_LEVEL, "A", "", false));
  table.push_back(definition(mf::indicator_form::OUTPUT_LEVEL, "", "Y", true));
  table.push_back(definition(mf::indicator_form::CENTRAL_DIFFERENCE, "B", "Y", false));
  const identification_probes probes = mf::detail::plan_probes(LAYOUT_NO_FZ, reference, table);
  EXPECT_EQ(probes.inputs.rows(), 0);
  EXPECT_EQ(probes.inputs.cols(), 2);
  ASSERT_EQ(probes.slots.size(), 5U);
  for (const probe_slot &slot : probes.slots) {
    EXPECT_TRUE(slot.missing_channel);
    EXPECT_EQ(bits(slot.load_n), bits(0.0));
    EXPECT_EQ(slot.plus_row, -1);
    EXPECT_EQ(slot.minus_row, -1);
    EXPECT_EQ(slot.output, -1);
  }
}

TEST(identification_probes, PlanProbes_MissingChannel_SlotMissing) {
  std::vector<mf::indicator_definition> table;
  table.push_back(definition(mf::indicator_form::INPUT_LEVEL, "Z", "", false));
  table.push_back(definition(mf::indicator_form::CENTRAL_DIFFERENCE, "Z", "Y", false));
  table.push_back(definition(mf::indicator_form::OUTPUT_LEVEL, "Z", "Y", false));
  const identification_probes probes = plan(table);
  ASSERT_EQ(probes.slots.size(), 3U);
  EXPECT_TRUE(probes.slots[0].missing_channel);
  EXPECT_TRUE(probes.slots[1].missing_channel);
  // The channel is not used by an OUTPUT_LEVEL definition.
  EXPECT_FALSE(probes.slots[2].missing_channel);
  EXPECT_EQ(probes.inputs.rows(), 1);
  EXPECT_EQ(bits(probes.slots[0].load_n), bits(4000.0));
  EXPECT_EQ(probes.slots[0].output, -1);
}

TEST(identification_probes, PlanProbes_MissingResponse_SlotMissing) {
  std::vector<mf::indicator_definition> table;
  table.push_back(definition(mf::indicator_form::OUTPUT_LEVEL, "", "Q", false));
  table.push_back(definition(mf::indicator_form::CENTRAL_DIFFERENCE, "A", "Q", false));
  table.push_back(definition(mf::indicator_form::INPUT_LEVEL, "A", "Q", false));
  const identification_probes probes = plan(table);
  ASSERT_EQ(probes.slots.size(), 3U);
  EXPECT_TRUE(probes.slots[0].missing_channel);
  EXPECT_TRUE(probes.slots[1].missing_channel);
  // The response is not used by an INPUT_LEVEL definition.
  EXPECT_FALSE(probes.slots[2].missing_channel);
  EXPECT_EQ(probes.inputs.rows(), 0);
  EXPECT_EQ(probes.slots[1].output, -1);
}

TEST(identification_probes, PlanProbes_DefaultTableOnPac2002_29Slots55Rows) {
  const auto model = mf::pac2002_model::create(mf::test::fixture_s());
  ASSERT_TRUE(model.has_value());
  const identification_probes probes =
      mf::detail::plan_probes(model->layout(), model->reference_input(), mf::IDENTIFICATION_INDICATORS);
  EXPECT_EQ(probes.slots.size(), 29U);
  EXPECT_EQ(probes.inputs.rows(), 55);
  EXPECT_EQ(probes.inputs.cols(), 5);
  for (const probe_slot &slot : probes.slots) {
    EXPECT_FALSE(slot.missing_channel);
  }
}

TEST(identification_probes, ReadProbes_InputLevel_ValueIsInputValue) {
  const std::vector<mf::indicator_definition> table{definition(mf::indicator_form::INPUT_LEVEL, "A", "", false)};
  const identification_probes probes = plan(table);
  const Eigen::MatrixXd outputs(0, 3);
  const std::vector<mf::indicator_reading> readings = readings_of(table, probes, outputs);
  ASSERT_EQ(readings.size(), 1U);
  EXPECT_EQ(bits(readings[0].value), bits(0.5));
  EXPECT_EQ(readings[0].status, mf::reading_status::VOTE);
  EXPECT_EQ(readings[0].vote, 1);
  EXPECT_EQ(readings[0].definition.name, "probe");
  EXPECT_EQ(bits(readings[0].load_n), bits(4000.0));
}

TEST(identification_probes, ReadProbes_OutputLevel_ValueIsOutput) {
  const std::vector<mf::indicator_definition> table{definition(mf::indicator_form::OUTPUT_LEVEL, "", "Y", false)};
  const identification_probes probes = plan(table);
  Eigen::MatrixXd outputs = Eigen::MatrixXd::Zero(1, 3);
  outputs(0, 0) = -3.0;
  const std::vector<mf::indicator_reading> readings = readings_of(table, probes, outputs);
  ASSERT_EQ(readings.size(), 1U);
  EXPECT_EQ(bits(readings[0].value), bits(-3.0));
  EXPECT_EQ(readings[0].status, mf::reading_status::VOTE);
  EXPECT_EQ(readings[0].vote, -1);
}

TEST(identification_probes, ReadProbes_CentralDifference_ValueIsSecant) {
  const mf::indicator_definition row = definition(mf::indicator_form::CENTRAL_DIFFERENCE, "B", "Y", false);
  const std::vector<mf::indicator_definition> table{row};
  const identification_probes probes = plan(table);
  Eigen::MatrixXd outputs = Eigen::MatrixXd::Zero(2, 3);
  outputs(0, 0) = 1.0;
  outputs(1, 0) = 0.5;
  const std::vector<mf::indicator_reading> readings = readings_of(table, probes, outputs);
  ASSERT_EQ(readings.size(), 1U);
  // (1.0 - 0.5) / (2 * 0.25)
  EXPECT_EQ(bits(readings[0].value), bits(1.0));
  EXPECT_EQ(readings[0].status, mf::reading_status::VOTE);
  EXPECT_EQ(readings[0].vote, 1);
}

TEST(identification_probes, ReadProbes_ClampOnPlusRow_Clamped) {
  const mf::indicator_definition row = definition(mf::indicator_form::CENTRAL_DIFFERENCE, "B", "Y", false);
  const std::vector<mf::indicator_definition> table{row};
  const identification_probes probes = plan(table);
  Eigen::MatrixXd outputs = Eigen::MatrixXd::Zero(2, 3);
  outputs(0, 0) = 1.0;
  outputs(0, 1) = 1.0;
  const std::vector<mf::indicator_reading> readings = readings_of(table, probes, outputs);
  ASSERT_EQ(readings.size(), 1U);
  EXPECT_EQ(readings[0].status, mf::reading_status::CLAMPED);
  EXPECT_EQ(readings[0].vote, 0);
}

TEST(identification_probes, ReadProbes_ClampOnMinusRow_Clamped) {
  const mf::indicator_definition row = definition(mf::indicator_form::CENTRAL_DIFFERENCE, "B", "Y", false);
  const std::vector<mf::indicator_definition> table{row};
  const identification_probes probes = plan(table);
  Eigen::MatrixXd outputs = Eigen::MatrixXd::Zero(2, 3);
  outputs(0, 0) = 1.0;
  outputs(1, 1) = 1.0;
  const std::vector<mf::indicator_reading> readings = readings_of(table, probes, outputs);
  ASSERT_EQ(readings.size(), 1U);
  EXPECT_EQ(readings[0].status, mf::reading_status::CLAMPED);
}

TEST(identification_probes, ReadProbes_BelowVxlowSet_NotClamped) {
  const mf::indicator_definition row = definition(mf::indicator_form::CENTRAL_DIFFERENCE, "B", "Y", false);
  const std::vector<mf::indicator_definition> table{row};
  const identification_probes probes = plan(table);
  Eigen::MatrixXd outputs = Eigen::MatrixXd::Zero(2, 3);
  outputs(0, 0) = 1.0;
  outputs(0, 2) = 1.0;
  outputs(1, 2) = 1.0;
  const std::vector<mf::indicator_reading> readings = readings_of(table, probes, outputs);
  ASSERT_EQ(readings.size(), 1U);
  EXPECT_EQ(readings[0].status, mf::reading_status::VOTE);
}

TEST(identification_probes, ReadProbes_NanOutput_NotFinite) {
  const std::vector<mf::indicator_definition> table{definition(mf::indicator_form::OUTPUT_LEVEL, "", "Y", false)};
  const identification_probes probes = plan(table);
  Eigen::MatrixXd outputs = Eigen::MatrixXd::Zero(1, 3);
  outputs(0, 0) = QNAN;
  const std::vector<mf::indicator_reading> readings = readings_of(table, probes, outputs);
  ASSERT_EQ(readings.size(), 1U);
  EXPECT_EQ(readings[0].status, mf::reading_status::NOT_FINITE);
}

TEST(identification_probes, ReadProbes_ZeroProbeLoad_NotFinite) {
  const std::vector<mf::indicator_definition> table{definition(mf::indicator_form::OUTPUT_LEVEL, "", "Y", false)};
  Eigen::VectorXd reference = reference_point();
  reference(0) = 0.0;
  const identification_probes probes = mf::detail::plan_probes(LAYOUT, reference, table);
  Eigen::MatrixXd outputs = Eigen::MatrixXd::Zero(1, 3);
  outputs(0, 0) = 1.0;
  const std::vector<mf::indicator_reading> readings = mf::detail::read_probes(LAYOUT, table, probes, outputs);
  ASSERT_EQ(readings.size(), 1U);
  EXPECT_EQ(bits(readings[0].load_n), bits(0.0));
  EXPECT_EQ(readings[0].status, mf::reading_status::NOT_FINITE);
}

TEST(identification_probes, ReadProbes_MissingSlot_MissingChannelStatus) {
  const std::vector<mf::indicator_definition> table{definition(mf::indicator_form::CENTRAL_DIFFERENCE, "Z", "Y", true)};
  const identification_probes probes = plan(table);
  const Eigen::MatrixXd outputs(0, 3);
  const std::vector<mf::indicator_reading> readings = readings_of(table, probes, outputs);
  ASSERT_EQ(readings.size(), 3U);
  for (const mf::indicator_reading &reading : readings) {
    EXPECT_EQ(reading.status, mf::reading_status::MISSING_CHANNEL);
    EXPECT_EQ(reading.vote, 0);
  }
  EXPECT_EQ(bits(readings[0].load_n), bits(2000.0));
}
