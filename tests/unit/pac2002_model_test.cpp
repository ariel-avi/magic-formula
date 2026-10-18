#include "../support/fixtures.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <gtest/gtest.h>
#include <initializer_list>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/pac2002_model.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <numbers>
#include <optional>
#include <utility>

namespace {
  namespace kernels = mf::detail;
  using coeffs = mf::mf5x_coefficients;
  using member_ptr = double coeffs::*;
  using parameters_t = mf::mf5x_parameters;
  using point_t = mf::operating_point;
  using result_t = std::expected<mf::tire_outputs, mf::error>;
  using flags_t = mf::input_flags;
  using mf::combined_method;
  using mf::slip_mode;
  using mf::test::bits;
  using mf::test::printed_tol;
  using mf::test::rel_near_result;
  using mf::test::REL_TOL_CLOSED_FORM;

  constexpr double FZ_NOMINAL = 4000.0;
  constexpr double SPEED_MPS = 10.0;
  // Absolute tolerance for outputs that are exactly zero in exact arithmetic.
  constexpr double ZERO_TOL = 1e-9;

  // Fixture S with the listed coefficients replaced.
  parameters_t s_with(std::initializer_list<std::pair<member_ptr, double>> overrides) {
    parameters_t parameters = mf::test::fixture_s();
    for (const auto &[member, value] : overrides) {
      parameters.coefficients.*member = value;
    }
    return parameters;
  }

  point_t make_point(double fz, double kappa, double alpha, double gamma = 0.0, double vx = SPEED_MPS) {
    return point_t{.fz_n = fz, .kappa = kappa, .alpha_rad = alpha, .gamma_rad = gamma, .vx_mps = vx};
  }

  // The raw IEEE-754 bit patterns of a struct of doubles (no padding).
  template <class Terms_T>
  std::array<std::uint64_t, sizeof(Terms_T) / sizeof(std::uint64_t)> raw_bits(const Terms_T &terms) {
    return std::bit_cast<std::array<std::uint64_t, sizeof(Terms_T) / sizeof(std::uint64_t)>>(terms);
  }

  // Creates a model and evaluates one point; creation errors are returned like evaluation errors.
  result_t run(const parameters_t &parameters, const point_t &point, slip_mode mode, combined_method method) {
    const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(parameters, method);
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
    return unwrap(run(parameters, point, mode, combined_method::COSINE));
  }

  mf::tire_outputs eval_ellipse(const parameters_t &parameters, const point_t &point) {
    return unwrap(run(parameters, point, slip_mode::COMBINED, combined_method::FRICTION_ELLIPSE));
  }

  kernels::slip_state state_of(const parameters_t &parameters, const point_t &point) {
    return kernels::evaluate_slip_state(parameters.coefficients, point);
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

  bool outputs_differ(const mf::tire_outputs &lhs, const mf::tire_outputs &rhs) {
    return !std::ranges::equal(values_of(lhs), values_of(rhs), std::ranges::equal_to{}, bits, bits);
  }

  // E-clamp check: member = 1.5 is bit-equal to 1.0, member = 0.999 is not.
  void expect_curvature_clamp(member_ptr member, const point_t &point) {
    const mf::tire_outputs clamped = eval(s_with({{member, 1.5}}), point);
    const mf::tire_outputs at_one = eval(s_with({{member, 1.0}}), point);
    const mf::tire_outputs below = eval(s_with({{member, 0.999}}), point);
    expect_same_bits(clamped, at_one);
    EXPECT_TRUE(outputs_differ(below, at_one));
  }

  point_t with(point_t point, double point_t::*member, double value) {
    point.*member = value;
    return point;
  }

  // Parameters of the eq (88) tests: nonzero SVx and SHx, so that eq (88) and eq (87) differ.
  parameters_t eq88_parameters(std::optional<int> fittyp, double qsy1, double qsy2) {
    parameters_t parameters = s_with({{&coeffs::PHX1, 0.001}, {&coeffs::PVX1, 0.01}});
    parameters.coefficients.QSY1 = qsy1;
    parameters.coefficients.QSY2 = qsy2;
    parameters.metadata.fittyp = fittyp;
    return parameters;
  }

  bool uses_eq88(const parameters_t &parameters) {
    const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(parameters);
    return model.has_value() && model->uses_eq88_rolling_resistance();
  }
} // namespace

// ---- construction -----------------------------------------------------------------------------------------------

TEST(pac2002_model, Create_FixtureS_CosineNoEq88) {
  const parameters_t parameters = mf::test::fixture_s();
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(parameters);
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->name(), "PAC2002");
  EXPECT_EQ(model->method(), combined_method::COSINE);
  EXPECT_FALSE(model->uses_eq88_rolling_resistance());
  EXPECT_EQ(raw_bits(model->coefficients()), raw_bits(parameters.coefficients));
  EXPECT_EQ(model->metadata().use_mode, 4);
  EXPECT_DOUBLE_EQ(model->limits().vxlow_mps, 1.0);
}

