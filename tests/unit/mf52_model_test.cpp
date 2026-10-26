#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <gtest/gtest.h>
#include <initializer_list>
#include <limits>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf52_model.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_forms.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/model_checks.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/pac2002_model.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
  namespace kernels = mf::detail;
  using coeffs = mf::mf5x_coefficients;
  using member_ptr = double coeffs::*;
  using parameters_t = mf::mf5x_parameters;
  using point_t = mf::test::point;
  using out_idx = mf::mf52_model::output_index;
  using mf::slip_mode;
  using mf::test::bits;
  using mf::test::input_of;
  using mf::test::printed_tol;
  using mf::test::rel_near_result;
  using mf::test::REL_TOL_CLOSED_FORM;
  using mf::test::REL_TOL_MF52;

  constexpr double FZ_NOMINAL = 4000.0;
  constexpr double SPEED_MPS = 10.0;
  // Absolute tolerance for outputs that are exactly zero in exact arithmetic.
  constexpr double ZERO_TOL = 1e-9;

  // The ten outputs of one evaluation, read through the output index.
  struct outputs_t {
    double fx_n = 0.0;
    double fy_n = 0.0;
    double mz_nm = 0.0;
    double mx_nm = 0.0;
    double my_nm = 0.0;
    double fz_clamped = 0.0;
    double kappa_clamped = 0.0;
    double alpha_clamped = 0.0;
    double gamma_clamped = 0.0;
    double below_vxlow = 0.0;
  };

  outputs_t outputs_of(const Eigen::VectorXd &y) {
    return outputs_t{.fx_n = y(out_idx::FX),
                     .fy_n = y(out_idx::FY),
                     .mz_nm = y(out_idx::MZ),
                     .mx_nm = y(out_idx::MX),
                     .my_nm = y(out_idx::MY),
                     .fz_clamped = y(out_idx::FZ_CLAMPED),
                     .kappa_clamped = y(out_idx::SR_CLAMPED),
                     .alpha_clamped = y(out_idx::SA_CLAMPED),
                     .gamma_clamped = y(out_idx::IA_CLAMPED),
                     .below_vxlow = y(out_idx::BELOW_VXLOW)};
  }

  // Fixture S52 with the listed coefficients replaced.
  parameters_t s52_with(std::initializer_list<std::pair<member_ptr, double>> overrides) {
    parameters_t parameters = mf::test::fixture_s52();
    for (const auto &[member, value] : overrides) {
      parameters.coefficients.*member = value;
    }
    return parameters;
  }

  point_t make_point(double fz, double kappa, double alpha, double gamma = 0.0, double vx = SPEED_MPS) {
    return point_t{.fz_n = fz, .kappa = kappa, .alpha_rad = alpha, .gamma_rad = gamma, .vx_mps = vx};
  }

  // Creates a model; a creation error fails the test.
  std::optional<mf::mf52_model> make_model(const parameters_t &parameters, slip_mode mode) {
    std::expected<mf::mf52_model, mf::error> model =
        mf::mf52_model::create(parameters, mf::model_options{.mode = mode});
    if (!model.has_value()) {
      ADD_FAILURE() << mf::to_string(model.error());
      return std::nullopt;
    }
    return std::move(*model);
  }

  // Creates a model and evaluates one point; a creation error fails the test and yields zero outputs.
  outputs_t run(const parameters_t &parameters, const point_t &point, slip_mode mode) {
    const std::optional<mf::mf52_model> model = make_model(parameters, mode);
    if (!model.has_value()) {
      return outputs_t{};
    }
    return outputs_of(model->evaluate(input_of(point)));
  }

  outputs_t eval(const parameters_t &parameters, const point_t &point, slip_mode mode = slip_mode::COMBINED) {
    return run(parameters, point, mode);
  }

  // cos'(alpha) = Vx/(Vc + eps) with Vc = hypot(Vx, tan(alpha)*Vx).
  double cos_prime(double alpha, double vx) {
    return vx / (std::hypot(vx, std::tan(alpha) * vx) + kernels::EPSILON);
  }

  std::array<double, 5> values_of(const outputs_t &out) {
    return {out.fx_n, out.fy_n, out.mz_nm, out.mx_nm, out.my_nm};
  }

  bool all_finite(const outputs_t &out) {
    return std::ranges::all_of(values_of(out), [](double value) { return std::isfinite(value); });
  }

  // Bit-equality of the five outputs (documented exception to "no EXPECT_EQ on doubles").
  void expect_same_bits(const outputs_t &actual, const outputs_t &expected) {
    EXPECT_EQ(bits(actual.fx_n), bits(expected.fx_n));
    EXPECT_EQ(bits(actual.fy_n), bits(expected.fy_n));
    EXPECT_EQ(bits(actual.mz_nm), bits(expected.mz_nm));
    EXPECT_EQ(bits(actual.mx_nm), bits(expected.mx_nm));
    EXPECT_EQ(bits(actual.my_nm), bits(expected.my_nm));
  }

  // validate_input of the fixture S52 model for one point.
  std::expected<void, mf::error> validate(const point_t &point) {
    const std::expected<mf::mf52_model, mf::error> model = mf::mf52_model::create(mf::test::fixture_s52());
    if (!model.has_value()) {
      ADD_FAILURE() << mf::to_string(model.error());
      return {};
    }
    return mf::validate_input(*model, input_of(point));
  }
} // namespace

