#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <cmath>
#include <cstddef>
#include <expected>
#include <gtest/gtest.h>
#include <magic_formula/error.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/text_file.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/io/tir_editor.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <magic_formula/model_checks.hpp>
#include <string>
#include <string_view>

// Native mirror of the browser pipeline: read the text, replace one value, parse, load, evaluate a checked batch.
namespace {
  using mf::test::bits;
  using mf::test::data_file;

  constexpr Eigen::Index SWEEP_POINTS = 11;
  constexpr double SWEEP_SA_LIMIT_RAD = 0.2;
  constexpr double SWEEP_VX_MPS = 16.6;
  constexpr double SWEEP_FZ_N = 4000.0;

  std::string read_fixture(std::string_view name) {
    const auto text = mf::io::read_text_file(data_file(name));
    EXPECT_TRUE(text.has_value());
    return text.has_value() ? *text : std::string{};
  }

  /// Parses and loads `text`; a failure is a test failure.
  std::expected<mf::io::loaded_model, mf::error> load_text(std::string_view text) {
    const auto document = mf::io::tir_document::parse(text);
    if (!document.has_value()) {
      return std::unexpected<mf::error>(document.error());
    }
    return mf::io::load_tire_model(*document);
  }

  /// Slip-angle sweep at FZ = `SWEEP_FZ_N`, SR = IA = 0, VX = `SWEEP_VX_MPS`.
  Eigen::MatrixXd sweep_inputs() {
    Eigen::MatrixXd inputs(SWEEP_POINTS, mf::mf5x::INPUT_LENGTH);
    for (Eigen::Index row = 0; row < SWEEP_POINTS; ++row) {
      const double fraction = static_cast<double>(row) / static_cast<double>(SWEEP_POINTS - 1);
      inputs(row, mf::mf5x::FZ) = SWEEP_FZ_N;
      inputs(row, mf::mf5x::SA) = (2.0 * fraction - 1.0) * SWEEP_SA_LIMIT_RAD;
      inputs(row, mf::mf5x::SR) = 0.0;
      inputs(row, mf::mf5x::IA) = 0.0;
      inputs(row, mf::mf5x::VX) = SWEEP_VX_MPS;
    }
    return inputs;
  }

  /// Outputs of the sweep for `text`; a failure is a test failure and yields an empty matrix.
  Eigen::MatrixXd sweep_outputs(std::string_view text) {
    const auto loaded = load_text(text);
    if (!loaded.has_value()) {
      ADD_FAILURE() << mf::to_string(loaded.error());
      return {};
    }
    const auto outputs = mf::checked_evaluate_batch(*loaded->model, sweep_inputs());
    if (!outputs.has_value()) {
      ADD_FAILURE() << mf::to_string(outputs.error().cause);
      return {};
    }
    return *outputs;
  }
} // namespace

TEST(tir_edit, ReplaceValue_ScalingFactor_ReloadedModelHasNewCoefficient) {
  const std::string text = read_fixture("pac2002_S.tir");
  const auto edited = mf::io::replace_value(text, "SCALING_COEFFICIENTS", "LMUY", "0.5");
  ASSERT_TRUE(edited.has_value()) << mf::to_string(edited.error());
  const auto loaded = load_text(*edited);
  ASSERT_TRUE(loaded.has_value()) << mf::to_string(loaded.error());
  const auto *const model = dynamic_cast<const mf::mf5x_model *>(loaded->model.get());
  ASSERT_NE(model, nullptr);
  EXPECT_EQ(bits(model->coefficients().LMUY), bits(0.5)); // parsed literal 0.5
}

TEST(tir_edit, ReplaceValue_RestoredFile_OutputsBitIdentical) {
  const std::string text = read_fixture("pac2002_S.tir");
  const auto edited = mf::io::replace_value(text, "SCALING_COEFFICIENTS", "LMUY", "0.5");
  ASSERT_TRUE(edited.has_value()) << mf::to_string(edited.error());
  const auto restored = mf::io::replace_value(*edited, "SCALING_COEFFICIENTS", "LMUY", "1");
  ASSERT_TRUE(restored.has_value()) << mf::to_string(restored.error());
  EXPECT_EQ(*restored, text);

  const Eigen::MatrixXd original_outputs = sweep_outputs(text);
  const Eigen::MatrixXd restored_outputs = sweep_outputs(*restored);
  ASSERT_EQ(original_outputs.rows(), SWEEP_POINTS);
  ASSERT_EQ(restored_outputs.rows(), SWEEP_POINTS);
  ASSERT_EQ(original_outputs.cols(), restored_outputs.cols());
  for (Eigen::Index row = 0; row < original_outputs.rows(); ++row) {
    for (Eigen::Index col = 0; col < original_outputs.cols(); ++col) {
      EXPECT_EQ(bits(original_outputs(row, col)), bits(restored_outputs(row, col))) << row << "," << col;
    }
  }
}

TEST(tir_edit, ReplaceValue_EditedFile_SweepFiniteAndChanged) {
  const std::string text = read_fixture("pac2002_S.tir");
  const auto edited = mf::io::replace_value(text, "SCALING_COEFFICIENTS", "LMUY", "0.5");
  ASSERT_TRUE(edited.has_value()) << mf::to_string(edited.error());

  const Eigen::MatrixXd original_outputs = sweep_outputs(text);
  const Eigen::MatrixXd edited_outputs = sweep_outputs(*edited);
  ASSERT_EQ(original_outputs.rows(), SWEEP_POINTS);
  ASSERT_EQ(edited_outputs.rows(), SWEEP_POINTS);
  EXPECT_TRUE(edited_outputs.allFinite());
  bool any_fy_differs = false;
  for (Eigen::Index row = 0; row < SWEEP_POINTS; ++row) {
    any_fy_differs = any_fy_differs || edited_outputs(row, mf::mf5x::FY) != original_outputs(row, mf::mf5x::FY);
  }
  EXPECT_TRUE(any_fy_differs);
}

TEST(tir_edit, ReplaceValue_FittypToUnsupported_LoadReturnsUnsupportedModel) {
  const std::string text = read_fixture("mf52_S52.tir");
  const auto edited = mf::io::replace_value(text, "MODEL", "FITTYP", "61");
  ASSERT_TRUE(edited.has_value()) << mf::to_string(edited.error());
  const auto loaded = load_text(*edited);
  ASSERT_FALSE(loaded.has_value());
  EXPECT_EQ(loaded.error().code, mf::error_code::UNSUPPORTED_MODEL);
}
