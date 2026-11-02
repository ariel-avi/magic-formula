#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <array>
#include <expected>
#include <format>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/cli/cli.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/input_table.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/io/text_file.hpp>
#include <magic_formula/io/tir_conversion.hpp>
#include <magic_formula/io/tir_document.hpp>
#include <magic_formula/mf52_model.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf61_model.hpp>
#include <magic_formula/mf6x_layout.hpp>
#include <magic_formula/model_checks.hpp>
#include <magic_formula/pac2002_model.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {
  std::string fixture() {
    return *mf::io::read_text_file(mf::test::data_file("mf61_solver.tir"));
  }

  std::string without(std::string_view text, std::string_view key) {
    std::istringstream stream{std::string(text)};
    std::ostringstream result;
    std::string line;
    while (std::getline(stream, line)) {
      const auto first = line.find_first_not_of(" \t");
      const auto end = line.find_first_of(" \t=", first);
      if (first != std::string::npos && std::string_view(line).substr(first, end - first) == key) {
        continue;
      }
      result << line << '\n';
    }
    return result.str();
  }

  std::string with(std::string_view text, std::string_view key, std::string_view value) {
    return without(text, key) + std::format("\n[EXTRA]\n{} = {}\n", key, value);
  }

  std::expected<mf::io::loaded_model, mf::error> load(std::string_view text, bool turn_slip = false) {
    const auto document = mf::io::tir_document::parse(text, "mf61-variant.tir");
    if (!document.has_value()) {
      return std::unexpected(document.error());
    }
    return mf::io::load_tire_model(*document, {.turn_slip = turn_slip});
  }

  bool warns(const mf::io::loaded_model &loaded, std::string_view word) {
    for (const auto &item : loaded.warnings) {
      if (item.message.contains(word)) {
        return true;
      }
    }
    return false;
  }

  void expect_error(std::string_view text, mf::error_code code, std::string_view key) {
    const auto loaded = load(text);
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(loaded.error().code, code) << mf::to_string(loaded.error());
    EXPECT_EQ(loaded.error().where.key, key) << mf::to_string(loaded.error());
    EXPECT_EQ(loaded.error().where.file, "mf61-variant.tir");
  }
} // namespace

TEST(mf61_integration, Load_Fixture_ExposesPressureTurnSlipAndRadii) {
  const auto loaded = load(fixture());
  ASSERT_TRUE(loaded.has_value()) << mf::to_string(loaded.error());
  EXPECT_EQ(loaded->model->name(), "MF 6.1");
  EXPECT_EQ(loaded->model->metadata().fittyp, 61);
  EXPECT_FALSE(loaded->model->options().turn_slip);
  EXPECT_EQ(loaded->model->input_length(), 7);
  EXPECT_EQ(loaded->model->output_length(), 13);
  EXPECT_EQ(loaded->model->characteristics_length(), 8);
  const auto enabled = load(fixture(), true);
  ASSERT_TRUE(enabled.has_value()) << mf::to_string(enabled.error());
  EXPECT_TRUE(enabled->model->options().turn_slip);
}

TEST(mf61_integration, Load_CoefficientErrors_AreLocated) {
  const std::string text = fixture();
  expect_error(without(text, "FNOMIN"), mf::error_code::MISSING_KEY, "FNOMIN");
  expect_error(with(text, "FNOMIN", "0"), mf::error_code::INVALID_VALUE, "FNOMIN");
  expect_error(with(text, "PCX1", "'bad'"), mf::error_code::INVALID_VALUE, "PCX1");
  const auto defaulted = load(without(without(text, "QDZ11"), "LMUV"));
  ASSERT_TRUE(defaulted.has_value()) << mf::to_string(defaulted.error());
  EXPECT_TRUE(warns(*defaulted, "model defaults"));
}