// ---- construction -----------------------------------------------------------------------------------------------

TEST(mf52_model, Create_FixtureS52_Succeeds) {
  const std::expected<mf::mf52_model, mf::error> model = mf::mf52_model::create(mf::test::fixture_s52());
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->name(), "MF 5.2");
  EXPECT_EQ(model->metadata().fittyp, 6);
  EXPECT_DOUBLE_EQ(model->coefficients().FNOMIN, FZ_NOMINAL);
}

TEST(mf52_model, Create_ForcesCamberScalingToOne) {
  parameters_t parameters = mf::test::fixture_s52();
  parameters.coefficients.LGAX = 0.5;
  parameters.coefficients.LGAY = 0.5;
  parameters.coefficients.LGAZ = 0.5;
  const std::expected<mf::mf52_model, mf::error> model = mf::mf52_model::create(parameters);
  ASSERT_TRUE(model.has_value());
  EXPECT_DOUBLE_EQ(model->coefficients().LGAX, 1.0);
  EXPECT_DOUBLE_EQ(model->coefficients().LGAY, 1.0);
  EXPECT_DOUBLE_EQ(model->coefficients().LGAZ, 1.0);
  // The other coefficients are untouched.
  EXPECT_DOUBLE_EQ(model->coefficients().PCX1, 1.5);
  EXPECT_DOUBLE_EQ(model->coefficients().LMUX, 1.0);
}

TEST(mf52_model, Create_InvalidCoefficient_PropagatesKey) {
  parameters_t parameters = mf::test::fixture_s52();
  parameters.coefficients.PKY2 = 0.0;
  const std::expected<mf::mf52_model, mf::error> model = mf::mf52_model::create(parameters);
  ASSERT_FALSE(model.has_value());
  EXPECT_EQ(model.error().code, mf::error_code::INVALID_VALUE);
  EXPECT_EQ(model.error().where.key, "PKY2");
}

TEST(mf52_model, Create_InvalidLimits_PropagatesKey) {
  parameters_t parameters = mf::test::fixture_s52();
  parameters.limits(mf::mf5x::ALPMIN) = 1.0;
  parameters.limits(mf::mf5x::ALPMAX) = -1.0;
  const std::expected<mf::mf52_model, mf::error> model = mf::mf52_model::create(parameters);
  ASSERT_FALSE(model.has_value());
  EXPECT_EQ(model.error().code, mf::error_code::INVALID_VALUE);
  EXPECT_EQ(model.error().where.key, "ALPMIN");
}

