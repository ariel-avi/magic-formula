#include <Eigen/Core>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <future>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/detail/dual_lane.hpp>
#include <magic_formula/detail/lanes.hpp>
#include <magic_formula/detail/mf61_equations.hpp>
#include <magic_formula/detail/mf61_kernels.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/io/model_loader.hpp>
#include <magic_formula/mf61_coefficients.hpp>
#include <magic_formula/mf61_model.hpp>
#include <magic_formula/mf6x_layout.hpp>
#include <magic_formula/mf6x_signs.hpp>
#include <magic_formula/model_checks.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/sign_transform.hpp>
#include <random>
#include <utility>

namespace {
  mf::mf61_parameters source_parameters() {
    const auto loaded = mf::io::load_tire_model_file(std::filesystem::path(MF_TEST_DATA_DIR) / "mf61_solver.tir");
    EXPECT_TRUE(loaded.has_value());
    mf::mf61_parameters parameters;
    if (loaded.has_value()) {
      parameters.coefficients = static_cast<const mf::mf61_model &>(*loaded->model).coefficients();
      parameters.metadata = loaded->model->metadata();
    }
    return parameters;
  }
  mf::mf61_parameters simple_parameters() {
    mf::mf61_parameters p;
    auto &c = p.coefficients;
    c.FNOMIN = 4000.0;
    c.UNLOADED_RADIUS = 0.3;
    c.LONGVL = 20.0;
    c.NOMPRES = 200000.0;
    c.VERTICAL_STIFFNESS = 200000.0;
    c.PCX1 = 1.5;
    c.PDX1 = 1.0;
    c.PKX1 = 20.0;
    c.PCY1 = 1.3;
    c.PDY1 = 1.1;
    c.PKY1 = -15.0;
    c.PKY2 = 1.5;
    c.PKY4 = 2.0;
    c.Q_FZ1 = 15.0;
    return p;
  }
  Eigen::VectorXd point(double sa = 0.0872665, double sr = 0.1, double ia = 0.0261799, double pressure = 180000.0,
                        double ts = 0.0) {
    Eigen::VectorXd input(mf::mf6x::INPUT_LENGTH);
    input << 5000.0, sa, sr, ia, 27.0, pressure, ts;
    return input;
  }
  void expect_scaled(double actual, double expected, double tolerance = 1e-12) {
    EXPECT_NEAR(actual, expected, tolerance * std::max(1.0, std::abs(expected)));
  }
} // namespace

TEST(mf61_model, Layout_ReferenceAndConstruction_DescribeMf61) {
  const auto parameters = source_parameters();
  const auto model = mf::mf61_model::create(parameters);
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->name(), "MF 6.1");
  EXPECT_EQ(model->input_length(), 7);
  EXPECT_EQ(model->output_length(), 13);
  EXPECT_EQ(model->characteristics_length(), 8);
  EXPECT_EQ(model->layout().inputs[mf::mf6x::P].unit, "Pa");
  EXPECT_EQ(model->layout().inputs[mf::mf6x::TS].unit, "1/m");
  EXPECT_EQ(model->layout().outputs[mf::mf6x::RE].unit, "m");
  EXPECT_EQ(model->layout().inputs[mf::mf6x::TS].negated_by, mf::sign_transform::NONE);
  const Eigen::VectorXd reference = model->reference_input();
  EXPECT_DOUBLE_EQ(reference(mf::mf6x::FZ), parameters.coefficients.FNOMIN * parameters.coefficients.LFZ0);
  EXPECT_DOUBLE_EQ(reference(mf::mf6x::VX), parameters.coefficients.LONGVL);
  EXPECT_DOUBLE_EQ(reference(mf::mf6x::P), parameters.coefficients.NOMPRES);
  EXPECT_DOUBLE_EQ(reference(mf::mf6x::SA), 0.0);
  EXPECT_DOUBLE_EQ(reference(mf::mf6x::TS), 0.0);
}

TEST(mf61_model, Coefficients_AllNonfiniteMembersAndInvalidDenominators_AreRejected) {
  const auto good = simple_parameters();
  for (const auto &entry : mf::mf6x::COEFFICIENTS) {
    auto p = good;
    p.coefficients.*entry.member = std::numeric_limits<double>::quiet_NaN();
    const auto model = mf::mf61_model::create(p);
    ASSERT_FALSE(model.has_value()) << entry.key;
    EXPECT_EQ(model.error().where.key, entry.key);
  }
  constexpr std::array<std::string_view, 9> invalid_keys{
      "FNOMIN", "UNLOADED_RADIUS", "LONGVL", "NOMPRES", "LFZ0", "LMUX", "LMUY", "VERTICAL_STIFFNESS", "PKY2"};
  for (const auto key : invalid_keys) {
    auto p = good;
    for (const auto &entry : mf::mf6x::COEFFICIENTS)
      if (entry.key == key)
        p.coefficients.*entry.member = 0.0;
    const auto model = mf::mf61_model::create(p);
    ASSERT_FALSE(model.has_value()) << key;
    EXPECT_EQ(model.error().where.key, key);
  }
  auto reversed = good;
  reversed.coefficients.VERTICAL_STIFFNESS = -200000.0;
  EXPECT_FALSE(mf::mf61_model::create(reversed).has_value());
  reversed.coefficients.FNOMIN = -4000.0;
  EXPECT_TRUE(mf::mf61_model::create(reversed).has_value());
}

TEST(mf61_model, Limits_InvalidSizeOrderAndPressure_AreRejected) {
  auto p = simple_parameters();
  p.limits.resize(2);
  EXPECT_FALSE(mf::mf61_model::create(p).has_value());
  p = simple_parameters();
  p.limits(mf::mf6x::ALPMIN) = 1.0;
  p.limits(mf::mf6x::ALPMAX) = -1.0;
  EXPECT_FALSE(mf::mf61_model::create(p).has_value());
  for (const int index : std::array<int, 2>{mf::mf6x::PRESMIN, mf::mf6x::PRESMAX}) {
    p = simple_parameters();
    p.limits(index) = 0.0;
    const auto model = mf::mf61_model::create(p);
    ASSERT_FALSE(model.has_value());
    EXPECT_EQ(model.error().where.key, mf::mf6x::LIMITS[static_cast<std::size_t>(index)].key);
  }
  p = simple_parameters();
  p.limits(mf::mf6x::PRESMIN) = 100000.0;
  p.limits(mf::mf6x::PRESMAX) = 300000.0;
  EXPECT_TRUE(mf::mf61_model::create(p).has_value());
}

