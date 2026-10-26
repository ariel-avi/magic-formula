#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/detail/lanes.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf52_model.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_forms.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/pac2002_model.hpp>
#include <magic_formula/sign_transform.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

namespace {
  using mf::slip_mode;
  using mf::test::bits;
  using out_idx = mf::mf5x::output_index;

  // Forces and moments scales of near_scaled: FNOMIN [N] and FNOMIN*UNLOADED_RADIUS [N*m] of fixture S.
  constexpr double FORCE_SCALE_N = 4000.0;
  constexpr double MOMENT_SCALE_NM = 1200.0;
  constexpr int POINT_ROWS = 67;

  enum class model_kind : std::uint8_t { PAC2002_COSINE, PAC2002_ELLIPSE, PAC2002_SHIFTS, MF52 };

  struct contract_case {
    std::string_view name;
    model_kind kind;
    slip_mode mode;
  };

  constexpr std::array<contract_case, 8> CASES{{
      {.name = "Pac2002Cosine_Combined", .kind = model_kind::PAC2002_COSINE, .mode = slip_mode::COMBINED},
      {.name = "Pac2002Cosine_Uncombined", .kind = model_kind::PAC2002_COSINE, .mode = slip_mode::UNCOMBINED},
      {.name = "Pac2002Ellipse_Combined", .kind = model_kind::PAC2002_ELLIPSE, .mode = slip_mode::COMBINED},
      {.name = "Pac2002Ellipse_Uncombined", .kind = model_kind::PAC2002_ELLIPSE, .mode = slip_mode::UNCOMBINED},
      {.name = "Pac2002Shifts_Combined", .kind = model_kind::PAC2002_SHIFTS, .mode = slip_mode::COMBINED},
      {.name = "Pac2002Shifts_Uncombined", .kind = model_kind::PAC2002_SHIFTS, .mode = slip_mode::UNCOMBINED},
      {.name = "Mf52_Combined", .kind = model_kind::MF52, .mode = slip_mode::COMBINED},
      {.name = "Mf52_Uncombined", .kind = model_kind::MF52, .mode = slip_mode::UNCOMBINED},
  }};

  /// The plans of the transformed-model tests: VERTICAL, LATERAL, both.
  constexpr std::array<mf::sign_transforms, 3> PLANS{{
      {.vertical = true, .lateral = false},
      {.vertical = false, .lateral = true},
      {.vertical = true, .lateral = true},
  }};

  // True for the forces and moments; false for the 0.0/1.0 flag outputs.
  bool is_physical_output(Eigen::Index j) {
    return j <= out_idx::MY;
  }

  // The scale of a physical output: forces in N, moments in N*m.
  double scale_of(Eigen::Index j) {
    return j <= out_idx::FY ? FORCE_SCALE_N : MOMENT_SCALE_NM;
  }
} // namespace

class model_contract : public ::testing::TestWithParam<contract_case> {
protected:
  void SetUp() override {
    const contract_case &param = GetParam();
    mf::mf5x_parameters parameters = mf::test::fixture_s_rng();
    const mf::model_options options{.mode = param.mode};
    if (param.kind == model_kind::MF52) {
      parameters.metadata.fittyp = 6;
      std::expected<mf::mf52_model, mf::error> created = mf::mf52_model::create(parameters, options);
      ASSERT_TRUE(created.has_value()) << mf::to_string(created.error());
      model_ = std::make_unique<mf::mf52_model>(std::move(*created));
      return;
    }
    const bool shifts = param.kind == model_kind::PAC2002_SHIFTS;
    if (shifts) {
      parameters.metadata.fittyp = 5;
      parameters.coefficients.QSY1 = 0.0;
    }
    const mf::combined_method method =
        param.kind == model_kind::PAC2002_ELLIPSE ? mf::combined_method::FRICTION_ELLIPSE : mf::combined_method::COSINE;
    std::expected<mf::pac2002_model, mf::error> created = mf::pac2002_model::create(parameters, method, options);
    ASSERT_TRUE(created.has_value()) << mf::to_string(created.error());
    ASSERT_EQ(created->uses_my_from_shifts(), shifts);
    model_ = std::make_unique<mf::pac2002_model>(std::move(*created));
  }