TEST(pac2002_model, Create_FrictionEllipse_RecordsMethod) {
  const std::expected<mf::pac2002_model, mf::error> model =
      mf::pac2002_model::create(mf::test::fixture_s(), combined_method::FRICTION_ELLIPSE);
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->method(), combined_method::FRICTION_ELLIPSE);
}

TEST(pac2002_model, Create_InvalidCoefficient_PropagatesKey) {
  parameters_t parameters = mf::test::fixture_s();
  parameters.coefficients.FNOMIN = 0.0;
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(parameters);
  ASSERT_FALSE(model.has_value());
  EXPECT_EQ(model.error().code, mf::error_code::INVALID_VALUE);
  EXPECT_EQ(model.error().where.key, "FNOMIN");
}

TEST(pac2002_model, Create_InvalidLimits_PropagatesKey) {
  parameters_t parameters = mf::test::fixture_s();
  parameters.limits.fz_n = mf::interval{.lower = 300.0, .upper = 200.0};
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(parameters);
  ASSERT_FALSE(model.has_value());
  EXPECT_EQ(model.error().code, mf::error_code::INVALID_VALUE);
  EXPECT_EQ(model.error().where.key, "FZMIN");
}

TEST(pac2002_model, Evaluate_ThroughBasePointer_MatchesDirectCall) {
  std::expected<mf::pac2002_model, mf::error> created = mf::pac2002_model::create(mf::test::fixture_s());
  ASSERT_TRUE(created.has_value());
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.05, 0.02);
  const result_t direct = created->evaluate(point);
  ASSERT_TRUE(direct.has_value());
  const std::unique_ptr<const mf::tire_model> base = std::make_unique<mf::pac2002_model>(std::move(*created));
  const result_t via_base = base->evaluate(point);
  ASSERT_TRUE(via_base.has_value());
  expect_same_bits(*via_base, *direct);
  EXPECT_EQ(base->name(), "PAC2002");
}

// ---- vectors V2-V22 ---------------------------------------------------------------------------------------------

TEST(pac2002_model, Evaluate_V2_CombinedAtZeroAlpha) {
  const mf::tire_outputs out = eval(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.1, 0.0));
  EXPECT_NEAR(out.fx_n, 3935.48, printed_tol(2));
  EXPECT_NEAR(out.fy_n, 0.0, ZERO_TOL);
}

TEST(pac2002_model, Evaluate_V3_PureLateralAtZeroKappa) {
  const mf::tire_outputs out = eval(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.0, 0.05));
  EXPECT_NEAR(out.fy_n, -2376.40, printed_tol(2));
  EXPECT_NEAR(out.fx_n, 0.0, ZERO_TOL);
}

TEST(pac2002_model, Evaluate_V4_Pvx1) {
  const mf::tire_outputs out = eval(s_with({{&coeffs::PVX1, 0.01}}), make_point(FZ_NOMINAL, 0.0, 0.0));
  EXPECT_TRUE(rel_near_result(out.fx_n, 40.0, REL_TOL_CLOSED_FORM));
}

TEST(pac2002_model, Evaluate_V5_RollingResistance) {
  const mf::tire_outputs out = eval(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.1, 0.05));
  EXPECT_TRUE(rel_near_result(out.my_nm, -12.0, REL_TOL_CLOSED_FORM));
}

TEST(pac2002_model, Evaluate_V6_OddSymmetry) {
  const parameters_t parameters = mf::test::fixture_s();
  for (const double slip : {0.001, 0.02, 0.1, 0.5, 1.0}) {
    SCOPED_TRACE(slip);
    const mf::tire_outputs positive = eval(parameters, make_point(FZ_NOMINAL, slip, 0.0));
    const mf::tire_outputs negative = eval(parameters, make_point(FZ_NOMINAL, -slip, 0.0));
    EXPECT_GT(positive.fx_n, 0.0);
    EXPECT_TRUE(rel_near_result(negative.fx_n, -positive.fx_n, REL_TOL_CLOSED_FORM));
    for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
      const mf::tire_outputs left = eval(parameters, make_point(FZ_NOMINAL, 0.0, slip), mode);
      const mf::tire_outputs right = eval(parameters, make_point(FZ_NOMINAL, 0.0, -slip), mode);
      EXPECT_LT(left.fy_n, 0.0);
      EXPECT_TRUE(rel_near_result(right.fy_n, -left.fy_n, REL_TOL_CLOSED_FORM));
      EXPECT_TRUE(rel_near_result(right.mz_nm, -left.mz_nm, REL_TOL_CLOSED_FORM));
    }
  }
}