TEST(mf61_model, PureForces_ZeroCurvatureAndShifts_MatchIndependentClosedForms) {
  auto p = simple_parameters();
  auto &c = p.coefficients;
  c.PPX1 = 0.2;
  c.PPX3 = 0.1;
  c.PPY1 = 0.15;
  c.PPY3 = 0.2;
  const mf::model_options options{.mode = mf::slip_mode::UNCOMBINED};
  const auto model = mf::mf61_model::create(p, options);
  ASSERT_TRUE(model.has_value());
  Eigen::VectorXd input = point(0.08, 0.12, 0.0, 250000.0);
  input(mf::mf6x::FZ) = c.FNOMIN;
  const Eigen::VectorXd output = model->evaluate(input);
  constexpr double pressure_delta = 0.25;
  const double dx = c.PDX1 * (1.0 + c.PPX3 * pressure_delta) * c.FNOMIN;
  const double kx = c.FNOMIN * c.PKX1 * (1.0 + c.PPX1 * pressure_delta);
  const double bx = kx / (c.PCX1 * dx + 1e-6);
  const double expected_fx = dx * std::sin(c.PCX1 * std::atan(bx * 0.12));
  const double dy = c.PDY1 * (1.0 + c.PPY3 * pressure_delta) * c.FNOMIN;
  const double ky = c.PKY1 * c.FNOMIN * (1.0 + c.PPY1 * pressure_delta) * std::sin(c.PKY4 * std::atan(1.0 / c.PKY2));
  const double by = ky / (c.PCY1 * dy + 1e-6);
  const double expected_fy = dy * std::sin(c.PCY1 * std::atan(by * 0.08));
  expect_scaled(output(mf::mf6x::FX), expected_fx);
  expect_scaled(output(mf::mf6x::FY), expected_fy);
}

TEST(mf61_model, SourceSixCases_ForcesMomentsAndRadii_PreserveRoundedRegressionEvidence) {
  struct source_case {
    double sa;
    double sr;
    double ia;
    double ts;
    bool turn_slip;
    std::array<double, 7> expected;
  };
  const std::array<source_case, 6> cases{{
      {0.0872665, 0.1, 0.0261799, 0.05, true, {4354.2, -3445.9, -8.895832935751637, 22.7990, -26.1149, 0.3838, 0.3726}},
      {0.0872665, 0.1, 0.0261799, 0.05, false, {4354.6, -3448.3, -8.7991, 22.8447, -26.1149, 0.3838, 0.3726}},
      {-0.0872665,
       0.1,
       -0.0261799,
       0.05,
       true,
       {4555.2, 3325.9, 2.306454054356660, -18.1037, -26.1149, 0.3838, 0.3727}},
      {-0.0872665, 0.1, -0.0261799, 0.0, false, {4555.2, 3326.6, 3.5747, -18.1157, -26.1149, 0.3838, 0.3727}},
      {0.0872665,
       0.1,
       -0.0261799,
       0.05,
       true,
       {4354.6, -3380.0, -12.748089423182599, 107.6791, -26.1149, 0.3838, 0.3727}},
      {0.0872665, 0.1, -0.0261799, 0.0, false, {4354.6, -3378.9, -11.4789, 107.6600, -26.1149, 0.3838, 0.3727}},
  }};
  constexpr std::array<int, 7> channels{mf::mf6x::FX, mf::mf6x::FY, mf::mf6x::MZ, mf::mf6x::MX,
                                        mf::mf6x::MY, mf::mf6x::RE, mf::mf6x::RL};
  for (const auto &row : cases) {
    const auto model = mf::mf61_model::create(source_parameters(), {.turn_slip = row.turn_slip});
    ASSERT_TRUE(model.has_value());
    const Eigen::VectorXd result = model->evaluate(point(row.sa, row.sr, row.ia, 180000.0, row.ts));
    ASSERT_TRUE(result.allFinite());
    for (std::size_t i = 0; i < channels.size(); ++i) {
      // Enabled-turn-slip Mz references come from the independently compiled source with the documented two
      // corrections.
      const double tolerance = row.turn_slip && channels[i] == mf::mf6x::MZ ? 1e-5 : (i < 2 ? 1e-3 : 1e-2);
      expect_scaled(result(channels[i]), row.expected[i], tolerance);
    }
  }
}

TEST(mf61_model, PureDispatchAndRadius_RespectPhysicalSlipArguments) {
  const auto model = mf::mf61_model::create(source_parameters(), {.mode = mf::slip_mode::UNCOMBINED});
  ASSERT_TRUE(model.has_value());
  const Eigen::VectorXd reference = model->evaluate(point());
  Eigen::VectorXd input = point(0.19);
  const Eigen::VectorXd changed_sa = model->evaluate(input);
  expect_scaled(changed_sa(mf::mf6x::FX), reference(mf::mf6x::FX));
  expect_scaled(changed_sa(mf::mf6x::RE), reference(mf::mf6x::RE));
  input = point(0.0872665, 0.23);
  const Eigen::VectorXd changed_sr = model->evaluate(input);
  expect_scaled(changed_sr(mf::mf6x::MZ), reference(mf::mf6x::MZ));
  EXPECT_GT(std::abs(changed_sr(mf::mf6x::FX) - reference(mf::mf6x::FX)), 1.0);
  const mf::detail::mf61_equations<mf::detail::point_lane> equations(model->coefficients(), 1, false);
  const auto lane = [](double value) { return mf::detail::point_lane::Constant(value); };
  expect_scaled(reference(mf::mf6x::FX),
                equations.fx_pure(lane(0.1), lane(0.0261799), lane(5000), lane(180000), lane(27), lane(0))(0));
  expect_scaled(reference(mf::mf6x::MZ),
                equations.mz_pure(lane(0.0872665), lane(0.0261799), lane(5000), lane(180000), lane(27), lane(0))(0));
  expect_scaled(reference(mf::mf6x::RE), equations.re(lane(0.1), lane(5000), lane(27), lane(180000))(0));
}