TEST(mf61_integration, Load_AbsentVelocitySensitivity_EqualsExplicitZeroAndWarns) {
  const auto absent = load(without(fixture(), "LMUV"));
  const auto explicit_zero = load(with(fixture(), "LMUV", "0"));
  ASSERT_TRUE(absent.has_value()) << mf::to_string(absent.error());
  ASSERT_TRUE(explicit_zero.has_value()) << mf::to_string(explicit_zero.error());
  EXPECT_TRUE(warns(*absent, "LMUV"));
  EXPECT_FALSE(warns(*explicit_zero, "LMUV"));
  Eigen::VectorXd point = absent->model->reference_input();
  point(mf::mf6x::SA) = 0.05;
  point(mf::mf6x::SR) = 0.1;
  const Eigen::VectorXd defaulted = absent->model->evaluate(point);
  const Eigen::VectorXd supplied = explicit_zero->model->evaluate(point);
  EXPECT_TRUE(defaulted.isApprox(supplied, 0.0));
}

TEST(mf61_integration, Load_NominalLoadAlias_AcceptsEqualAndRejectsConflictingValues) {
  const std::string text = with(fixture(), "LFZ0", "1");
  const auto equal = load(with(text, "LFZO", "1"));
  ASSERT_TRUE(equal.has_value()) << mf::to_string(equal.error());
  const auto alias = load(with(without(text, "LFZ0"), "LFZO", "1"));
  ASSERT_TRUE(alias.has_value()) << mf::to_string(alias.error());
  expect_error(with(text, "LFZO", "2"), mf::error_code::INVALID_VALUE, "LFZO");
  expect_error(with(text, "LFZO", "'bad'"), mf::error_code::INVALID_VALUE, "LFZO");
  const auto neither = load(without(without(text, "LFZ0"), "LFZO"));
  ASSERT_TRUE(neither.has_value()) << mf::to_string(neither.error());
}

TEST(mf61_integration, Load_PressureRanges_AreValidatedAndPartialRangesWarn) {
  const std::string text = with(with(fixture(), "PRESMIN", "100000"), "PRESMAX", "300000");
  for (const auto key : {"PRESMIN", "PRESMAX"}) {
    expect_error(with(text, key, "'bad'"), mf::error_code::INVALID_VALUE, key);
    const auto partial = load(without(text, key));
    ASSERT_TRUE(partial.has_value()) << mf::to_string(partial.error());
    EXPECT_TRUE(warns(*partial, "range is not applied"));
    EXPECT_EQ(partial->model->limits()(mf::mf6x::PRESMIN), -std::numeric_limits<double>::infinity());
    EXPECT_EQ(partial->model->limits()(mf::mf6x::PRESMAX), std::numeric_limits<double>::infinity());
  }
  const auto absent = load(without(without(without(text, "PRESMIN"), "PRESMAX"), "VXLOW"));
  ASSERT_TRUE(absent.has_value()) << mf::to_string(absent.error());
  expect_error(with(text, "VXLOW", "'bad'"), mf::error_code::INVALID_VALUE, "VXLOW");
  expect_error(with(with(text, "PRESMIN", "300000"), "PRESMAX", "100000"), mf::error_code::INVALID_VALUE, "PRESMIN");
}

TEST(mf61_integration, Load_Metadata_DiagnosticsAndErrors) {
  const std::string text = fixture();
  expect_error(with(text, "USE_MODE", "'bad'"), mf::error_code::INVALID_VALUE, "USE_MODE");
  expect_error(with(text, "TYRESIDE", "'SYMMETRIC'"), mf::error_code::NOT_IMPLEMENTED, "TYRESIDE");
  const auto right = load(with(with(with(text, "TYRESIDE", "'RIGHT'"), "USE_MODE", "14"), "FE_METHOD", "1"));
  ASSERT_TRUE(right.has_value()) << mf::to_string(right.error());
  EXPECT_EQ(right->model->metadata().side, mf::tyre_side::RIGHT);
  EXPECT_TRUE(warns(*right, "RIGHT"));
  EXPECT_TRUE(warns(*right, "metadata only"));
  EXPECT_TRUE(warns(*right, "FE_METHOD"));
  const auto left = load(with(text, "TYRESIDE", "'LEFT'"));
  ASSERT_TRUE(left.has_value()) << mf::to_string(left.error());
  EXPECT_EQ(left->model->metadata().side, mf::tyre_side::LEFT);
  EXPECT_FALSE(warns(*left, "RIGHT"));
  const auto absent = load(without(without(without(text, "USE_MODE"), "TYRESIDE"), "FE_METHOD"));
  ASSERT_TRUE(absent.has_value()) << mf::to_string(absent.error());
  EXPECT_FALSE(absent->model->metadata().use_mode.has_value());
}