TEST(mf52_model, Create_WithOptions_StoresModeAndFlag) {
  const std::expected<mf::mf52_model, mf::error> defaults = mf::mf52_model::create(mf::test::fixture_s52());
  ASSERT_TRUE(defaults.has_value());
  EXPECT_EQ(defaults->options().mode, slip_mode::COMBINED);
  EXPECT_FALSE(defaults->options().throw_if_model_failure);

  const std::expected<mf::mf52_model, mf::error> custom = mf::mf52_model::create(
      mf::test::fixture_s52(), mf::model_options{.mode = slip_mode::UNCOMBINED, .throw_if_model_failure = true});
  ASSERT_TRUE(custom.has_value());
  EXPECT_EQ(custom->options().mode, slip_mode::UNCOMBINED);
  EXPECT_TRUE(custom->options().throw_if_model_failure);
}

TEST(mf52_model, Forms_FixedCosPrimeCosineSvyk0Qsy) {
  const std::expected<mf::mf52_model, mf::error> model = mf::mf52_model::create(mf::test::fixture_s52());
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->forms().trail_cosine, mf::trail_cosine_form::COS_PRIME);
  EXPECT_EQ(model->forms().combined, mf::combined_method::COSINE);
  EXPECT_EQ(model->forms().fy_prime, mf::fy_prime_form::SVYK0);
  EXPECT_EQ(model->forms().rolling_resistance, mf::rolling_resistance_form::QSY);
}

TEST(mf52_model, Layout_VxExclusiveNameLengths) {
  const std::expected<mf::mf52_model, mf::error> model = mf::mf52_model::create(mf::test::fixture_s52());
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(&model->layout(), &mf::mf5x::MF52_LAYOUT);
  EXPECT_EQ(model->name(), "MF 5.2");
  EXPECT_EQ(model->input_length(), 5);
  EXPECT_EQ(model->output_length(), 10);
  EXPECT_EQ(model->limits().size(), 9);
  EXPECT_FALSE(model->layout().inputs[mf::mf52_model::input_index::VX].minimum_inclusive);
}

TEST(mf52_model, Evaluate_ThroughBasePointer_MatchesDirectCall) {
  std::expected<mf::mf52_model, mf::error> created = mf::mf52_model::create(mf::test::fixture_s52());
  ASSERT_TRUE(created.has_value());
  const Eigen::VectorXd input = input_of(make_point(FZ_NOMINAL, 0.1, 0.05, 0.02));
  const outputs_t direct = outputs_of(created->evaluate(input));
  const std::unique_ptr<const mf::tire_model> base = std::make_unique<mf::mf52_model>(std::move(*created));
  const outputs_t via_base = outputs_of(base->evaluate(input));
  expect_same_bits(via_base, direct);
  EXPECT_EQ(base->name(), "MF 5.2");
}

TEST(mf52_model, EvaluateRaw_BitEqualToRef) {
  const std::expected<mf::mf52_model, mf::error> model = mf::mf52_model::create(mf::test::fixture_s_rng());
  ASSERT_TRUE(model.has_value());
  for (const point_t &p : mf::test::REGIME_POINTS) {
    const Eigen::VectorXd input = input_of(p);
    const Eigen::VectorXd expected = model->evaluate(input);
    std::array<double, mf::mf5x::OUTPUT_LENGTH> data{};
    model->evaluate(input.data(), mf::mf5x::INPUT_LENGTH, data.data(), mf::mf5x::OUTPUT_LENGTH);
    for (Eigen::Index j = 0; j < expected.size(); ++j) {
      SCOPED_TRACE(std::format("fz {} output {}", p.fz_n, j));
      EXPECT_EQ(bits(data.at(static_cast<std::size_t>(j))), bits(expected(j)));
    }
  }
}