TEST(mf61_model, PointAndBatch_AllModesAndBlockBoundaries_Agree) {
  const auto parameters = source_parameters();
  for (const bool turn_slip : std::array<bool, 2>{false, true}) {
    for (const auto mode : std::array<mf::slip_mode, 2>{mf::slip_mode::COMBINED, mf::slip_mode::UNCOMBINED}) {
      const auto model = mf::mf61_model::create(parameters, {.mode = mode, .turn_slip = turn_slip});
      ASSERT_TRUE(model.has_value());
      Eigen::MatrixXd inputs(65, mf::mf6x::INPUT_LENGTH);
      for (Eigen::Index row = 0; row < inputs.rows(); ++row) {
        const double side = row % 2 == 0 ? 1.0 : -1.0;
        inputs.row(row) = point(side * (0.015 + 0.001 * static_cast<double>(row)), side * 0.08, side * 0.015,
                                210000.0 + 100.0 * static_cast<double>(row), turn_slip ? side * 0.02 : 0.0)
                              .transpose();
      }
      const Eigen::MatrixXd batch = model->evaluate_batch(inputs);
      ASSERT_TRUE(batch.allFinite());
      for (Eigen::Index row = 0; row < inputs.rows(); ++row) {
        const Eigen::VectorXd input = inputs.row(row).transpose();
        const Eigen::VectorXd expected = model->evaluate(input);
        for (Eigen::Index col = 0; col < batch.cols(); ++col)
          expect_scaled(batch(row, col), expected(col));
      }
      Eigen::MatrixXd empty(0, mf::mf6x::INPUT_LENGTH);
      EXPECT_EQ(model->evaluate_batch(empty).rows(), 0);
      EXPECT_EQ(model->evaluate_characteristics_batch(empty).rows(), 0);
    }
  }
}

TEST(mf61_model, Conditioning_AllRangesFlagsAndUnloadedRows_FollowContract) {
  auto parameters = source_parameters();
  parameters.limits << 1000.0, 6000.0, -0.1, 0.1, -0.15, 0.15, -0.04, 0.04, 30.0, 200000.0, 300000.0;
  const auto model = mf::mf61_model::create(parameters);
  ASSERT_TRUE(model.has_value());
  Eigen::VectorXd input = point(0.3, 0.2, 0.1, 150000.0);
  input(mf::mf6x::FZ) = 8000.0;
  const Eigen::VectorXd result = model->evaluate(input);
  ASSERT_TRUE(result.allFinite());
  for (const int channel : std::array<int, 6>{mf::mf6x::FZ_CLAMPED, mf::mf6x::SR_CLAMPED, mf::mf6x::SA_CLAMPED,
                                              mf::mf6x::IA_CLAMPED, mf::mf6x::BELOW_VXLOW, mf::mf6x::P_CLAMPED})
    EXPECT_DOUBLE_EQ(result(channel), 1.0);
  input << 6000.0, 0.15, 0.1, 0.04, 27.0, 200000.0, 0.0;
  const Eigen::VectorXd expected = model->evaluate(input);
  for (const int channel : std::array<int, 7>{mf::mf6x::FX, mf::mf6x::FY, mf::mf6x::MZ, mf::mf6x::MX, mf::mf6x::MY,
                                              mf::mf6x::RE, mf::mf6x::RL})
    expect_scaled(result(channel), expected(channel));
  for (const double fz : std::array<double, 3>{0.0, -1.0, -0.0}) {
    input(mf::mf6x::FZ) = fz;
    const Eigen::VectorXd unloaded = model->evaluate(input);
    const Eigen::VectorXd characteristics = model->evaluate_characteristics(input);
    for (const int channel : std::array<int, 7>{mf::mf6x::FX, mf::mf6x::FY, mf::mf6x::MZ, mf::mf6x::MX, mf::mf6x::MY,
                                                mf::mf6x::RE, mf::mf6x::RL})
      EXPECT_DOUBLE_EQ(unloaded(channel), 0.0);
    for (int channel = 0; channel < 6; ++channel)
      EXPECT_DOUBLE_EQ(characteristics(channel), 0.0);
    EXPECT_TRUE(std::isnan(characteristics(mf::mf6x::SA_AT_PEAK_FY)));
    EXPECT_TRUE(std::isnan(characteristics(mf::mf6x::SR_AT_PEAK_FX)));
  }
  auto no_contact = parameters;
  no_contact.limits(mf::mf6x::FZMIN) = -100.0;
  no_contact.limits(mf::mf6x::FZMAX) = 0.0;
  const auto clamped_to_zero = mf::mf61_model::create(no_contact);
  ASSERT_TRUE(clamped_to_zero.has_value());
  input = point();
  const Eigen::VectorXd clamped = clamped_to_zero->evaluate(input);
  EXPECT_DOUBLE_EQ(clamped(mf::mf6x::FZ_CLAMPED), 1.0);
  EXPECT_DOUBLE_EQ(clamped(mf::mf6x::FX), 0.0);
}

