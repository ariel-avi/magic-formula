#include "../support/fixtures.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <gtest/gtest.h>
#include <initializer_list>
#include <limits>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf52_model.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <numbers>
#include <string>
#include <utility>

namespace {
  namespace kernels = mf::detail;
  using coeffs = mf::mf5x_coefficients;
  using member_ptr = double coeffs::*;
  using parameters_t = mf::mf5x_parameters;
  using point_t = mf::operating_point;
  using result_t = std::expected<mf::tire_outputs, mf::error>;
  using mf::slip_mode;
  using mf::test::bits;
  using mf::test::printed_tol;
  using mf::test::rel_near_result;
  using mf::test::REL_TOL_CLOSED_FORM;
  using mf::test::REL_TOL_MF52;

  constexpr double FZ_NOMINAL = 4000.0;
  constexpr double SPEED_MPS = 10.0;
  // Absolute tolerance for outputs that are exactly zero in exact arithmetic.
  constexpr double ZERO_TOL = 1e-9;

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

  // Creates a model and evaluates one point; creation errors are returned like evaluation errors.
  result_t run(const parameters_t &parameters, const point_t &point, slip_mode mode) {
    const std::expected<mf::mf52_model, mf::error> model = mf::mf52_model::create(parameters);
    if (!model.has_value()) {
      return std::unexpected<mf::error>(model.error());
    }
    return model->evaluate(point, mode);
  }

  // The outputs of a result; an error fails the test and yields zero outputs.
  mf::tire_outputs unwrap(const result_t &result) {
    if (!result.has_value()) {
      ADD_FAILURE() << mf::to_string(result.error());
      return mf::tire_outputs{};
    }
    return *result;
  }

  mf::tire_outputs eval(const parameters_t &parameters, const point_t &point, slip_mode mode = slip_mode::COMBINED) {
    return unwrap(run(parameters, point, mode));
  }

  // cos'(alpha) = Vx/(Vc + eps) with Vc = hypot(Vx, tan(alpha)*Vx).
  double cos_prime(double alpha, double vx) {
    return vx / (std::hypot(vx, std::tan(alpha) * vx) + kernels::EPSILON);
  }

  std::array<double, 5> values_of(const mf::tire_outputs &out) {
    return {out.fx_n, out.fy_n, out.mz_nm, out.mx_nm, out.my_nm};
  }

  bool all_finite(const mf::tire_outputs &out) {
    return std::ranges::all_of(values_of(out), [](double value) { return std::isfinite(value); });
  }

  // Bit-equality of the five outputs (documented exception to "no EXPECT_EQ on doubles").
  void expect_same_bits(const mf::tire_outputs &actual, const mf::tire_outputs &expected) {
    EXPECT_EQ(bits(actual.fx_n), bits(expected.fx_n));
    EXPECT_EQ(bits(actual.fy_n), bits(expected.fy_n));
    EXPECT_EQ(bits(actual.mz_nm), bits(expected.mz_nm));
    EXPECT_EQ(bits(actual.mx_nm), bits(expected.mx_nm));
    EXPECT_EQ(bits(actual.my_nm), bits(expected.my_nm));
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
  parameters.limits.alpha_rad = mf::interval{.lower = 1.0, .upper = -1.0};
  const std::expected<mf::mf52_model, mf::error> model = mf::mf52_model::create(parameters);
  ASSERT_FALSE(model.has_value());
  EXPECT_EQ(model.error().code, mf::error_code::INVALID_VALUE);
  EXPECT_EQ(model.error().where.key, "ALPMIN");
}

TEST(mf52_model, Evaluate_ThroughBasePointer_MatchesDirectCall) {
  std::expected<mf::mf52_model, mf::error> created = mf::mf52_model::create(mf::test::fixture_s52());
  ASSERT_TRUE(created.has_value());
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.05, 0.02);
  const result_t direct = created->evaluate(point);
  ASSERT_TRUE(direct.has_value());
  const std::unique_ptr<const mf::tire_model> base = std::make_unique<mf::mf52_model>(std::move(*created));
  const result_t via_base = base->evaluate(point);
  ASSERT_TRUE(via_base.has_value());
  expect_same_bits(*via_base, *direct);
  EXPECT_EQ(base->name(), "MF 5.2");
}