// ---- vectors M1-M11 ---------------------------------------------------------------------------------------------

TEST(mf52_model, Evaluate_M1_PureLongitudinal) {
  const outputs_t out = eval(mf::test::fixture_s52(), make_point(FZ_NOMINAL, 0.1, 0.0));
  EXPECT_NEAR(out.fx_n, 3935.48, printed_tol(2));
  EXPECT_NEAR(out.fy_n, 0.0, ZERO_TOL);
}

TEST(mf52_model, Evaluate_M2_PureLateral) {
  const outputs_t out = eval(mf::test::fixture_s52(), make_point(FZ_NOMINAL, 0.0, 0.05));
  EXPECT_NEAR(out.fy_n, -2376.40, printed_tol(2));
}

TEST(mf52_model, Evaluate_M3_RawGamma) {
  // LGAX is ignored by MF 5.2: gamma enters raw, mu_x = 1 - PDX3*gamma^2 = 0.9 and Dx = 3600.
  const parameters_t parameters = s52_with({{&coeffs::PDX3, 10.0}, {&coeffs::LGAX, 0.5}});
  const outputs_t out = eval(parameters, make_point(FZ_NOMINAL, 0.1, 0.0, 0.1));
  constexpr double dx = 3600.0;
  const double bx = 80000.0 / ((1.5 * dx) + 1e-6);
  const double expected = dx * std::sin(1.5 * std::atan(bx * 0.1));
  EXPECT_TRUE(rel_near_result(out.fx_n, expected, REL_TOL_MF52));
}

TEST(mf52_model, Evaluate_M4_RollingResistance) {
  const outputs_t out = eval(mf::test::fixture_s52(), make_point(FZ_NOMINAL, 0.0, 0.0));
  EXPECT_TRUE(rel_near_result(out.my_nm, -12.0, REL_TOL_MF52));
}

TEST(mf52_model, Evaluate_M5_CombinedMz) {
  const outputs_t out = eval(mf::test::fixture_s52(), make_point(FZ_NOMINAL, 0.0, 0.05));
  EXPECT_NEAR(out.mz_nm, 60.46, printed_tol(2));
  EXPECT_NEAR(out.fx_n, 0.0, ZERO_TOL);
}

TEST(mf52_model, Evaluate_M6_Lfzo12) {
  const parameters_t parameters = s52_with({{&coeffs::LFZO, 1.2}});
  const point_t point = make_point(FZ_NOMINAL, 0.0, 0.05);
  const std::expected<mf::mf52_model, mf::error> model = mf::mf52_model::create(parameters);
  ASSERT_TRUE(model.has_value());
  const kernels::slip_state<kernels::point_lane> state = mf::test::slip_state_at(model->coefficients(), point);
  EXPECT_NEAR(state.lat.ky_n(0), -61132.08, printed_tol(2));
  // The effective trail amplitude Dt*LFZO of the MF 5.2 form equals the shared R0/FNOMIN form: 0.03 m.
  EXPECT_TRUE(rel_near_result(state.aln.dt_m(0), 0.03, REL_TOL_CLOSED_FORM));
  const outputs_t out = outputs_of(model->evaluate(input_of(point)));
  EXPECT_TRUE(rel_near_result(out.fy_n, state.lat.fy0_n(0), REL_TOL_CLOSED_FORM));
  const double trail_m =
      kernels::t(state.aln, state.aln.alpha_t, mf::test::lane(cos_prime(point.alpha_rad, point.vx_mps)))(0);
  EXPECT_TRUE(rel_near_result(-out.mz_nm / out.fy_n, trail_m, 1e-9));
}

TEST(mf52_model, Evaluate_M7_Phy3) {
  const parameters_t parameters = s52_with({{&coeffs::PHY3, 0.1}});
  const outputs_t out = eval(parameters, make_point(FZ_NOMINAL, 0.0, 0.0, 0.1));
  EXPECT_NEAR(out.fy_n, -550.02, printed_tol(2));
  EXPECT_NEAR(out.mz_nm, 16.50, printed_tol(2));
}