TEST(pac2002_model, Evaluate_V8_Lmux05) {
  const mf::tire_outputs out = eval(s_with({{&coeffs::LMUX, 0.5}}), make_point(FZ_NOMINAL, 0.1, 0.0));
  // Dx = 0.5*4000 = 2000 and Bx = Kx/(Cx*Dx + eps) with Kx = 80000 (LMUX does not scale Kx).
  constexpr double dx = 2000.0;
  const double bx = 80000.0 / ((1.5 * dx) + 1e-6);
  const double expected = dx * std::sin(1.5 * std::atan(bx * 0.1));
  EXPECT_TRUE(rel_near_result(out.fx_n, expected, 1e-9));
}

TEST(pac2002_model, Evaluate_V9_Overturning) {
  parameters_t parameters = mf::test::fixture_s();
  parameters.coefficients.QSX1 = 0.01;
  parameters.coefficients.QSX2 = 0.5;
  parameters.coefficients.QSX3 = 0.1;
  const mf::tire_outputs out = eval(parameters, make_point(FZ_NOMINAL, 0.0, 0.0, 0.05));
  EXPECT_NEAR(out.fy_n, 0.0, ZERO_TOL);
  EXPECT_TRUE(rel_near_result(out.mx_nm, -18.0, REL_TOL_CLOSED_FORM));
}

TEST(pac2002_model, Evaluate_V10_Eq88) {
  parameters_t parameters = s_with({{&coeffs::QSY1, 0.0}, {&coeffs::PHX1, 0.001}, {&coeffs::PVX1, 0.01}});
  parameters.metadata.fittyp = 5;
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(parameters);
  ASSERT_TRUE(model.has_value());
  EXPECT_TRUE(model->uses_eq88_rolling_resistance());
  const result_t out = model->evaluate(make_point(FZ_NOMINAL, 0.0, 0.0));
  ASSERT_TRUE(out.has_value());
  EXPECT_TRUE(rel_near_result(out->my_nm, -36.0, REL_TOL_CLOSED_FORM));
}

TEST(pac2002_model, Evaluate_Eq88Combinations) {
  const point_t point = make_point(FZ_NOMINAL, 0.0, 0.0);

  // No FITTYP: eq (87) with QSY1 = QSY2 = 0 gives zero.
  const parameters_t no_fittyp = eq88_parameters(std::nullopt, 0.0, 0.0);
  EXPECT_FALSE(uses_eq88(no_fittyp));
  EXPECT_NEAR(eval(no_fittyp, point).my_nm, 0.0, 1e-12);

  // FITTYP 6: eq (87).
  const parameters_t fittyp6 = eq88_parameters(6, 0.0, 0.0);
  EXPECT_FALSE(uses_eq88(fittyp6));
  EXPECT_NEAR(eval(fittyp6, point).my_nm, 0.0, 1e-12);

  // FITTYP 5 with QSY1 != 0: eq (87).
  const parameters_t qsy1_set = eq88_parameters(5, 0.01, 0.0);
  EXPECT_FALSE(uses_eq88(qsy1_set));
  EXPECT_TRUE(rel_near_result(eval(qsy1_set, point).my_nm, -12.0, REL_TOL_CLOSED_FORM));

  // FITTYP 5 with QSY1 = 0 and QSY2 != 0: eq (87), My = -R0*Fz*QSY2*Fx/FNOMIN = -0.03*Fx.
  const parameters_t qsy2_set = eq88_parameters(5, 0.0, 0.1);
  EXPECT_FALSE(uses_eq88(qsy2_set));
  const mf::tire_outputs qsy2_out = eval(qsy2_set, point);
  EXPECT_GT(qsy2_out.fx_n, 1.0);
  EXPECT_TRUE(rel_near_result(qsy2_out.my_nm, -0.03 * qsy2_out.fx_n, REL_TOL_CLOSED_FORM));
  EXPECT_GT(std::abs(qsy2_out.my_nm - (-36.0)), 1.0);

  // FITTYP 5 with QSY1 = QSY2 = 0: eq (88).
  const parameters_t both_zero = eq88_parameters(5, 0.0, 0.0);
  EXPECT_TRUE(uses_eq88(both_zero));
  EXPECT_TRUE(rel_near_result(eval(both_zero, point).my_nm, -36.0, REL_TOL_CLOSED_FORM));
}

TEST(pac2002_model, Evaluate_V11_UncombinedTrail) {
  const mf::tire_outputs out = eval(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.0, 0.05), slip_mode::UNCOMBINED);
  EXPECT_NEAR(-out.mz_nm / out.fy_n, 0.0254434, 5e-8);
  EXPECT_NEAR(out.mz_nm, 60.46, printed_tol(2));
}

TEST(pac2002_model, Evaluate_V12_CombinedFx) {
  const mf::tire_outputs out = eval(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.1, 0.1));
  EXPECT_NEAR(out.fx_n, 2782.80, printed_tol(2));
}

TEST(pac2002_model, Evaluate_V13_CombinedFy) {
  const mf::tire_outputs out = eval(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.1, 0.05));
  EXPECT_NEAR(out.fy_n, -1680.37, printed_tol(2));
}