// ---- vectors M1-M11 ---------------------------------------------------------------------------------------------

TEST(mf52_model, Evaluate_M1_PureLongitudinal) {
  const mf::tire_outputs out = eval(mf::test::fixture_s52(), make_point(FZ_NOMINAL, 0.1, 0.0));
  EXPECT_NEAR(out.fx_n, 3935.48, printed_tol(2));
  EXPECT_NEAR(out.fy_n, 0.0, ZERO_TOL);
}

TEST(mf52_model, Evaluate_M2_PureLateral) {
  const mf::tire_outputs out = eval(mf::test::fixture_s52(), make_point(FZ_NOMINAL, 0.0, 0.05));
  EXPECT_NEAR(out.fy_n, -2376.40, printed_tol(2));
}

TEST(mf52_model, Evaluate_M3_RawGamma) {
  // LGAX is ignored by MF 5.2: gamma enters raw, mu_x = 1 - PDX3*gamma^2 = 0.9 and Dx = 3600.
  const parameters_t parameters = s52_with({{&coeffs::PDX3, 10.0}, {&coeffs::LGAX, 0.5}});
  const mf::tire_outputs out = eval(parameters, make_point(FZ_NOMINAL, 0.1, 0.0, 0.1));
  constexpr double dx = 3600.0;
  const double bx = 80000.0 / ((1.5 * dx) + 1e-6);
  const double expected = dx * std::sin(1.5 * std::atan(bx * 0.1));
  EXPECT_TRUE(rel_near_result(out.fx_n, expected, REL_TOL_MF52));
}

TEST(mf52_model, Evaluate_M4_RollingResistance) {
  const mf::tire_outputs out = eval(mf::test::fixture_s52(), make_point(FZ_NOMINAL, 0.0, 0.0));
  EXPECT_TRUE(rel_near_result(out.my_nm, -12.0, REL_TOL_MF52));
}

TEST(mf52_model, Evaluate_M5_CombinedMz) {
  const mf::tire_outputs out = eval(mf::test::fixture_s52(), make_point(FZ_NOMINAL, 0.0, 0.05));
  EXPECT_NEAR(out.mz_nm, 60.46, printed_tol(2));
  EXPECT_NEAR(out.fx_n, 0.0, ZERO_TOL);
}

TEST(mf52_model, Evaluate_M6_Lfzo12) {
  const parameters_t parameters = s52_with({{&coeffs::LFZO, 1.2}});
  const point_t point = make_point(FZ_NOMINAL, 0.0, 0.05);
  const std::expected<mf::mf52_model, mf::error> model = mf::mf52_model::create(parameters);
  ASSERT_TRUE(model.has_value());
  const kernels::slip_state state = kernels::evaluate_slip_state(model->coefficients(), point);
  EXPECT_NEAR(state.lat.ky_n, -61132.08, printed_tol(2));
  // The effective trail amplitude Dt*LFZO of the MF 5.2 form equals the shared R0/FNOMIN form: 0.03 m.
  EXPECT_TRUE(rel_near_result(state.aln.dt_m, 0.03, REL_TOL_CLOSED_FORM));
  const result_t out = model->evaluate(point);
  ASSERT_TRUE(out.has_value());
  EXPECT_TRUE(rel_near_result(out->fy_n, state.lat.fy0_n, REL_TOL_CLOSED_FORM));
  const double trail_m = kernels::t(state.aln, state.aln.alpha_t, cos_prime(point.alpha_rad, point.vx_mps));
  EXPECT_TRUE(rel_near_result(-out->mz_nm / out->fy_n, trail_m, 1e-9));
}