TEST(mf52_model, Evaluate_M8_ZeroVx_Rejected) {
  const std::expected<void, mf::error> result = validate(make_point(FZ_NOMINAL, 0.0, 0.05, 0.0, 0.0));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
  EXPECT_EQ(result.error().message, "VX = 0 m/s is outside the MF 5.2 domain (VX > 0)");
}

TEST(mf52_model, ValidateInput_ZeroAndNegativeZeroVx_Rejected) {
  for (const double vx : {0.0, -0.0}) {
    SCOPED_TRACE(vx);
    const std::expected<void, mf::error> result = validate(make_point(FZ_NOMINAL, 0.0, 0.05, 0.0, vx));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
    EXPECT_EQ(result.error().where.key, "VX");
    EXPECT_NE(result.error().message.find("outside the MF 5.2 domain (VX > 0)"), std::string::npos);
  }
}

TEST(mf52_model, Evaluate_NegativeZeroVx_Rejected) {
  const std::expected<void, mf::error> result = validate(make_point(FZ_NOMINAL, 0.0, 0.05, 0.0, -0.0));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
}

TEST(mf52_model, Evaluate_NegativeVx_Rejected) {
  const std::expected<void, mf::error> result = validate(make_point(FZ_NOMINAL, 0.0, 0.05, 0.0, -1.0));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
}

TEST(mf52_model, Evaluate_NanVx_RejectedByBase) {
  const std::expected<void, mf::error> result =
      validate(make_point(FZ_NOMINAL, 0.0, 0.05, 0.0, std::numeric_limits<double>::quiet_NaN()));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
  EXPECT_NE(result.error().message.find("not finite"), std::string::npos);
}

TEST(mf52_model, CheckedModel_ZeroVx_ThrowsModelFailure) {
  const std::expected<mf::mf52_model, mf::error> model =
      mf::mf52_model::create(mf::test::fixture_s52(), mf::model_options{.throw_if_model_failure = true});
  ASSERT_TRUE(model.has_value());
  // A valid point passes.
  EXPECT_NO_THROW((void)model->evaluate(input_of(make_point(FZ_NOMINAL, 0.0, 0.05))));
  bool thrown = false;
  try {
    (void)model->evaluate(input_of(make_point(FZ_NOMINAL, 0.0, 0.05, 0.0, 0.0)));
  } catch (const mf::model_failure &failure) {
    thrown = true;
    EXPECT_EQ(failure.cause().code, mf::error_code::INVALID_INPUT);
    EXPECT_EQ(failure.cause().where.key, "VX");
    EXPECT_FALSE(failure.row().has_value());
  }
  EXPECT_TRUE(thrown);
}

TEST(mf52_model, Evaluate_M9_ZeroRbx1_NoFxReduction) {
  // Gxa = 1 when RBX1 = 0 (missing R* keys default to 0 in MF 5.2).
  const parameters_t parameters = s52_with({{&coeffs::RBX1, 0.0}});
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.1);
  const std::expected<mf::mf52_model, mf::error> model = mf::mf52_model::create(parameters);
  ASSERT_TRUE(model.has_value());
  const kernels::slip_state<kernels::point_lane> state = mf::test::slip_state_at(model->coefficients(), point);
  const outputs_t out = outputs_of(model->evaluate(input_of(point)));
  EXPECT_EQ(bits(out.fx_n), bits(state.lon.fx0_n(0)));
}