TEST(mf61_model, Characteristics_AnalyticSlopesAndBatch_MatchFiniteDifferenceChecks) {
  for (const bool turn_slip : std::array<bool, 2>{false, true}) {
    const auto model = mf::mf61_model::create(source_parameters(), {.turn_slip = turn_slip});
    ASSERT_TRUE(model.has_value());
    Eigen::VectorXd input = point(0.07, 0.08, 0.02, 230000.0, turn_slip ? 0.03 : 0.0);
    const Eigen::VectorXd at = model->evaluate(input);
    const Eigen::VectorXd characteristics = model->evaluate_characteristics(input);
    ASSERT_TRUE(characteristics.head(6).allFinite());
    constexpr double step = 1e-6;
    for (const auto pair : std::array<std::array<int, 3>, 2>{
             {{mf::mf6x::SA, mf::mf6x::FY, mf::mf6x::DFY_DSA}, {mf::mf6x::SR, mf::mf6x::FX, mf::mf6x::DFX_DSR}}}) {
      Eigen::VectorXd lower = input;
      Eigen::VectorXd upper = input;
      lower(pair[0]) -= step;
      upper(pair[0]) += step;
      const Eigen::VectorXd low = model->evaluate(lower);
      const Eigen::VectorXd high = model->evaluate(upper);
      const double numerical = (high(pair[1]) - low(pair[1])) / (2.0 * step);
      expect_scaled(characteristics(pair[2]), numerical, 2e-7);
    }
    expect_scaled(characteristics(mf::mf6x::FY_OVER_SA), at(mf::mf6x::FY) / input(mf::mf6x::SA));
    expect_scaled(characteristics(mf::mf6x::FX_OVER_SR), at(mf::mf6x::FX) / input(mf::mf6x::SR));
    Eigen::MatrixXd inputs(2, mf::mf6x::INPUT_LENGTH);
    inputs.row(0) = input.transpose();
    input(mf::mf6x::SA) = -0.05;
    input(mf::mf6x::SR) = -0.08;
    inputs.row(1) = input.transpose();
    const Eigen::MatrixXd batch = model->evaluate_characteristics_batch(inputs);
    for (Eigen::Index row = 0; row < 2; ++row) {
      const Eigen::VectorXd row_input = inputs.row(row).transpose();
      const Eigen::VectorXd expected = model->evaluate_characteristics(row_input);
      for (Eigen::Index col = 0; col < batch.cols(); ++col) {
        if (std::isnan(expected(col)))
          EXPECT_TRUE(std::isnan(batch(row, col)));
        else
          expect_scaled(batch(row, col), expected(col));
      }
    }
  }
}

TEST(mf61_model, Characteristics_ZeroSlipsSpeedFrictionAndOffsets_UseRightDerivative) {
  auto p = simple_parameters();
  p.coefficients.LMUV = 0.4;
  p.coefficients.PVX1 = 0.01;
  p.coefficients.PHX1 = 0.02;
  p.coefficients.PHY1 = 0.03;
  p.coefficients.PVY1 = 0.01;
  const auto model = mf::mf61_model::create(p, {.mode = mf::slip_mode::UNCOMBINED});
  ASSERT_TRUE(model.has_value());
  Eigen::VectorXd input = point(0.0, 0.0, 0.0, 200000.0);
  const Eigen::VectorXd output = model->evaluate(input);
  const Eigen::VectorXd characteristics = model->evaluate_characteristics(input);
  ASSERT_TRUE(characteristics.head(6).allFinite());
  EXPECT_DOUBLE_EQ(characteristics(mf::mf6x::FY_OVER_SA), characteristics(mf::mf6x::DFY_DSA));
  EXPECT_DOUBLE_EQ(characteristics(mf::mf6x::FX_OVER_SR), characteristics(mf::mf6x::DFX_DSR));
  constexpr double step = 1e-8;
  for (const auto pair : std::array<std::array<int, 3>, 2>{
           {{mf::mf6x::SA, mf::mf6x::FY, mf::mf6x::DFY_DSA}, {mf::mf6x::SR, mf::mf6x::FX, mf::mf6x::DFX_DSR}}}) {
    Eigen::VectorXd higher = input;
    higher(pair[0]) += step;
    const Eigen::VectorXd value = model->evaluate(higher);
    expect_scaled(characteristics(pair[2]), (value(pair[1]) - output(pair[1])) / step, 1e-5);
  }
}

TEST(mf61_model, Characteristics_ClampedSlipsAndAbsentStationaryPoints_AreExplicit) {
  auto p = simple_parameters();
  p.limits(mf::mf6x::ALPMIN) = -0.1;
  p.limits(mf::mf6x::ALPMAX) = 0.1;
  p.limits(mf::mf6x::KPUMIN) = -0.1;
  p.limits(mf::mf6x::KPUMAX) = 0.1;
  const auto model = mf::mf61_model::create(p);
  ASSERT_TRUE(model.has_value());
  const Eigen::VectorXd characteristics = model->evaluate_characteristics(point(0.5, 0.5, 0.0, 200000.0));
  EXPECT_DOUBLE_EQ(characteristics(mf::mf6x::DFY_DSA), 0.0);
  EXPECT_DOUBLE_EQ(characteristics(mf::mf6x::DFX_DSR), 0.0);
  EXPECT_TRUE(std::isnan(characteristics(mf::mf6x::SA_AT_PEAK_FY)));
  EXPECT_TRUE(std::isnan(characteristics(mf::mf6x::SR_AT_PEAK_FX)));
}

TEST(mf61_model, NeutralTurnSlipCoefficients_ZeroAndNonzeroTurnSlip_StayFinite) {
  auto p = simple_parameters();
  const auto model = mf::mf61_model::create(p, {.turn_slip = true});
  ASSERT_TRUE(model.has_value());
  for (const double ts : std::array<double, 3>{0.0, 0.05, -0.05}) {
    const Eigen::VectorXd input = point(0.03, 0.04, 0.0, 200000.0, ts);
    EXPECT_TRUE(model->evaluate(input).allFinite());
    EXPECT_TRUE(model->evaluate_characteristics(input).head(6).allFinite());
  }
  p.coefficients.Q_FZ1 = 0.0;
  p.coefficients.Q_FZ2 = 0.0;
  const auto invalid = mf::mf61_model::create(p);
  ASSERT_FALSE(invalid.has_value());
  EXPECT_EQ(invalid.error().where.key, "Q_FZ1");
  p.coefficients.Q_FZ2 = 1.0;
  EXPECT_TRUE(mf::mf61_model::create(p).has_value());
}