TEST(pac2002_model, Evaluate_V14_KappaInducedSideForce) {
  parameters_t parameters = mf::test::fixture_s();
  parameters.coefficients.RBY1 = 0.0;
  parameters.coefficients.RVY1 = 0.1;
  parameters.coefficients.RVY5 = 1.0;
  parameters.coefficients.RVY6 = 10.0;
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.05);
  const mf::tire_outputs out = eval(parameters, point);
  // Gyk = 1 (RBY1 = 0), so Fy = Fy0 + SVyk with SVyk = 400*sin(atan(1)).
  const double svyk_n = 400.0 * std::sin(std::numbers::pi / 4.0);
  EXPECT_TRUE(rel_near_result(out.fy_n, state_of(parameters, point).lat.fy0_n + svyk_n, REL_TOL_CLOSED_FORM));
  // The printed -2093.56 is the sum of two rounded terms, hence the tolerance of one unit of the last digit.
  EXPECT_NEAR(out.fy_n, -2093.56, 2.0 * printed_tol(2));
}

TEST(pac2002_model, Evaluate_V15_ExClamp) {
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.0);
  const mf::tire_outputs clamped = eval(s_with({{&coeffs::PEX1, 1.5}}), point);
  const mf::tire_outputs at_one = eval(s_with({{&coeffs::PEX1, 1.0}}), point);
  const mf::tire_outputs below = eval(s_with({{&coeffs::PEX1, 0.999}}), point);
  // V15: bit-equal to PEX1 = 1.0.
  expect_same_bits(clamped, at_one);
  EXPECT_NEAR(clamped.fx_n, 3603.08, printed_tol(2));
  EXPECT_NE(bits(below.fx_n), bits(at_one.fx_n));
}

TEST(pac2002_model, Evaluate_EClamps_EyEtExaEyk) {
  // Ey: pure lateral force; Et: aligning trail; Exa: Gxa in Fx; Eyk: Gyk in Fy. 1.5 is bit-equal to 1.0 (D-17).
  expect_curvature_clamp(&coeffs::PEY1, make_point(FZ_NOMINAL, 0.0, 0.05));
  expect_curvature_clamp(&coeffs::QEZ1, make_point(FZ_NOMINAL, 0.0, 0.05));
  expect_curvature_clamp(&coeffs::REX1, make_point(FZ_NOMINAL, 0.1, 0.1));
  expect_curvature_clamp(&coeffs::REY1, make_point(FZ_NOMINAL, 0.1, 0.05));
}

TEST(pac2002_model, Evaluate_V16_GammaX) {
  const parameters_t parameters = s_with({{&coeffs::PDX3, 10.0}, {&coeffs::LGAX, 0.5}});
  const mf::tire_outputs out = eval(parameters, make_point(FZ_NOMINAL, 0.1, 0.0, 0.1));
  // mu_x = 1 - PDX3*(gamma*LGAX)^2 = 0.975, so Dx = 3900.
  constexpr double dx = 3900.0;
  const double bx = 80000.0 / ((1.5 * dx) + 1e-6);
  const double expected = dx * std::sin(1.5 * std::atan(bx * 0.1));
  EXPECT_TRUE(rel_near_result(out.fx_n, expected, REL_TOL_CLOSED_FORM));
}

TEST(pac2002_model, Evaluate_V17_ReadingA) {
  const parameters_t parameters = s_with({{&coeffs::PHY3, 0.1}});
  for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
    const mf::tire_outputs out = eval(parameters, make_point(FZ_NOMINAL, 0.0, 0.0, 0.1), mode);
    EXPECT_NEAR(out.fy_n, -550.02, printed_tol(2));
    EXPECT_NEAR(out.mz_nm, 16.50, printed_tol(2));
  }
}