TEST(mf52_model, Evaluate_M10_SvykZeroDiffers) {
  parameters_t parameters = mf::test::fixture_s52();
  parameters.coefficients.PDY3 = 1.0;
  parameters.coefficients.RVY1 = 0.1;
  parameters.coefficients.RVY5 = 1.0;
  parameters.coefficients.RVY6 = 10.0;
  parameters.coefficients.QCZ1 = 0.0;
  const outputs_t out = eval(parameters, make_point(FZ_NOMINAL, 0.1, 0.0, 0.1));
  // F'y = Fy - SVyk0 = SVyk - SVyk0 = 396*sin(pi/4) - 400*sin(pi/4) (mu_y = 0.99 at gamma = 0.1, 1 at gamma = 0),
  // t = Dt = 0.03 (Ct = 0): Mz = 0.03*400*(1 - 0.99)*sin(pi/4) = 0.0848528 N m.
  const double expected = 0.03 * 400.0 * (1.0 - 0.99) * std::sin(std::numbers::pi / 4.0);
  EXPECT_NEAR(expected, 0.0848528, 1e-7);
  EXPECT_TRUE(rel_near_result(out.mz_nm, expected, REL_TOL_MF52));
}

TEST(mf52_model, Evaluate_M11_TrailCosPrime) {
  const parameters_t parameters = s52_with({{&coeffs::QCZ1, 0.0}});
  const outputs_t out = eval(parameters, make_point(FZ_NOMINAL, 0.0, 0.05, 0.0, 1e-3));
  // Ct = 0 makes t = Dt*cos'(alpha), observed as t = -Mz/Fy.
  const double vc = 1e-3 / std::cos(0.05);
  const double expected = 0.03 * std::cos(0.05) * vc / (vc + 1e-6);
  EXPECT_TRUE(rel_near_result(-out.mz_nm / out.fy_n, expected, 1e-9));
  EXPECT_EQ(bits(out.below_vxlow), bits(1.0));
}

TEST(mf52_model, Evaluate_TinyVx_FiniteTrailVanishes) {
  const parameters_t parameters = mf::test::fixture_s52();
  const outputs_t reference = eval(parameters, make_point(FZ_NOMINAL, 0.0, 0.05));
  const outputs_t tiny = eval(parameters, make_point(FZ_NOMINAL, 0.0, 0.05, 0.0, 1e-12));
  EXPECT_TRUE(all_finite(tiny));
  EXPECT_LE(std::abs(tiny.mz_nm), 1e-4 * std::abs(reference.mz_nm));
}

// ---- property tests and modes -----------------------------------------------------------------------------------

TEST(mf52_model, Evaluate_P3_ZeroKappaIdentity) {
  parameters_t parameters = mf::test::fixture_s52();
  parameters.coefficients.PVX1 = 0.01;
  parameters.coefficients.SSZ1 = 0.1;
  parameters.coefficients.SSZ2 = 0.05;
  parameters.coefficients.PHY3 = 0.05;
  const point_t point = make_point(FZ_NOMINAL, 0.0, 0.05, 0.02);
  const outputs_t combined = eval(parameters, point, slip_mode::COMBINED);
  const outputs_t pure = eval(parameters, point, slip_mode::UNCOMBINED);
  const kernels::slip_state<kernels::point_lane> state = mf::test::slip_state_at(parameters.coefficients, point);
  const double arm_m = kernels::s(parameters.coefficients, state.load, mf::test::lane(combined.fy_n),
                                  mf::test::lane(point.gamma_rad))(0);
  // The Fx term is not trivially zero.
  EXPECT_GT(std::abs(arm_m * combined.fx_n), 0.1);
  EXPECT_TRUE(rel_near_result(combined.mz_nm - (arm_m * combined.fx_n), pure.mz_nm, REL_TOL_CLOSED_FORM));
}