TEST(mf52_model, Evaluate_M7_Phy3) {
  const parameters_t parameters = s52_with({{&coeffs::PHY3, 0.1}});
  const mf::tire_outputs out = eval(parameters, make_point(FZ_NOMINAL, 0.0, 0.0, 0.1));
  EXPECT_NEAR(out.fy_n, -550.02, printed_tol(2));
  EXPECT_NEAR(out.mz_nm, 16.50, printed_tol(2));
}

TEST(mf52_model, Evaluate_M8_ZeroVx_Rejected) {
  const point_t point = make_point(FZ_NOMINAL, 0.0, 0.05, 0.0, 0.0);
  const result_t result = run(mf::test::fixture_s52(), point, slip_mode::COMBINED);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
  EXPECT_NE(result.error().message.find("requires Vx > 0"), std::string::npos);
}

TEST(mf52_model, Evaluate_NegativeZeroVx_Rejected) {
  const point_t point = make_point(FZ_NOMINAL, 0.0, 0.05, 0.0, -0.0);
  const result_t result = run(mf::test::fixture_s52(), point, slip_mode::COMBINED);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
}

TEST(mf52_model, Evaluate_NegativeVx_Rejected) {
  const point_t point = make_point(FZ_NOMINAL, 0.0, 0.05, 0.0, -1.0);
  const result_t result = run(mf::test::fixture_s52(), point, slip_mode::COMBINED);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
}

TEST(mf52_model, Evaluate_NanVx_RejectedByBase) {
  const point_t point = make_point(FZ_NOMINAL, 0.0, 0.05, 0.0, std::numeric_limits<double>::quiet_NaN());
  const result_t result = run(mf::test::fixture_s52(), point, slip_mode::COMBINED);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
  EXPECT_NE(result.error().message.find("not finite"), std::string::npos);
}

TEST(mf52_model, Evaluate_M9_ZeroRbx1_NoFxReduction) {
  // Gxa = 1 when RBX1 = 0 (missing R* keys default to 0 in MF 5.2).
  const parameters_t parameters = s52_with({{&coeffs::RBX1, 0.0}});
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.1);
  const std::expected<mf::mf52_model, mf::error> model = mf::mf52_model::create(parameters);
  ASSERT_TRUE(model.has_value());
  const kernels::slip_state state = kernels::evaluate_slip_state(model->coefficients(), point);
  const result_t out = model->evaluate(point);
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(bits(out->fx_n), bits(state.lon.fx0_n));
}

TEST(mf52_model, Evaluate_M10_SvykZeroDiffers) {
  parameters_t parameters = mf::test::fixture_s52();
  parameters.coefficients.PDY3 = 1.0;
  parameters.coefficients.RVY1 = 0.1;
  parameters.coefficients.RVY5 = 1.0;
  parameters.coefficients.RVY6 = 10.0;
  parameters.coefficients.QCZ1 = 0.0;
  const mf::tire_outputs out = eval(parameters, make_point(FZ_NOMINAL, 0.1, 0.0, 0.1));
  // F'y = Fy - SVyk0 = SVyk - SVyk0 = 396*sin(pi/4) - 400*sin(pi/4) (mu_y = 0.99 at gamma = 0.1, 1 at gamma = 0),
  // t = Dt = 0.03 (Ct = 0): Mz = 0.03*400*(1 - 0.99)*sin(pi/4) = 0.0848528 N m.
  const double expected = 0.03 * 400.0 * (1.0 - 0.99) * std::sin(std::numbers::pi / 4.0);
  EXPECT_NEAR(expected, 0.0848528, 1e-7);
  EXPECT_TRUE(rel_near_result(out.mz_nm, expected, REL_TOL_MF52));
}