TEST(pac2002_model, Evaluate_V18_Lfzo12) {
  const parameters_t parameters = s_with({{&coeffs::LFZO, 1.2}});
  const point_t point = make_point(FZ_NOMINAL, 0.0, 0.05);
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(parameters);
  ASSERT_TRUE(model.has_value());
  const kernels::slip_state state = kernels::evaluate_slip_state(model->coefficients(), point);
  EXPECT_TRUE(rel_near_result(state.aln.dt_m, 0.03, REL_TOL_CLOSED_FORM));
  EXPECT_NEAR(state.lat.ky0_n, -61132.08, printed_tol(2));
  const double cos_alpha = std::cos(point.alpha_rad);
  const double trail_m = kernels::trail(state.aln, state.aln.alpha_t, cos_alpha);
  const double expected_mz =
      -(trail_m * state.lat.fy0_n) + kernels::residual_moment(state.aln, state.aln.alpha_r, cos_alpha);
  const result_t pure = model->evaluate(point, slip_mode::UNCOMBINED);
  ASSERT_TRUE(pure.has_value());
  EXPECT_TRUE(rel_near_result(pure->fy_n, state.lat.fy0_n, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(pure->mz_nm, expected_mz, REL_TOL_CLOSED_FORM));
  const result_t combined = model->evaluate(point, slip_mode::COMBINED);
  ASSERT_TRUE(combined.has_value());
  EXPECT_TRUE(rel_near_result(combined->mz_nm, expected_mz, REL_TOL_CLOSED_FORM));
}

TEST(pac2002_model, Evaluate_V19_P6_BeyondLimits_EqualsAtLimit) {
  const parameters_t parameters = mf::test::fixture_s_rng();
  const point_t base = make_point(FZ_NOMINAL, 0.1, 0.05, 0.02);
  const auto check = [&parameters](const point_t &beyond, const point_t &at_limit, bool flags_t::*flag) {
    const mf::tire_outputs clamped = eval(parameters, beyond);
    const mf::tire_outputs reference = eval(parameters, at_limit);
    // V19 / P6: bit-equal to the evaluation at the limit; the exact limit itself is not flagged.
    expect_same_bits(clamped, reference);
    EXPECT_TRUE(clamped.flags.*flag);
    EXPECT_FALSE(reference.flags.*flag);
  };
  check(with(base, &point_t::kappa, 10.0), with(base, &point_t::kappa, 1.0), &flags_t::kappa_clamped);
  check(with(base, &point_t::alpha_rad, 2.0), with(base, &point_t::alpha_rad, 1.5), &flags_t::alpha_clamped);
  check(with(base, &point_t::gamma_rad, 0.5), with(base, &point_t::gamma_rad, 0.2), &flags_t::gamma_clamped);
  check(with(base, &point_t::fz_n, 20000.0), with(base, &point_t::fz_n, 10000.0), &flags_t::fz_clamped);
  check(with(base, &point_t::fz_n, 1e-9), with(base, &point_t::fz_n, 200.0), &flags_t::fz_clamped);
}

TEST(pac2002_model, Evaluate_V20_TinyFz_Finite) {
  const parameters_t parameters = mf::test::fixture_s();
  for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
    EXPECT_TRUE(all_finite(eval(parameters, make_point(1e-9, 0.1, 0.05, 0.02), mode)));
    const mf::tire_outputs pure_slip = eval(parameters, make_point(1e-9, 0.1, 0.0), mode);
    EXPECT_TRUE(all_finite(pure_slip));
    EXPECT_LE(std::abs(pure_slip.fx_n), 1e-9);
  }
}

TEST(pac2002_model, Evaluate_FzTiny1em300_Finite) {
  const parameters_t parameters = mf::test::fixture_s();
  for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
    const mf::tire_outputs out = eval(parameters, make_point(1e-300, 0.1, 0.05, 0.02), mode);
    EXPECT_TRUE(all_finite(out));
    EXPECT_LE(std::abs(out.fx_n), 1e-9);
  }
}

TEST(pac2002_model, Evaluate_Fz1e300_Finite) {
  // Both modes stay finite at Fz = 1e300; My = -R0 * Fz * QSY1 = -0.3 * 1e300 * 0.01 (fixture S, QSY2..4 = 0).
  for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
    const mf::tire_outputs out = eval(mf::test::fixture_s(), make_point(1e300, 0.1, 0.05, 0.02), mode);
    EXPECT_TRUE(all_finite(out));
    EXPECT_GT(out.fx_n, 1e299);
    EXPECT_TRUE(rel_near_result(out.my_nm, -0.3 * 1e300 * 0.01, REL_TOL_CLOSED_FORM));
  }
}

TEST(pac2002_model, Evaluate_V21_SpeedTerms) {
  const parameters_t parameters = s_with({{&coeffs::QSY3, 0.1}, {&coeffs::QSY4, 0.01}});
  const mf::tire_outputs out = eval(parameters, make_point(FZ_NOMINAL, 0.1, 0.05));
  const double ratio = SPEED_MPS / 16.6;
  const double expected = -1200.0 * (0.01 + (0.1 * ratio) + (0.01 * std::pow(ratio, 4.0)));
  EXPECT_TRUE(rel_near_result(out.my_nm, expected, REL_TOL_CLOSED_FORM));
  EXPECT_NEAR(out.my_nm, -85.87, printed_tol(2));
}

TEST(pac2002_model, Evaluate_V22_NonPositiveFz_ExactZeros) {
  const parameters_t parameters = mf::test::fixture_s();
  for (const double fz : {0.0, -0.0, -100.0}) {
    SCOPED_TRACE(fz);
    for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
      const mf::tire_outputs out = eval(parameters, make_point(fz, 0.1, 0.05, 0.02), mode);
      // V22: bit-equal to +0.0.
      EXPECT_EQ(bits(out.fx_n), bits(0.0));
      EXPECT_EQ(bits(out.fy_n), bits(0.0));
      EXPECT_EQ(bits(out.mz_nm), bits(0.0));
      EXPECT_EQ(bits(out.mx_nm), bits(0.0));
      EXPECT_EQ(bits(out.my_nm), bits(0.0));
    }
  }
}