TEST(mf61_model, CoordinateGauges_OptionsAndLimits_PreserveEvaluationAndRoundTrip) {
  std::mt19937 generator(612);
  std::uniform_real_distribution<double> perturbation(0.8, 1.2);
  for (const bool turn_slip : std::array<bool, 2>{false, true}) {
    for (const auto mode : std::array<mf::slip_mode, 2>{mf::slip_mode::COMBINED, mf::slip_mode::UNCOMBINED}) {
      auto p = source_parameters();
      for (const auto &row : mf::mf6x::COEFFICIENT_SIGNS) {
        if (p.coefficients.*row.member == 0.0)
          p.coefficients.*row.member = 0.001;
        EXPECT_NE(p.coefficients.*row.member, 0.0) << row.key;
      }
      for (const auto &entry : mf::mf6x::COEFFICIENTS)
        p.coefficients.*entry.member *= perturbation(generator);
      p.coefficients.LMUV = 0.15;
      p.limits(mf::mf6x::ALPMIN) = -0.5;
      p.limits(mf::mf6x::ALPMAX) = 0.4;
      const auto model = mf::mf61_model::create(p, {.mode = mode, .turn_slip = turn_slip});
      ASSERT_TRUE(model.has_value());
      for (const auto transforms : std::array<mf::sign_transforms, 4>{
               {{}, {.vertical = true}, {.lateral = true}, {.vertical = true, .lateral = true}}}) {
        const auto converted = model->transformed(transforms);
        EXPECT_EQ(converted->options().turn_slip, turn_slip);
        const auto &converted_mf61 = static_cast<const mf::mf61_model &>(*converted);
        EXPECT_TRUE(mf::validate_coefficients(converted_mf61.coefficients()).has_value());
        const auto restored = converted->transformed(transforms);
        const auto &restored_mf61 = static_cast<const mf::mf61_model &>(*restored);
        for (const auto &entry : mf::mf6x::COEFFICIENTS)
          EXPECT_EQ(std::bit_cast<std::uint64_t>(restored_mf61.coefficients().*entry.member),
                    std::bit_cast<std::uint64_t>(p.coefficients.*entry.member))
              << entry.key;
        Eigen::MatrixXd inputs(4, mf::mf6x::INPUT_LENGTH);
        inputs.row(0) = point(0.06, 0.1, 0.02, 240000.0, 0.02).transpose();
        inputs.row(1) = point(-0.06, -0.1, -0.02, 210000.0, -0.03).transpose();
        inputs.row(2) = point(0.0, 0.0, 0.0, 230000.0, 0.0).transpose();
        inputs.row(3) = point(0.8, 0.9, 0.02, 250000.0, 0.04).transpose();
        Eigen::MatrixXd expected = model->evaluate_batch(inputs);
        mf::apply_sign_transforms(model->layout().outputs, transforms, expected);
        mf::apply_sign_transforms(model->layout().inputs, transforms, inputs);
        const Eigen::MatrixXd actual = converted->evaluate_batch(inputs);
        ASSERT_TRUE(actual.allFinite());
        for (Eigen::Index row = 0; row < actual.rows(); ++row)
          for (Eigen::Index col = 0; col < actual.cols(); ++col)
            expect_scaled(actual(row, col), expected(row, col));
      }
    }
  }
}

TEST(mf61_model, RadiusFailuresAndDegenerateDeflection_ReportNumericalInsteadOfPlausibleValues) {
  auto p = simple_parameters();
  p.coefficients.Q_V1 = 1e100;
  const auto model = mf::mf61_model::create(p);
  ASSERT_TRUE(model.has_value());
  const Eigen::VectorXd result = model->evaluate(point(0.02, 0.1, 0.0, 200000.0));
  EXPECT_TRUE(std::isnan(result(mf::mf6x::RE)));
  EXPECT_TRUE(std::isnan(result(mf::mf6x::RL)));
  EXPECT_FALSE(mf::check_output(*model, result).has_value());
  const auto checked = mf::mf61_model::create(p, {.throw_if_model_failure = true});
  ASSERT_TRUE(checked.has_value());
  EXPECT_THROW(static_cast<void>(checked->evaluate(point())), mf::model_failure);
  p = simple_parameters();
  p.coefficients.Q_FCX = 1.0;
  const mf::detail::mf61_equations<mf::detail::point_lane> equations(p.coefficients, 1, false);
  const auto lane = [](double value) { return mf::detail::point_lane::Constant(value); };
  const auto invalid_radius = equations.rl(lane(0.1), lane(0.1), lane(0), lane(5000), lane(200000), lane(27),
                                           lane(std::numeric_limits<double>::quiet_NaN()));
  EXPECT_TRUE(std::isnan(invalid_radius(0)));
  auto zero_external = p.coefficients;
  zero_external.PFZ1 = -1.0;
  const mf::detail::mf61_equations<mf::detail::point_lane> singular(zero_external, 1, false);
  const auto invalid_external =
      singular.rl(lane(0.0), lane(0.0), lane(0), lane(5000), lane(400000), lane(27), lane(0.3));
  EXPECT_TRUE(std::isnan(invalid_external(0)));
  auto negative_discriminant = p.coefficients;
  negative_discriminant.Q_FZ2 = -10000.0;
  const mf::detail::mf61_equations<mf::detail::point_lane> negative(negative_discriminant, 1, false);
  const auto no_root = negative.rl(lane(0.0), lane(0.0), lane(0), lane(5000), lane(200000), lane(27), lane(0.3));
  EXPECT_DOUBLE_EQ(no_root(0), 0.0);
  negative_discriminant.Q_FZ1 = 0.0;
  const mf::detail::mf61_equations<mf::detail::point_lane> zero_linear(negative_discriminant, 1, false);
  const auto zero_linear_no_root =
      zero_linear.rl(lane(0.0), lane(0.0), lane(0), lane(5000), lane(200000), lane(27), lane(0.3));
  EXPECT_DOUBLE_EQ(zero_linear_no_root(0), 0.0);
}