TEST(mf61_integration, InputTable_PressureRequiredAndTurnSlipOptional) {
  const auto loaded = load(fixture());
  ASSERT_TRUE(loaded.has_value());
  const auto table = mf::io::parse_input_table("FZ SA SR IA VX P\nN rad - rad m/s Pa\n5000 0.02 0.1 0 27 180000\n");
  ASSERT_TRUE(table.has_value());
  const auto mapped = mf::io::map_inputs(*table, loaded->model->layout().inputs);
  ASSERT_TRUE(mapped.has_value());
  EXPECT_DOUBLE_EQ(mapped->values(0, mf::mf6x::TS), 0.0);
  const auto no_pressure = mf::io::parse_input_table("FZ SA SR IA VX\nN rad - rad m/s\n5000 0 0 0 27\n");
  ASSERT_TRUE(no_pressure.has_value());
  const auto missing = mf::io::map_inputs(*no_pressure, loaded->model->layout().inputs);
  ASSERT_FALSE(missing.has_value());
  EXPECT_EQ(missing.error().where.key, "P");
  const auto with_ts =
      mf::io::parse_input_table("FZ SA SR IA VX P TS\nN rad - rad m/s Pa 1/m\n5000 0.02 0.1 0 27 180000 0.05\n");
  ASSERT_TRUE(with_ts.has_value()) << mf::to_string(with_ts.error());
  const auto mapped_ts = mf::io::map_inputs(*with_ts, loaded->model->layout().inputs);
  ASSERT_TRUE(mapped_ts.has_value());
  EXPECT_DOUBLE_EQ(mapped_ts->values(0, mf::mf6x::TS), 0.05);
  const auto result = mf::checked_evaluate_batch(*loaded->model, mapped_ts->values);
  ASSERT_TRUE(result.has_value()) << mf::to_string(result.error().cause);
  EXPECT_GT((*result)(0, mf::mf6x::RE), 0.0);
  EXPECT_GT((*result)(0, mf::mf6x::RL), 0.0);
  const auto warnings = mf::collect_input_warnings(*loaded->model, mapped_ts->values);
  ASSERT_EQ(warnings.size(), 1U);
  EXPECT_EQ(warnings[0].cause.where.key, "TS");
  EXPECT_TRUE(mf::collect_input_warnings(*loaded->model, mapped->values).empty());
  const auto enabled = load(fixture(), true);
  ASSERT_TRUE(enabled.has_value());
  EXPECT_TRUE(mf::collect_input_warnings(*enabled->model, mapped_ts->values).empty());
  Eigen::MatrixXd empty(0, 7);
  EXPECT_TRUE(mf::collect_input_warnings(*loaded->model, empty).empty());
}

TEST(mf61_integration, OtherModels_RejectTurnSlip) {
  const mf::model_options options{.turn_slip = true};
  mf::mf5x_parameters parameters;
  const auto mf52 = mf::mf52_model::create(parameters, options);
  ASSERT_FALSE(mf52.has_value());
  EXPECT_EQ(mf52.error().code, mf::error_code::NOT_IMPLEMENTED);
  const auto pac = mf::pac2002_model::create(parameters, mf::combined_method::COSINE, options);
  ASSERT_FALSE(pac.has_value());
  EXPECT_EQ(pac.error().code, mf::error_code::NOT_IMPLEMENTED);
  const auto old = mf::io::load_tire_model_file(mf::test::data_file("pac2002_S.tir"));
  ASSERT_TRUE(old.has_value());
  Eigen::MatrixXd inputs(1, 5);
  inputs << 4000, 0, 0, 0, 10;
  EXPECT_TRUE(mf::collect_input_warnings(*old->model, inputs).empty());
}