// ---- property tests ---------------------------------------------------------------------------------------------

TEST(pac2002_model, Evaluate_P3_ZeroKappaIdentity) {
  parameters_t parameters = mf::test::fixture_s();
  parameters.coefficients.PVX1 = 0.01;
  parameters.coefficients.SSZ1 = 0.1;
  parameters.coefficients.SSZ2 = 0.05;
  parameters.coefficients.PHY3 = 0.05;
  const point_t point = make_point(FZ_NOMINAL, 0.0, 0.05, 0.02);
  const mf::tire_outputs combined = eval(parameters, point, slip_mode::COMBINED);
  const mf::tire_outputs pure = eval(parameters, point, slip_mode::UNCOMBINED);
  const kernels::slip_state state = state_of(parameters, point);
  const double arm_m = kernels::moment_arm(parameters.coefficients, state.load, combined.fy_n, point.gamma_rad);
  // The Fx term is not trivially zero.
  EXPECT_GT(std::abs(arm_m * combined.fx_n), 0.1);
  EXPECT_TRUE(rel_near_result(combined.mz_nm - (arm_m * combined.fx_n), pure.mz_nm, REL_TOL_CLOSED_FORM));
}

TEST(pac2002_model, Evaluate_P4_MzContinuousAcrossAlphaTZero) {
  parameters_t parameters = mf::test::fixture_s();
  parameters.coefficients.QHZ1 = 0.01;
  // alpha_t = alpha + QHZ1 = -delta, 0, +delta.
  constexpr double delta = 1e-9;
  const double mz_below = eval(parameters, make_point(FZ_NOMINAL, 0.1, -0.01 - delta)).mz_nm;
  const double mz_zero = eval(parameters, make_point(FZ_NOMINAL, 0.1, -0.01)).mz_nm;
  const double mz_above = eval(parameters, make_point(FZ_NOMINAL, 0.1, -0.01 + delta)).mz_nm;
  EXPECT_GT(std::abs(mz_zero), 1.0);
  EXPECT_LE(std::abs(mz_below - mz_zero), 1e-6 * std::abs(mz_zero));
  EXPECT_LE(std::abs(mz_above - mz_zero), 1e-6 * std::abs(mz_zero));
}

TEST(pac2002_model, Evaluate_P15_Uncombined_ForcesArePure) {
  const parameters_t parameters = mf::test::fixture_s();
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.05, 0.02);
  const mf::tire_outputs out = eval(parameters, point, slip_mode::UNCOMBINED);
  const kernels::slip_state state = state_of(parameters, point);
  // P15: bit-equal to the pure-slip kernels.
  EXPECT_EQ(bits(out.fx_n), bits(state.lon.fx0_n));
  EXPECT_EQ(bits(out.fy_n), bits(state.lat.fy0_n));
}

TEST(pac2002_model, Evaluate_Uncombined_MxMyFromPureForces) {
  parameters_t parameters = mf::test::fixture_s();
  parameters.coefficients.QSX1 = 0.01;
  parameters.coefficients.QSX3 = 0.1;
  parameters.coefficients.QSY2 = 0.1;
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.05, 0.02);
  const kernels::slip_state state = state_of(parameters, point);
  const coeffs &c = parameters.coefficients;
  const double expected_mx = kernels::overturning_moment(c, point.fz_n, state.lat.fy0_n, point.gamma_rad);
  const double expected_my = kernels::rolling_resistance_moment(c, point.fz_n, state.lon.fx0_n, point.vx_mps);
  const mf::tire_outputs pure = eval(parameters, point, slip_mode::UNCOMBINED);
  EXPECT_TRUE(rel_near_result(pure.mx_nm, expected_mx, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(pure.my_nm, expected_my, REL_TOL_CLOSED_FORM));
  // The combined forces differ from the pure ones, so Mx and My differ too.
  const mf::tire_outputs combined = eval(parameters, point, slip_mode::COMBINED);
  EXPECT_GT(std::abs(combined.mx_nm - pure.mx_nm), 1e-3);
  EXPECT_GT(std::abs(combined.my_nm - pure.my_nm), 1e-3);
}