TEST(mf61_model, RawEvaluationAndCharacteristics_DoNotAllocateEigenTemporaries) {
  const auto model = mf::mf61_model::create(source_parameters(), {.turn_slip = true});
  ASSERT_TRUE(model.has_value());
  const Eigen::VectorXd input = point(0.04, 0.05, 0.01, 220000.0, 0.03);
  std::array<double, mf::mf6x::OUTPUT_LENGTH> output{};
  Eigen::MatrixXd inputs(33, mf::mf6x::INPUT_LENGTH);
  for (Eigen::Index row = 0; row < inputs.rows(); ++row)
    inputs.row(row) = input.transpose();
  Eigen::MatrixXd outputs(inputs.rows(), mf::mf6x::OUTPUT_LENGTH);
  mf::detail::mf61_inputs<mf::detail::block_lane> lanes;
  for (std::size_t channel = 0; channel < lanes.size(); ++channel)
    lanes[channel] = inputs.col(static_cast<Eigen::Index>(channel)).head(32).array();
#if defined(EIGEN_RUNTIME_NO_MALLOC)
  Eigen::internal::set_is_malloc_allowed(false);
#endif
  model->evaluate(input.data(), mf::mf6x::INPUT_LENGTH, output.data(), mf::mf6x::OUTPUT_LENGTH);
  model->evaluate_batch(inputs.data(), static_cast<int>(inputs.rows()), outputs.data(),
                        static_cast<int>(outputs.size()));
  const auto characteristics =
      mf::detail::mf61_characteristics(model->coefficients(), model->options(), model->limits(), lanes);
#if defined(EIGEN_RUNTIME_NO_MALLOC)
  Eigen::internal::set_is_malloc_allowed(true);
#endif
  EXPECT_TRUE(outputs.allFinite());
  EXPECT_TRUE(characteristics[mf::mf6x::DFY_DSA].isFinite().all());
}

TEST(mf61_model, SharedImmutableModel_ConcurrentEvaluations_Agree) {
  const auto model = mf::mf61_model::create(source_parameters(), {.turn_slip = true});
  ASSERT_TRUE(model.has_value());
  const Eigen::VectorXd input = point(0.06, 0.05, 0.01, 220000.0, 0.03);
  const Eigen::VectorXd expected = model->evaluate(input);
  std::array<std::future<Eigen::VectorXd>, 4> workers;
  for (auto &worker : workers)
    worker = std::async(std::launch::async, [&model, &input]() { return model->evaluate(input); });
  for (auto &worker : workers) {
    const Eigen::VectorXd result = worker.get();
    for (Eigen::Index channel = 0; channel < result.size(); ++channel)
      expect_scaled(result(channel), expected(channel));
  }
}

TEST(mf61_model, LaneHelpers_DualValuesAndDirectionalLimits_Agree) {
  using lane = mf::detail::point_lane;
  const lane zero = lane::Zero();
  const lane one = lane::Constant(1.0);
  const lane three = lane::Constant(3.0);
  const lane four = lane::Constant(4.0);
  const auto norm = mf::detail::mf61_norm(mf::detail::make_dual(three, one), mf::detail::make_dual(four, zero));
  EXPECT_DOUBLE_EQ(norm.value(0), 5.0);
  EXPECT_DOUBLE_EQ(norm.derivative(0), 0.6);
  const auto origin = mf::detail::mf61_norm(mf::detail::make_dual(zero, three), mf::detail::make_dual(zero, four));
  EXPECT_DOUBLE_EQ(origin.derivative(0), 5.0);
  const auto root = mf::detail::mf61_sqrt(mf::detail::make_dual(four, one));
  EXPECT_DOUBLE_EQ(root.value(0), 2.0);
  EXPECT_DOUBLE_EQ(root.derivative(0), 0.25);
  const auto root_zero = mf::detail::mf61_sqrt(mf::detail::make_dual(zero, zero));
  EXPECT_DOUBLE_EQ(root_zero.derivative(0), 0.0);
  const auto negated = -mf::detail::make_dual(three, one);
  EXPECT_DOUBLE_EQ(negated.value(0), -3.0);
  EXPECT_DOUBLE_EQ(negated.derivative(0), -1.0);
}

TEST(mf61_model, UncombinedMoments_UseSelectedForces_MatchIndependentMomentAlgebra) {
  auto p = simple_parameters();
  p.coefficients.QSX3 = 0.2;
  p.coefficients.QSY2 = 0.03;
  p.coefficients.RBX1 = 7.0;
  p.coefficients.RCX1 = 1.0;
  p.coefficients.RBY1 = 8.0;
  p.coefficients.RCY1 = 1.0;
  const auto pure = mf::mf61_model::create(p, {.mode = mf::slip_mode::UNCOMBINED});
  const auto combined = mf::mf61_model::create(p);
  ASSERT_TRUE(pure.has_value());
  ASSERT_TRUE(combined.has_value());
  const Eigen::VectorXd input = point(0.1, 0.15, 0.0, 200000.0);
  const Eigen::VectorXd output = pure->evaluate(input);
  const Eigen::VectorXd combined_output = combined->evaluate(input);
  const double expected_mx = p.coefficients.UNLOADED_RADIUS * input(mf::mf6x::FZ) * p.coefficients.QSX3 *
                             output(mf::mf6x::FY) / p.coefficients.FNOMIN;
  const double expected_my = -p.coefficients.UNLOADED_RADIUS * p.coefficients.QSY2 * output(mf::mf6x::FX);
  expect_scaled(output(mf::mf6x::MX), expected_mx);
  expect_scaled(output(mf::mf6x::MY), expected_my);
  EXPECT_GT(std::abs(output(mf::mf6x::MX) - combined_output(mf::mf6x::MX)), 1.0);
  EXPECT_GT(std::abs(output(mf::mf6x::MY) - combined_output(mf::mf6x::MY)), 1.0);
}