TEST(mf52_model, Evaluate_M11_TrailCosPrime) {
  const parameters_t parameters = s52_with({{&coeffs::QCZ1, 0.0}});
  const mf::tire_outputs out = eval(parameters, make_point(FZ_NOMINAL, 0.0, 0.05, 0.0, 1e-3));
  // Ct = 0 makes t = Dt*cos'(alpha), observed as t = -Mz/Fy.
  const double vc = 1e-3 / std::cos(0.05);
  const double expected = 0.03 * std::cos(0.05) * vc / (vc + 1e-6);
  EXPECT_TRUE(rel_near_result(-out.mz_nm / out.fy_n, expected, 1e-9));
  EXPECT_TRUE(out.flags.below_vxlow);
}

TEST(mf52_model, Evaluate_TinyVx_FiniteTrailVanishes) {
  const parameters_t parameters = mf::test::fixture_s52();
  const mf::tire_outputs reference = eval(parameters, make_point(FZ_NOMINAL, 0.0, 0.05));
  const mf::tire_outputs tiny = eval(parameters, make_point(FZ_NOMINAL, 0.0, 0.05, 0.0, 1e-12));
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
  const mf::tire_outputs combined = eval(parameters, point, slip_mode::COMBINED);
  const mf::tire_outputs pure = eval(parameters, point, slip_mode::UNCOMBINED);
  const kernels::slip_state state = kernels::evaluate_slip_state(parameters.coefficients, point);
  const double arm_m = kernels::s(parameters.coefficients, state.load, combined.fy_n, point.gamma_rad);
  // The Fx term is not trivially zero.
  EXPECT_GT(std::abs(arm_m * combined.fx_n), 0.1);
  EXPECT_TRUE(rel_near_result(combined.mz_nm - (arm_m * combined.fx_n), pure.mz_nm, REL_TOL_CLOSED_FORM));
}

TEST(mf52_model, Evaluate_Uncombined_PureForcesAndCosPrimeTrail) {
  parameters_t parameters = mf::test::fixture_s52();
  parameters.coefficients.QDZ6 = 0.01;
  parameters.coefficients.QBZ10 = 1.0;
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.05, 0.02);
  const mf::tire_outputs out = eval(parameters, point, slip_mode::UNCOMBINED);
  const kernels::slip_state state = kernels::evaluate_slip_state(parameters.coefficients, point);
  EXPECT_EQ(bits(out.fx_n), bits(state.lon.fx0_n));
  EXPECT_EQ(bits(out.fy_n), bits(state.lat.fy0_n));
  // Mz0 = -t(alpha_t, cos')*Fy0 + Mzr(alpha_r, cos); the residual moment uses cos(alpha).
  const double trail_m = kernels::t(state.aln, state.aln.alpha_t, cos_prime(point.alpha_rad, point.vx_mps));
  const double residual_nm = kernels::mzr(state.aln, state.aln.alpha_r, std::cos(point.alpha_rad));
  EXPECT_GT(std::abs(residual_nm), 0.0);
  EXPECT_TRUE(rel_near_result(out.mz_nm, -(trail_m * state.lat.fy0_n) + residual_nm, REL_TOL_CLOSED_FORM));
}

TEST(mf52_model, Evaluate_NonPositiveFz_ExactZeros) {
  const parameters_t parameters = mf::test::fixture_s52();
  for (const double fz : {0.0, -0.0, -100.0}) {
    SCOPED_TRACE(fz);
    for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
      const mf::tire_outputs out = eval(parameters, make_point(fz, 0.1, 0.05, 0.02), mode);
      EXPECT_EQ(bits(out.fx_n), bits(0.0));
      EXPECT_EQ(bits(out.fy_n), bits(0.0));
      EXPECT_EQ(bits(out.mz_nm), bits(0.0));
      EXPECT_EQ(bits(out.mx_nm), bits(0.0));
      EXPECT_EQ(bits(out.my_nm), bits(0.0));
    }
  }
}