TEST(pac2002_model, Evaluate_LmuyHalf_HalvesDyDoublesBy) {
  const point_t point = make_point(FZ_NOMINAL, 0.0, 0.05);
  const std::expected<mf::pac2002_model, mf::error> base = mf::pac2002_model::create(mf::test::fixture_s());
  const std::expected<mf::pac2002_model, mf::error> half = mf::pac2002_model::create(s_with({{&coeffs::LMUY, 0.5}}));
  ASSERT_TRUE(base.has_value());
  ASSERT_TRUE(half.has_value());
  const kernels::slip_state base_state = kernels::evaluate_slip_state(base->coefficients(), point);
  const kernels::slip_state half_state = kernels::evaluate_slip_state(half->coefficients(), point);
  EXPECT_TRUE(rel_near_result(half_state.lat.dy_n, 0.5 * base_state.lat.dy_n, REL_TOL_CLOSED_FORM));
  // By = Ky/(Cy*Dy + eps): the eps term perturbs the exact doubling at the 1e-9 level.
  EXPECT_TRUE(rel_near_result(half_state.lat.by, 2.0 * base_state.lat.by, 1e-8));
  const result_t half_out = half->evaluate(point, slip_mode::UNCOMBINED);
  ASSERT_TRUE(half_out.has_value());
  EXPECT_EQ(bits(half_out->fy_n), bits(half_state.lat.fy0_n));
}

TEST(pac2002_model, Evaluate_FrictionEllipse_UsesRatiosAndSvykZero) {
  parameters_t parameters = mf::test::fixture_s();
  parameters.coefficients.PHX1 = 0.001;
  parameters.coefficients.PVX1 = 0.01;
  parameters.coefficients.PHY1 = 0.002;
  parameters.coefficients.PVY1 = 0.01;
  parameters.coefficients.SSZ1 = 0.1;
  parameters.coefficients.QDZ6 = 0.01;
  parameters.coefficients.QBZ10 = 1.0;
  // A nonzero SVyk that the cosine method adds to Fy and the ellipse must ignore.
  parameters.coefficients.RVY1 = 0.1;
  parameters.coefficients.RVY5 = 1.0;
  parameters.coefficients.RVY6 = 10.0;
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.05, 0.02);

  const mf::tire_outputs out = eval_ellipse(parameters, point);
  const kernels::slip_state state = state_of(parameters, point);
  const kernels::ellipse_ratios ratios = kernels::friction_ellipse_ratios(state, point.kappa, point.alpha_rad);
  const double expected_fx = state.lon.fx0_n * ratios.rx;
  const double expected_fy = state.lat.fy0_n * ratios.ry;
  EXPECT_TRUE(rel_near_result(out.fx_n, expected_fx, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(out.fy_n, expected_fy, REL_TOL_CLOSED_FORM));

  // Eq (78) with F'y = Fy (SVyk = 0).
  const coeffs &c = parameters.coefficients;
  const double cos_alpha = std::cos(point.alpha_rad);
  const double ky_guarded_n = kernels::add_epsilon(state.lat.ky_n);
  const double ate = kernels::equivalent_slip_angle(state.aln.alpha_t, point.kappa, state.lon.kx_n, ky_guarded_n);
  const double are = kernels::equivalent_slip_angle(state.aln.alpha_r, point.kappa, state.lon.kx_n, ky_guarded_n);
  const double arm_m = kernels::moment_arm(c, state.load, expected_fy, point.gamma_rad);
  const double trail_m = kernels::trail(state.aln, ate, cos_alpha);
  const double expected_mz =
      -(trail_m * expected_fy) + kernels::residual_moment(state.aln, are, cos_alpha) + (arm_m * expected_fx);
  EXPECT_TRUE(rel_near_result(out.mz_nm, expected_mz, REL_TOL_CLOSED_FORM));

  // The ellipse differs from the cosine method.
  const mf::tire_outputs cosine = eval(parameters, point);
  EXPECT_GT(std::abs(out.fx_n - cosine.fx_n), 1.0);
  EXPECT_GT(std::abs(out.fy_n - cosine.fy_n), 1.0);
}

TEST(pac2002_model, Evaluate_FrictionEllipse_UncombinedIgnoresMethod) {
  const parameters_t parameters = mf::test::fixture_s();
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.05);
  const result_t ellipse = run(parameters, point, slip_mode::UNCOMBINED, combined_method::FRICTION_ELLIPSE);
  const result_t cosine = run(parameters, point, slip_mode::UNCOMBINED, combined_method::COSINE);
  ASSERT_TRUE(ellipse.has_value());
  ASSERT_TRUE(cosine.has_value());
  expect_same_bits(*ellipse, *cosine);
}

// ---- domain and corner cases ------------------------------------------------------------------------------------

TEST(pac2002_model, Evaluate_ZeroVx_AcceptedFinite) {
  const mf::tire_outputs out = eval(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.1, 0.05, 0.02, 0.0));
  EXPECT_TRUE(all_finite(out));
  EXPECT_TRUE(out.flags.below_vxlow);
}

TEST(pac2002_model, Evaluate_NegativeVx_Rejected) {
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.05, 0.0, -1.0);
  const result_t result = run(mf::test::fixture_s(), point, slip_mode::COMBINED, combined_method::COSINE);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
}

TEST(pac2002_model, Evaluate_KappaMinusOneAndHuge_Finite) {
  const parameters_t parameters = mf::test::fixture_s();
  for (const double kappa : {-1.0, 1e6}) {
    SCOPED_TRACE(kappa);
    for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
      EXPECT_TRUE(all_finite(eval(parameters, make_point(FZ_NOMINAL, kappa, 0.05, 0.02), mode)));
    }
  }
}