TEST(mf61_model, TurnSlipAbsoluteValue_NegativeAndPositiveZeroSeeds_UseRightDirectionalSlope) {
  using lane = mf::detail::point_lane;
  using dual = mf::detail::dual_lane<lane>;
  auto p = simple_parameters();
  p.coefficients.UNLOADED_RADIUS = 1.0;
  p.coefficients.PDYP4 = 1.0;
  const mf::detail::mf61_equations<dual> equations(p.coefficients, 1, true);
  const lane zero = lane::Zero();
  const lane one = lane::Constant(1.0);
  const auto byp = mf::detail::make_dual(one, zero);
  for (const double seed : std::array<double, 2>{-1.0, 1.0}) {
    const lane seeded = lane::Constant(seed);
    const auto phi = mf::detail::make_dual(zero, seeded);
    const auto reduction = equations.zeta2(byp, phi);
    EXPECT_DOUBLE_EQ(reduction.value(0), 1.0);
    EXPECT_DOUBLE_EQ(reduction.derivative(0), -0.5);
    const auto absolute = mf::detail::mf61_abs(phi);
    EXPECT_DOUBLE_EQ(absolute.derivative(0), 1.0);
  }
}

TEST(mf61_model, TurnSlipCancellation_AnalyticLateralSlope_AgreesWithRightDifference) {
  auto p = simple_parameters();
  p.coefficients.UNLOADED_RADIUS = 1.0;
  p.coefficients.PDYP1 = 1.0;
  p.coefficients.PDYP4 = 1.0;
  p.coefficients.PVY1 = 0.02;
  const auto model = mf::mf61_model::create(p, {.mode = mf::slip_mode::UNCOMBINED, .turn_slip = true});
  ASSERT_TRUE(model.has_value());
  Eigen::VectorXd input = point(-0.1, 0.0, 0.05, 200000.0);
  input(mf::mf6x::FZ) = 4000.0;
  input(mf::mf6x::VX) = 1.0;
  const mf::detail::mf61_equations<mf::detail::point_lane> equations(p.coefficients, 1, true);
  const auto lane = [](double value) { return mf::detail::point_lane::Constant(value); };
  for (int correction = 0; correction < 5; ++correction) {
    const auto residual =
        equations.phi(lane(input(mf::mf6x::SA)), lane(0.0), lane(input(mf::mf6x::FZ)), lane(input(mf::mf6x::IA)),
                      lane(input(mf::mf6x::VX)), lane(input(mf::mf6x::P)), lane(input(mf::mf6x::TS)));
    input(mf::mf6x::TS) += residual(0);
  }
  const Eigen::VectorXd output = model->evaluate(input);
  const Eigen::VectorXd characteristics = model->evaluate_characteristics(input);
  constexpr double step = 1e-8;
  input(mf::mf6x::SA) += step;
  const Eigen::VectorXd higher = model->evaluate(input);
  const double numerical = (higher(mf::mf6x::FY) - output(mf::mf6x::FY)) / step;
  expect_scaled(characteristics(mf::mf6x::DFY_DSA), numerical, 2e-5);
}

TEST(mf61_model, DualForceValueLanes_AllModesAndLaneShapes_AreBitIdentical) {
  const auto parameters = source_parameters();
  const auto check = [&]<typename Lane_T>() {
    constexpr Eigen::Index count = Lane_T::RowsAtCompileTime == 1 ? 1 : 4;
    mf::detail::mf61_inputs<Lane_T> input;
    const Eigen::VectorXd operating = point(0.05, 0.08, 0.02, 230000.0, 0.03);
    for (std::size_t channel = 0; channel < input.size(); ++channel)
      input[channel] = Lane_T::Constant(count, operating(static_cast<Eigen::Index>(channel)));
    const Lane_T one = Lane_T::Constant(count, 1.0);
    for (const bool enabled : std::array<bool, 2>{false, true}) {
      for (const auto mode : std::array<mf::slip_mode, 2>{mf::slip_mode::COMBINED, mf::slip_mode::UNCOMBINED}) {
        const auto plain = mf::detail::mf61_planar(parameters.coefficients, mode, enabled, input);
        for (const std::size_t channel : std::array<std::size_t, 2>{mf::mf6x::SA, mf::mf6x::SR}) {
          const auto dual = mf::detail::mf61_planar(parameters.coefficients, mode, enabled,
                                                    mf::detail::mf61_seed(input, channel, input[channel], one));
          for (Eigen::Index row = 0; row < count; ++row) {
            EXPECT_EQ(std::bit_cast<std::uint64_t>(plain.fx(row)), std::bit_cast<std::uint64_t>(dual.fx.value(row)));
            EXPECT_EQ(std::bit_cast<std::uint64_t>(plain.fy(row)), std::bit_cast<std::uint64_t>(dual.fy.value(row)));
            EXPECT_EQ(std::bit_cast<std::uint64_t>(plain.dx(row)), std::bit_cast<std::uint64_t>(dual.dx.value(row)));
            EXPECT_EQ(std::bit_cast<std::uint64_t>(plain.dy(row)), std::bit_cast<std::uint64_t>(dual.dy.value(row)));
          }
        }
      }
    }
  };
  check.template operator()<mf::detail::point_lane>();
  check.template operator()<mf::detail::block_lane>();
}