  std::unique_ptr<const mf::tire_model> model_;
};

TEST_P(model_contract, Lengths_EqualLayoutSizes) {
  EXPECT_EQ(static_cast<std::size_t>(model_->input_length()), model_->layout().inputs.size());
  EXPECT_EQ(static_cast<std::size_t>(model_->output_length()), model_->layout().outputs.size());
  EXPECT_EQ(model_->input_length(), 5);
  EXPECT_EQ(model_->output_length(), 10);
}

// The evaluate forms size their views from lengths stored at construction; every form must agree with the virtual
// lengths. The raw forms write into buffers with one trailing NaN sentinel, which must stay untouched.
TEST_P(model_contract, StoredLengths_AgreeWithVirtualLengths) {
  const auto in_len = static_cast<std::size_t>(model_->input_length());
  const auto out_len = static_cast<std::size_t>(model_->output_length());
  constexpr double sentinel = std::numeric_limits<double>::quiet_NaN();

  Eigen::MatrixXd inputs(3, model_->input_length());
  inputs.col(mf::mf5x::FZ).setConstant(4000.0);
  inputs.col(mf::mf5x::SA).setConstant(0.05);
  inputs.col(mf::mf5x::SR).setConstant(0.02);
  inputs.col(mf::mf5x::IA).setConstant(0.0);
  inputs.col(mf::mf5x::VX).setConstant(16.6);
  const Eigen::VectorXd point = inputs.row(0).transpose();

  EXPECT_EQ(static_cast<std::size_t>(model_->evaluate(point).size()), out_len);
  EXPECT_EQ(static_cast<std::size_t>(model_->evaluate_batch(inputs).cols()), out_len);
  EXPECT_EQ(static_cast<std::size_t>(model_->evaluate_batch(inputs).rows()), 3U);

  std::vector<double> one(out_len + 1, sentinel);
  model_->evaluate(point.data(), model_->input_length(), one.data(), model_->output_length());
  for (std::size_t j = 0; j < out_len; ++j) {
    EXPECT_FALSE(std::isnan(one[j])) << "output " << j;
  }
  EXPECT_TRUE(std::isnan(one[out_len]));

  std::vector<double> many((3 * out_len) + 3, sentinel);
  model_->evaluate_batch(inputs.data(), 3, many.data(), 3 * model_->output_length());
  for (std::size_t k = 0; k < 3 * out_len; ++k) {
    EXPECT_FALSE(std::isnan(many[k])) << "element " << k;
  }
  for (std::size_t k = 3 * out_len; k < many.size(); ++k) {
    EXPECT_TRUE(std::isnan(many[k])) << "element " << k;
  }
  EXPECT_EQ(in_len, static_cast<std::size_t>(inputs.cols()));
}

TEST_P(model_contract, LimitsSize_EqualsLayoutLimits) {
  EXPECT_EQ(static_cast<std::size_t>(model_->limits().size()), model_->layout().limits.size());
  EXPECT_EQ(model_->limits().size(), 9);
}

TEST_P(model_contract, BatchVsPoint_RegimeGrid_Rows0_1_32_67_Agree) {
  constexpr Eigen::Index block_rows = mf::detail::BLOCK_ROWS;
  const std::array<Eigen::Index, 4> row_counts{0, 1, block_rows, (2 * block_rows) + 3};
  for (const Eigen::Index rows : row_counts) {
    SCOPED_TRACE(std::format("rows {}", rows));
    const Eigen::MatrixXd inputs = mf::test::regime_grid(rows);
    const Eigen::MatrixXd batch = model_->evaluate_batch(inputs);
    ASSERT_EQ(batch.rows(), rows);
    ASSERT_EQ(batch.cols(), model_->output_length());
    for (Eigen::Index r = 0; r < rows; ++r) {
      const Eigen::VectorXd input = inputs.row(r).transpose();
      const Eigen::VectorXd expected = model_->evaluate(input);
      for (Eigen::Index j = 0; j < expected.size(); ++j) {
        SCOPED_TRACE(std::format("row {} output {}", r, j));
        // Flags and exact zeros are bit-equal; the other values agree within the batch tolerance.
        if (!is_physical_output(j) || bits(expected(j)) == bits(0.0)) {
          EXPECT_EQ(bits(batch(r, j)), bits(expected(j)));
        } else {
          EXPECT_TRUE(mf::test::near_scaled(batch(r, j), expected(j), mf::test::BATCH_REL_TOL, scale_of(j)));
        }
      }
    }
  }
}