TEST(mf61_integration, ConvertTir_UsesMf61CoefficientRules) {
  const auto converted = mf::io::convert_tir(fixture(), mf::coordinate_system::SAE);
  ASSERT_TRUE(converted.has_value()) << mf::to_string(converted.error());
  const auto model = load(converted->text);
  ASSERT_TRUE(model.has_value()) << mf::to_string(model.error());
  EXPECT_EQ(model->model->name(), "MF 6.1");
  EXPECT_EQ(model->model->loaded_side(), mf::coordinate_system::SAE);
}

TEST(mf61_integration, Cli_TurnSlipFlagAndPressureRadiiColumns) {
  for (const auto enabled : {false, true}) {
    const auto table = mf::test::data_file("inputs_mf61.csv").string();
    const auto model = mf::test::data_file("mf61_solver.tir").string();
    std::vector<std::string_view> args{"mf", table, model};
    if (enabled) {
      args.push_back("--turn-slip");
    }
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(mf::cli::run(args, out, err), mf::cli::EXIT_OK) << err.str();
    EXPECT_TRUE(out.str().contains("P[Pa],TS[1/m]"));
    EXPECT_TRUE(out.str().contains("P_CLAMPED[-],RE[m],RL[m]"));
    EXPECT_EQ(err.str().contains("turn slip is disabled"), !enabled);
  }
}

TEST(mf61_integration, TurnSlipDiagnostics_ReportFirstNonzeroRowAndLeaveAllZeroRowsQuiet) {
  const auto loaded = load(fixture());
  ASSERT_TRUE(loaded.has_value()) << mf::to_string(loaded.error());
  Eigen::MatrixXd inputs(4, mf::mf6x::INPUT_LENGTH);
  for (Eigen::Index row = 0; row < inputs.rows(); ++row)
    inputs.row(row) = loaded->model->reference_input().transpose();
  EXPECT_TRUE(mf::collect_input_warnings(*loaded->model, inputs).empty());
  inputs(2, mf::mf6x::TS) = 0.02;
  inputs(3, mf::mf6x::TS) = -0.03;
  const auto warnings = mf::collect_input_warnings(*loaded->model, inputs);
  ASSERT_EQ(warnings.size(), 1U);
  EXPECT_EQ(warnings[0].index, 2U);
  EXPECT_EQ(warnings[0].cause.where.key, "TS");
}

TEST(mf61_integration, ConvertTir_IdempotentAndSparseZeroCoefficients_PreservesSupportedFile) {
  const std::string text = without(with(with(fixture(), "QSX14", "0"), "SSZ4", "0"), "QSX14");
  const auto converted = mf::io::convert_tir(text, mf::coordinate_system::SAE);
  ASSERT_TRUE(converted.has_value()) << mf::to_string(converted.error());
  const auto document = mf::io::tir_document::parse(converted->text);
  ASSERT_TRUE(document.has_value()) << mf::to_string(document.error());
  EXPECT_EQ(document->find("QSX14"), nullptr);
  ASSERT_NE(document->find("SSZ4"), nullptr);
  EXPECT_EQ(document->find("SSZ4")->value, "0");
  const auto repeated = mf::io::convert_tir(converted->text, mf::coordinate_system::SAE);
  ASSERT_TRUE(repeated.has_value()) << mf::to_string(repeated.error());
  EXPECT_FALSE(repeated->applied.vertical);
  EXPECT_FALSE(repeated->applied.lateral);
  EXPECT_TRUE(repeated->changes.empty());
  EXPECT_EQ(repeated->text, converted->text);
  const auto loaded = load(repeated->text);
  ASSERT_TRUE(loaded.has_value()) << mf::to_string(loaded.error());
  EXPECT_EQ(loaded->model->name(), "MF 6.1");
}