TEST(mf61_model, ResidualAngle_ZeroStiffnessLimitsAndNaNs_AreExplicit) {
  auto parameters = simple_parameters();
  parameters.coefficients.QCRP2 = 1.0;
  const auto check = [&]<typename Lane_T>() {
    constexpr Eigen::Index count = Lane_T::RowsAtCompileTime == 1 ? 1 : 4;
    const mf::detail::mf61_equations<Lane_T> equations(parameters.coefficients, count, true);
    const Lane_T zero = Lane_T::Zero(count);
    const Lane_T one = Lane_T::Constant(count, 1.0);
    const Lane_T minus = Lane_T::Constant(count, -1.0);
    const Lane_T nan = Lane_T::Constant(count, std::numeric_limits<double>::quiet_NaN());
    const Lane_T neutral = equations.zeta7(zero, zero, zero, one, zero, zero);
    const Lane_T positive = equations.zeta7(zero, zero, one, one, zero, zero);
    const Lane_T negative = equations.zeta7(zero, zero, minus, one, zero, zero);
    const Lane_T invalid_regular = equations.zeta7(nan, one, one, one, one, zero);
    const Lane_T invalid_limit = equations.zeta7(zero, zero, nan, one, zero, zero);
    const Lane_T invalid_parking = equations.zeta7(zero, nan, one, one, one, zero);
    const Lane_T invalid_stiffness = equations.zeta7(zero, zero, one, one, nan, zero);
    const Lane_T invalid_reduction = equations.zeta7(zero, zero, one, one, one, nan);
    for (Eigen::Index row = 0; row < count; ++row) {
      EXPECT_DOUBLE_EQ(neutral(row), 1.0);
      EXPECT_DOUBLE_EQ(positive(row), 0.0);
      EXPECT_DOUBLE_EQ(negative(row), 2.0);
      EXPECT_TRUE(std::isnan(invalid_regular(row)));
      EXPECT_TRUE(std::isnan(invalid_limit(row)));
      EXPECT_TRUE(std::isnan(invalid_parking(row)));
      EXPECT_TRUE(std::isnan(invalid_stiffness(row)));
      EXPECT_TRUE(std::isnan(invalid_reduction(row)));
    }
  };
  check.template operator()<mf::detail::point_lane>();
  check.template operator()<mf::detail::block_lane>();
}

TEST(mf61_model, ReferenceCorneringStiffness_AllLaneForms_MatchesClosedFormWithAndWithoutTurnSlip) {
  auto parameters = simple_parameters();
  auto &c = parameters.coefficients;
  c.PPY1 = 0.2;
  c.PPY2 = 0.1;
  c.PKYP1 = 0.7;
  constexpr double fz_value = 3000.0;
  constexpr double pressure_delta = 0.1;
  constexpr double spin = 0.04;
  const double amplitude = c.PKY1 * c.FNOMIN * c.LFZ0 * (1.0 + c.PPY1 * pressure_delta) * c.LKY;
  const double slope = 1.0 / (c.FNOMIN * c.LFZ0 * c.PKY2 * (1.0 + c.PPY2 * pressure_delta));
  const double angle = c.PKY4 * std::atan(slope * fz_value);
  const double ordinary = amplitude * std::sin(angle);
  const double tangent = amplitude * c.PKY4 * slope * std::cos(angle) / (1.0 + (slope * fz_value) * (slope * fz_value));
  const double reduction = std::cos(std::atan(c.PKYP1 * c.UNLOADED_RADIUS * c.UNLOADED_RADIUS * spin * spin));
  const auto check = [&]<typename Lane_T>() {
    constexpr Eigen::Index count = Lane_T::RowsAtCompileTime == 1 ? 1 : 4;
    const Lane_T fz = Lane_T::Constant(count, fz_value);
    const Lane_T dpi = Lane_T::Constant(count, pressure_delta);
    const Lane_T phi = Lane_T::Constant(count, spin);
    const Lane_T zero = Lane_T::Zero(count);
    const Lane_T one = Lane_T::Constant(count, 1.0);
    for (const bool enabled : std::array<bool, 2>{false, true}) {
      const mf::detail::mf61_equations<Lane_T> equations(c, count, enabled);
      const mf::detail::mf61_equations<mf::detail::dual_lane<Lane_T>> differentiated(c, count, enabled);
      const Lane_T value = equations.kya0(fz, dpi, phi);
      const auto derivative = differentiated.kya0(mf::detail::make_dual(fz, one), mf::detail::make_dual(dpi, zero),
                                                  mf::detail::make_dual(phi, zero));
      const double factor = enabled ? reduction : 1.0;
      for (Eigen::Index row = 0; row < count; ++row) {
        expect_scaled(value(row), ordinary * factor);
        expect_scaled(derivative.value(row), ordinary * factor);
        expect_scaled(derivative.derivative(row), tangent * factor);
      }
    }
  };
  check.template operator()<mf::detail::point_lane>();
  check.template operator()<mf::detail::block_lane>();
}

TEST(mf61_model, RadiusIterationLimit_AllLaneForms_ConvergesOrReportsFailure) {
  const auto parameters = simple_parameters();
  const auto check = [&]<typename Lane_T>() {
    constexpr Eigen::Index count = Lane_T::RowsAtCompileTime == 1 ? 1 : 4;
    const Lane_T kappa = Lane_T::Constant(count, 0.1);
    const Lane_T fz = Lane_T::Constant(count, 4000.0);
    const Lane_T speed = Lane_T::Constant(count, 20.0);
    const Lane_T pressure = Lane_T::Constant(count, 200000.0);
    const Lane_T zero = Lane_T::Zero(count);
    const Lane_T one = Lane_T::Constant(count, 1.0);
    for (const bool fails : std::array<bool, 2>{false, true}) {
      auto c = parameters.coefficients;
      c.Q_V1 = fails ? 1e100 : 0.0;
      const mf::detail::mf61_equations<Lane_T> equations(c, count, true);
      const mf::detail::mf61_equations<mf::detail::dual_lane<Lane_T>> differentiated(c, count, true);
      const Lane_T radius = equations.re(kappa, fz, speed, pressure);
      const auto dual_radius =
          differentiated.re(mf::detail::make_dual(kappa, one), mf::detail::make_dual(fz, zero),
                            mf::detail::make_dual(speed, zero), mf::detail::make_dual(pressure, zero));
      for (Eigen::Index row = 0; row < count; ++row) {
        if (fails) {
          EXPECT_TRUE(std::isnan(radius(row)));
          EXPECT_TRUE(std::isnan(dual_radius.value(row)));
        } else {
          expect_scaled(radius(row), c.UNLOADED_RADIUS);
          expect_scaled(dual_radius.value(row), c.UNLOADED_RADIUS);
          EXPECT_DOUBLE_EQ(dual_radius.derivative(row), 0.0);
        }
      }
    }
  };
  check.template operator()<mf::detail::point_lane>();
  check.template operator()<mf::detail::block_lane>();
}