TEST_P(model_contract, RawBatch_BitEqualRefBatch) {
  const Eigen::MatrixXd inputs = mf::test::regime_grid(POINT_ROWS);
  const Eigen::MatrixXd expected = model_->evaluate_batch(inputs);
  const int outputs = model_->output_length();
  std::vector<double> data(static_cast<std::size_t>(POINT_ROWS * outputs), -1.0);
  model_->evaluate_batch(inputs.data(), POINT_ROWS, data.data(), POINT_ROWS * outputs);
  for (Eigen::Index c = 0; c < outputs; ++c) {
    for (Eigen::Index r = 0; r < POINT_ROWS; ++r) {
      SCOPED_TRACE(std::format("row {} output {}", r, c));
      EXPECT_EQ(bits(data.at(static_cast<std::size_t>((c * POINT_ROWS) + r))), bits(expected(r, c)));
    }
  }
}

TEST_P(model_contract, RawPoint_BitEqualRefPoint) {
  for (const mf::test::point &p : mf::test::REGIME_POINTS) {
    SCOPED_TRACE(std::format("fz {} kappa {} alpha {}", p.fz_n, p.kappa, p.alpha_rad));
    const Eigen::VectorXd input = mf::test::input_of(p);
    const Eigen::VectorXd expected = model_->evaluate(input);
    std::array<double, mf::mf5x::OUTPUT_LENGTH> data{};
    model_->evaluate(input.data(), mf::mf5x::INPUT_LENGTH, data.data(), mf::mf5x::OUTPUT_LENGTH);
    for (Eigen::Index j = 0; j < expected.size(); ++j) {
      EXPECT_EQ(bits(data.at(static_cast<std::size_t>(j))), bits(expected(j)));
    }
  }
}

TEST_P(model_contract, Batch_AllOutputsFinite_OnRegimeGrid) {
  const Eigen::MatrixXd batch = model_->evaluate_batch(mf::test::regime_grid(POINT_ROWS));
  EXPECT_TRUE(batch.allFinite());
}

TEST_P(model_contract, ReferenceInput_Size_EqualsInputLength) {
  EXPECT_EQ(model_->reference_input().size(), model_->input_length());
}

TEST_P(model_contract, NegatedBy_Channels_ValidAndLimitSignsWellFormed) {
  for (const mf::input_channel &channel : model_->layout().inputs) {
    mf::sign_transform expected = mf::sign_transform::NONE;
    if (channel.name == "FZ") {
      expected = mf::sign_transform::VERTICAL;
    } else if (channel.name == "SA") {
      expected = mf::sign_transform::LATERAL;
    }
    EXPECT_EQ(channel.negated_by, expected) << channel.name;
  }
  for (const mf::output_channel &channel : model_->layout().outputs) {
    const bool vertical = channel.name == "FY" || channel.name == "MZ" || channel.name == "MY";
    EXPECT_EQ(channel.negated_by, vertical ? mf::sign_transform::VERTICAL : mf::sign_transform::NONE) << channel.name;
  }
  EXPECT_TRUE(mf::limit_pairs_well_formed(model_->layout().limits));
  EXPECT_TRUE(mf::limit_signs_well_formed(model_->layout().limits));
}