TEST(mf52_model, Evaluate_Uncombined_PureForcesAndCosPrimeTrail) {
  parameters_t parameters = mf::test::fixture_s52();
  parameters.coefficients.QDZ6 = 0.01;
  parameters.coefficients.QBZ10 = 1.0;
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.05, 0.02);
  const outputs_t out = eval(parameters, point, slip_mode::UNCOMBINED);
  const kernels::slip_state<kernels::point_lane> state = mf::test::slip_state_at(parameters.coefficients, point);
  EXPECT_EQ(bits(out.fx_n), bits(state.lon.fx0_n(0)));
  EXPECT_EQ(bits(out.fy_n), bits(state.lat.fy0_n(0)));
  // Mz0 = -t(alpha_t, cos')*Fy0 + Mzr(alpha_r, cos); the residual moment uses cos(alpha).
  const double trail_m =
      kernels::t(state.aln, state.aln.alpha_t, mf::test::lane(cos_prime(point.alpha_rad, point.vx_mps)))(0);
  const double residual_nm = kernels::mzr(state.aln, state.aln.alpha_r, mf::test::lane(std::cos(point.alpha_rad)))(0);
  EXPECT_GT(std::abs(residual_nm), 0.0);
  EXPECT_TRUE(rel_near_result(out.mz_nm, -(trail_m * state.lat.fy0_n(0)) + residual_nm, REL_TOL_CLOSED_FORM));
}

TEST(mf52_model, Evaluate_NonPositiveFz_ExactZeros) {
  const parameters_t parameters = mf::test::fixture_s52();
  for (const double fz : {0.0, -0.0, -100.0}) {
    SCOPED_TRACE(fz);
    for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
      const outputs_t out = eval(parameters, make_point(fz, 0.1, 0.05, 0.02), mode);
      EXPECT_EQ(bits(out.fx_n), bits(0.0));
      EXPECT_EQ(bits(out.fy_n), bits(0.0));
      EXPECT_EQ(bits(out.mz_nm), bits(0.0));
      EXPECT_EQ(bits(out.mx_nm), bits(0.0));
      EXPECT_EQ(bits(out.my_nm), bits(0.0));
    }
  }
}

// ---- characteristics --------------------------------------------------------------------------------------------

namespace {
  // The operating points of the characteristics vectors: pure and combined slip of both signs, zero slips, no contact.
  std::vector<point_t> characteristic_points() {
    std::vector<point_t> points;
    points.push_back(make_point(FZ_NOMINAL, 0.1, 0.0));
    points.push_back(make_point(FZ_NOMINAL, 0.0, 0.05));
    points.push_back(make_point(FZ_NOMINAL, 0.1, 0.1));
    points.push_back(make_point(FZ_NOMINAL, 0.0, 0.0));
    points.push_back(make_point(FZ_NOMINAL, 0.0, -0.05));
    points.push_back(make_point(FZ_NOMINAL, -0.1, 0.0));
    points.push_back(make_point(FZ_NOMINAL, 0.05, -0.03));
    points.push_back(make_point(0.0, 0.1, 0.05));
    points.push_back(make_point(-100.0, 0.0, 0.0));
    return points;
  }

  // Both models of fixture S (PAC2002) and S52 (MF 5.2) in the given slip mode.
  struct model_pair {
    mf::pac2002_model pac2002;
    mf::mf52_model mf52;
  };

  std::optional<model_pair> make_pair(slip_mode mode) {
    std::expected<mf::pac2002_model, mf::error> pac =
        mf::pac2002_model::create(mf::test::fixture_s(), mf::combined_method::COSINE, mf::model_options{.mode = mode});
    std::optional<mf::mf52_model> mf52 = make_model(mf::test::fixture_s52(), mode);
    if (!pac.has_value()) {
      ADD_FAILURE() << mf::to_string(pac.error());
      return std::nullopt;
    }
    if (!mf52.has_value()) {
      return std::nullopt;
    }
    return model_pair{.pac2002 = std::move(*pac), .mf52 = std::move(*mf52)};
  }

  // Bit-equality, with any NaN equal to any NaN (the sign bit of a NaN carries no information).
  bool same_value(double actual, double expected) {
    if (std::isnan(actual) || std::isnan(expected)) {
      return std::isnan(actual) && std::isnan(expected);
    }
    return bits(actual) == bits(expected);
  }
} // namespace