TEST(pac2002_model, Evaluate_AlphaHalfPi_Mz0NearZero) {
  parameters_t parameters = mf::test::fixture_s();
  parameters.coefficients.QDZ6 = 0.01;
  parameters.coefficients.QBZ9 = 1.0;
  constexpr double half_pi = std::numbers::pi / 2.0;
  for (const double alpha : {half_pi, -half_pi}) {
    SCOPED_TRACE(alpha);
    // The cos(alpha) factors of t and Mzr vanish.
    const mf::tire_outputs pure = eval(parameters, make_point(FZ_NOMINAL, 0.0, alpha), slip_mode::UNCOMBINED);
    EXPECT_NEAR(pure.mz_nm, 0.0, ZERO_TOL);
    EXPECT_TRUE(all_finite(eval(parameters, make_point(FZ_NOMINAL, 0.1, alpha), slip_mode::COMBINED)));
  }
}

TEST(pac2002_model, Evaluate_GammaHalfPi_Finite) {
  parameters_t parameters = mf::test::fixture_s();
  parameters.coefficients.PDX3 = 0.1;
  parameters.coefficients.PDY3 = 0.1;
  parameters.coefficients.PKY3 = 0.2;
  parameters.coefficients.PHY3 = 0.05;
  parameters.coefficients.QSX2 = 0.5;
  parameters.coefficients.QBZ4 = 0.5;
  parameters.coefficients.QDZ3 = 0.2;
  constexpr double half_pi = std::numbers::pi / 2.0;
  for (const double gamma : {half_pi, -half_pi}) {
    SCOPED_TRACE(gamma);
    for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
      EXPECT_TRUE(all_finite(eval(parameters, make_point(FZ_NOMINAL, 0.1, 0.05, gamma), mode)));
    }
  }
}

TEST(pac2002_model, Evaluate_SignedZeroSlips_Identical) {
  const parameters_t parameters = mf::test::fixture_s();
  for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
    const mf::tire_outputs reference = eval(parameters, make_point(FZ_NOMINAL, 0.0, 0.0), mode);
    for (const double kappa : {0.0, -0.0}) {
      for (const double alpha : {0.0, -0.0}) {
        SCOPED_TRACE(std::format("kappa {} alpha {}", kappa, alpha));
        expect_same_bits(eval(parameters, make_point(FZ_NOMINAL, kappa, alpha), mode), reference);
      }
    }
  }
}

TEST(pac2002_model, Evaluate_EpsilonGuards_Finite) {
  // Cx*Dx = 0.
  EXPECT_TRUE(all_finite(eval(s_with({{&coeffs::PCX1, 0.0}}), make_point(FZ_NOMINAL, 0.1, 0.05))));
  // Ky = 0 through LKY.
  EXPECT_TRUE(all_finite(eval(s_with({{&coeffs::LKY, 0.0}}), make_point(FZ_NOMINAL, 0.1, 0.05))));
  // Ky = 0 through PKY3 = 1 and gamma = 1 rad, with a vertical shift that divides by Ky in SHf.
  const parameters_t no_stiffness = s_with({{&coeffs::PKY3, 1.0}, {&coeffs::PVY1, 0.01}});
  for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
    EXPECT_TRUE(all_finite(eval(no_stiffness, make_point(FZ_NOMINAL, 0.1, 0.05, 1.0), mode)));
  }
}

TEST(pac2002_model, Evaluate_HugeFz_ReturnsNumerical) {
  const parameters_t parameters = s_with({{&coeffs::PDX2, 0.1}});
  const point_t point = make_point(1e308, 0.1, 0.05);
  const result_t result = run(parameters, point, slip_mode::COMBINED, combined_method::COSINE);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::NUMERICAL);
  EXPECT_EQ(result.error().where.key, "FX");
}

TEST(pac2002_model, Evaluate_M10_Pac2002MzZero) {
  parameters_t parameters = mf::test::fixture_s();
  parameters.coefficients.PDY3 = 1.0;
  parameters.coefficients.RVY1 = 0.1;
  parameters.coefficients.RVY5 = 1.0;
  parameters.coefficients.RVY6 = 10.0;
  parameters.coefficients.QCZ1 = 0.0;
  const mf::tire_outputs out = eval(parameters, make_point(FZ_NOMINAL, 0.1, 0.0, 0.1));
  // F'y = Fy - SVyk = 0 (reading A), so Mz = 0 even though Fy = SVyk != 0.
  const double svyk_n = (1.0 - (1.0 * 0.1 * 0.1)) * FZ_NOMINAL * 0.1 * std::sin(std::numbers::pi / 4.0);
  EXPECT_TRUE(rel_near_result(out.fy_n, svyk_n, 1e-9));
  EXPECT_NEAR(out.mz_nm, 0.0, 1e-12);
}