TEST_P(model_contract, Transformed_EachPlan_SameDynamicTypeNameOptionsMetadata) {
  const auto *const pac = dynamic_cast<const mf::pac2002_model *>(model_.get());
  for (const mf::sign_transforms &plan : PLANS) {
    SCOPED_TRACE(std::format("vertical {} lateral {}", plan.vertical, plan.lateral));
    const std::unique_ptr<mf::tire_model> copy = model_->transformed(plan);
    ASSERT_TRUE(copy != nullptr);
    const mf::tire_model *const copy_ptr = copy.get();
    const mf::tire_model *const source_ptr = model_.get();
    EXPECT_TRUE(typeid(*copy_ptr) == typeid(*source_ptr));
    EXPECT_EQ(copy->name(), model_->name());
    EXPECT_EQ(copy->options().mode, model_->options().mode);
    EXPECT_EQ(copy->options().throw_if_model_failure, model_->options().throw_if_model_failure);
    EXPECT_EQ(copy->metadata().fittyp, model_->metadata().fittyp);
    EXPECT_EQ(copy->metadata().use_mode, model_->metadata().use_mode);
    EXPECT_EQ(copy->metadata().side, model_->metadata().side);
    EXPECT_EQ(copy->limits().size(), model_->limits().size());
    if (pac != nullptr) {
      const auto *const pac_copy = dynamic_cast<const mf::pac2002_model *>(copy.get());
      ASSERT_TRUE(pac_copy != nullptr);
      EXPECT_EQ(pac_copy->method(), pac->method());
      EXPECT_EQ(pac_copy->uses_my_from_shifts(), pac->uses_my_from_shifts());
    }
  }
}

TEST_P(model_contract, Transformed_Parameters_PassCreate) {
  const auto *const pac = dynamic_cast<const mf::pac2002_model *>(model_.get());
  for (const mf::sign_transforms &plan : PLANS) {
    SCOPED_TRACE(std::format("vertical {} lateral {}", plan.vertical, plan.lateral));
    const std::unique_ptr<mf::tire_model> copy = model_->transformed(plan);
    const auto *const typed = dynamic_cast<const mf::mf5x_model *>(copy.get());
    ASSERT_TRUE(typed != nullptr);
    const mf::mf5x_parameters parameters{
        .coefficients = typed->coefficients(), .limits = typed->limits(), .metadata = typed->metadata()};
    if (pac != nullptr) {
      const auto created = mf::pac2002_model::create(parameters, pac->method(), model_->options());
      EXPECT_TRUE(created.has_value());
    } else {
      const auto created = mf::mf52_model::create(parameters, model_->options());
      EXPECT_TRUE(created.has_value());
    }
  }
}

TEST_P(model_contract, LoadedSide_FlipsUnderVerticalOnly) {
  EXPECT_EQ(model_->loaded_side(), mf::coordinate_system::ISO);
  EXPECT_EQ(model_->transformed(PLANS[0])->loaded_side(), mf::coordinate_system::SAE);
  EXPECT_EQ(model_->transformed(PLANS[1])->loaded_side(), mf::coordinate_system::ISO);
  EXPECT_EQ(model_->transformed(PLANS[2])->loaded_side(), mf::coordinate_system::SAE);
  EXPECT_EQ(model_->transformed(PLANS[0])->transformed(PLANS[0])->loaded_side(), mf::coordinate_system::ISO);
}

TEST_P(model_contract, FindCoordinateSystem_ReferenceInputSizeAnd29Readings) {
  const mf::coordinate_identification identification = model_->find_coordinate_system();
  EXPECT_EQ(identification.readings.size(), 29U);
  for (const mf::indicator_reading &reading : identification.readings) {
    EXPECT_NE(reading.status, mf::reading_status::MISSING_CHANNEL) << reading.definition.name;
    EXPECT_NE(reading.status, mf::reading_status::CLAMPED) << reading.definition.name;
    EXPECT_NE(reading.status, mf::reading_status::NOT_FINITE) << reading.definition.name;
  }
  EXPECT_EQ(identification.classification, mf::system_classification::MIXED);
  EXPECT_EQ(identification.warnings.size(), 1U);
  const Eigen::VectorXd reference = model_->reference_input();
  ASSERT_EQ(identification.reference_input.size(), reference.size());
  EXPECT_EQ(identification.reference_input.size(), model_->input_length());
  for (Eigen::Index i = 0; i < reference.size(); ++i) {
    EXPECT_EQ(bits(identification.reference_input(i)), bits(reference(i))) << "element " << i;
  }
}

INSTANTIATE_TEST_SUITE_P(models, model_contract, ::testing::ValuesIn(CASES),
                         [](const ::testing::TestParamInfo<contract_case> &param_info) {
                           return std::string(param_info.param.name);
                         });