namespace {
  // Point and batch characteristics of both models agree bit for bit on every row of `inputs`.
  void expect_models_bit_equal(const model_pair &models, const Eigen::MatrixXd &inputs) {
    const Eigen::MatrixXd batch_pac = models.pac2002.evaluate_characteristics_batch(inputs);
    const Eigen::MatrixXd batch_mf52 = models.mf52.evaluate_characteristics_batch(inputs);
    ASSERT_EQ(batch_pac.rows(), batch_mf52.rows());
    ASSERT_EQ(batch_pac.cols(), mf::mf5x::CHARACTERISTICS_LENGTH);
    for (Eigen::Index r = 0; r < inputs.rows(); ++r) {
      const Eigen::VectorXd input = inputs.row(r).transpose();
      const Eigen::VectorXd point_pac = models.pac2002.evaluate_characteristics(input);
      const Eigen::VectorXd point_mf52 = models.mf52.evaluate_characteristics(input);
      for (Eigen::Index j = 0; j < point_pac.size(); ++j) {
        EXPECT_TRUE(same_value(point_mf52(j), point_pac(j))) << "point row " << r << " characteristic " << j;
        EXPECT_TRUE(same_value(batch_mf52(r, j), batch_pac(r, j))) << "batch row " << r << " characteristic " << j;
      }
    }
  }

  // The batch characteristics of `model` equal its point characteristics at a scaled relative tolerance.
  void expect_batch_equals_point(const mf::mf52_model &model, const Eigen::MatrixXd &inputs) {
    // Slopes in N and N/rad, peak slips in rad and 1.
    constexpr double slope_scale = 1e4;
    constexpr double peak_scale = 0.1;
    const Eigen::MatrixXd batch = model.evaluate_characteristics_batch(inputs);
    ASSERT_EQ(batch.rows(), inputs.rows());
    ASSERT_EQ(batch.cols(), model.characteristics_length());
    for (Eigen::Index r = 0; r < inputs.rows(); ++r) {
      const Eigen::VectorXd input = inputs.row(r).transpose();
      const Eigen::VectorXd expected = model.evaluate_characteristics(input);
      for (Eigen::Index j = 0; j < expected.size(); ++j) {
        SCOPED_TRACE(std::format("row {} characteristic {}", r, j));
        if (same_value(batch(r, j), expected(j))) {
          continue;
        }
        const bool peak = j == mf::mf5x::SA_AT_PEAK_FY || j == mf::mf5x::SR_AT_PEAK_FX;
        const double scale = peak ? peak_scale : slope_scale;
        EXPECT_TRUE(mf::test::near_scaled(batch(r, j), expected(j), mf::test::BATCH_REL_TOL, scale));
      }
    }
  }
} // namespace

TEST(mf52_model, EvaluateCharacteristics_S52_BitEqualPac2002S) {
  const std::vector<point_t> points = characteristic_points();
  const Eigen::MatrixXd inputs = mf::test::inputs_of(points);
  for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
    SCOPED_TRACE(mode == slip_mode::COMBINED ? "combined" : "uncombined");
    const std::optional<model_pair> models = make_pair(mode);
    if (!models.has_value()) {
      return;
    }
    expect_models_bit_equal(*models, inputs);
  }
}

TEST(mf52_model, EvaluateCharacteristicsBatch_EqualPoint) {
  const std::vector<point_t> points = characteristic_points();
  const Eigen::MatrixXd inputs = mf::test::inputs_of(points);
  for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
    SCOPED_TRACE(mode == slip_mode::COMBINED ? "combined" : "uncombined");
    const std::optional<mf::mf52_model> model = make_model(mf::test::fixture_s52(), mode);
    if (!model.has_value()) {
      return;
    }
    expect_batch_equals_point(*model, inputs);
  }
}
