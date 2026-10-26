#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <gtest/gtest.h>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
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
  using out_idx = mf::pac2002_model::output_index;
  using mf::combined_method;
  using mf::slip_mode;
  using mf::test::bits;
  using mf::test::input_of;
  using mf::test::printed_tol;
  using mf::test::rel_near_result;
  using mf::test::REL_TOL_CLOSED_FORM;

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

  // Creates a model; a creation error fails the test.
  std::optional<mf::pac2002_model> make_model(const parameters_t &parameters, slip_mode mode, combined_method method) {
    std::expected<mf::pac2002_model, mf::error> model =
        mf::pac2002_model::create(parameters, method, mf::model_options{.mode = mode});
    if (!model.has_value()) {
      ADD_FAILURE() << mf::to_string(model.error());
      return std::nullopt;
    }
    return std::move(*model);
  }

  // Creates a model and evaluates one point; a creation error fails the test and yields zero outputs.
  outputs_t run(const parameters_t &parameters, const point_t &point, slip_mode mode, combined_method method) {
    const std::optional<mf::pac2002_model> model = make_model(parameters, mode, method);
    if (!model.has_value()) {
      return outputs_t{};
    }
    return outputs_of(model->evaluate(input_of(point)));
  }

  outputs_t eval(const parameters_t &parameters, const point_t &point, slip_mode mode = slip_mode::COMBINED) {
    return run(parameters, point, mode, combined_method::COSINE);
  }

  outputs_t eval_ellipse(const parameters_t &parameters, const point_t &point) {
    return run(parameters, point, slip_mode::COMBINED, combined_method::FRICTION_ELLIPSE);
  }

  kernels::slip_state<kernels::point_lane> state_of(const parameters_t &parameters, const point_t &point) {
    return mf::test::slip_state_at(parameters.coefficients, point);
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

  bool outputs_differ(const outputs_t &lhs, const outputs_t &rhs) {
    return !std::ranges::equal(values_of(lhs), values_of(rhs), std::ranges::equal_to{}, bits, bits);
  }

  // E-clamp check: member = 1.5 is bit-equal to 1.0, member = 0.999 is not.
  void expect_curvature_clamp(member_ptr member, const point_t &point) {
    const outputs_t clamped = eval(s_with({{member, 1.5}}), point);
    const outputs_t at_one = eval(s_with({{member, 1.0}}), point);
    const outputs_t below = eval(s_with({{member, 0.999}}), point);
    expect_same_bits(clamped, at_one);
    EXPECT_TRUE(outputs_differ(below, at_one));
  }

  point_t with(point_t point, double point_t::*member, double value) {
    point.*member = value;
    return point;
  }

  // Parameters of the shift-form My tests: nonzero SVx and SHx, so that the shift form and the QSY form differ.
  parameters_t my_from_shifts_parameters(std::optional<int> fittyp, double qsy1, double qsy2) {
    parameters_t parameters = s_with({{&coeffs::PHX1, 0.001}, {&coeffs::PVX1, 0.01}});
    parameters.coefficients.QSY1 = qsy1;
    parameters.coefficients.QSY2 = qsy2;
    parameters.metadata.fittyp = fittyp;
    return parameters;
  }

  bool selects_my_from_shifts(const parameters_t &parameters) {
    const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(parameters);
    return model.has_value() && model->uses_my_from_shifts();
  }
} // namespace

// ---- construction -----------------------------------------------------------------------------------------------

TEST(pac2002_model, Create_FixtureS_CosineNoMyFromShifts) {
  const parameters_t parameters = mf::test::fixture_s();
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(parameters);
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(model->name(), "PAC2002");
  EXPECT_EQ(model->method(), combined_method::COSINE);
  EXPECT_FALSE(model->uses_my_from_shifts());
  EXPECT_EQ(raw_bits(model->coefficients()), raw_bits(parameters.coefficients));
  EXPECT_EQ(model->metadata().use_mode, 4);
  EXPECT_DOUBLE_EQ(model->limits()(mf::mf5x::VXLOW), 1.0);
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
  parameters.limits(mf::mf5x::FZMIN) = 300.0;
  parameters.limits(mf::mf5x::FZMAX) = 200.0;
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(parameters);
  ASSERT_FALSE(model.has_value());
  EXPECT_EQ(model.error().code, mf::error_code::INVALID_VALUE);
  EXPECT_EQ(model.error().where.key, "FZMIN");
}

TEST(pac2002_model, Create_WithOptions_StoresModeAndFlag) {
  const std::expected<mf::pac2002_model, mf::error> defaults = mf::pac2002_model::create(mf::test::fixture_s());
  ASSERT_TRUE(defaults.has_value());
  EXPECT_EQ(defaults->options().mode, slip_mode::COMBINED);
  EXPECT_FALSE(defaults->options().throw_if_model_failure);

  const mf::model_options options{.mode = slip_mode::UNCOMBINED, .throw_if_model_failure = true};
  const std::expected<mf::pac2002_model, mf::error> custom =
      mf::pac2002_model::create(mf::test::fixture_s(), combined_method::COSINE, options);
  ASSERT_TRUE(custom.has_value());
  EXPECT_EQ(custom->options().mode, slip_mode::UNCOMBINED);
  EXPECT_TRUE(custom->options().throw_if_model_failure);
}

TEST(pac2002_model, Forms_CosineEllipseShifts_Reported) {
  const std::expected<mf::pac2002_model, mf::error> cosine = mf::pac2002_model::create(mf::test::fixture_s());
  ASSERT_TRUE(cosine.has_value());
  EXPECT_EQ(cosine->forms().trail_cosine, mf::trail_cosine_form::COS_ALPHA);
  EXPECT_EQ(cosine->forms().combined, combined_method::COSINE);
  EXPECT_EQ(cosine->forms().fy_prime, mf::fy_prime_form::SVYK);
  EXPECT_EQ(cosine->forms().rolling_resistance, mf::rolling_resistance_form::QSY);

  const std::expected<mf::pac2002_model, mf::error> ellipse =
      mf::pac2002_model::create(mf::test::fixture_s(), combined_method::FRICTION_ELLIPSE);
  ASSERT_TRUE(ellipse.has_value());
  EXPECT_EQ(ellipse->forms().combined, combined_method::FRICTION_ELLIPSE);
  EXPECT_EQ(ellipse->forms().rolling_resistance, mf::rolling_resistance_form::QSY);

  parameters_t shift_parameters = mf::test::fixture_s();
  shift_parameters.metadata.fittyp = 5;
  shift_parameters.coefficients.QSY1 = 0.0;
  const std::expected<mf::pac2002_model, mf::error> shifts = mf::pac2002_model::create(shift_parameters);
  ASSERT_TRUE(shifts.has_value());
  EXPECT_EQ(shifts->forms().rolling_resistance, mf::rolling_resistance_form::SHIFTS);
  EXPECT_EQ(shifts->forms().trail_cosine, mf::trail_cosine_form::COS_ALPHA);
  EXPECT_EQ(shifts->forms().fy_prime, mf::fy_prime_form::SVYK);
}

TEST(pac2002_model, Layout_NameLengthsAndLimitsSize) {
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(mf::test::fixture_s());
  ASSERT_TRUE(model.has_value());
  EXPECT_EQ(&model->layout(), &mf::mf5x::PAC2002_LAYOUT);
  EXPECT_EQ(model->name(), "PAC2002");
  EXPECT_EQ(model->input_length(), 5);
  EXPECT_EQ(model->output_length(), 10);
  EXPECT_EQ(model->limits().size(), 9);
}

TEST(pac2002_model, Evaluate_ThroughBasePointer_MatchesDirectCall) {
  std::expected<mf::pac2002_model, mf::error> created = mf::pac2002_model::create(mf::test::fixture_s());
  ASSERT_TRUE(created.has_value());
  const Eigen::VectorXd input = input_of(make_point(FZ_NOMINAL, 0.1, 0.05, 0.02));
  const outputs_t direct = outputs_of(created->evaluate(input));
  const std::unique_ptr<const mf::tire_model> base = std::make_unique<mf::pac2002_model>(std::move(*created));
  const outputs_t via_base = outputs_of(base->evaluate(input));
  expect_same_bits(via_base, direct);
  EXPECT_EQ(base->name(), "PAC2002");
}

TEST(pac2002_model, EvaluateRaw_BitEqualToRef) {
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(mf::test::fixture_s_rng());
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

TEST(pac2002_model, Outputs_FlagIndicesHoldZeroOrOne) {
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(mf::test::fixture_s_rng());
  ASSERT_TRUE(model.has_value());
  // Every range exceeded and Vx below VXLOW: all five flags are 1.0.
  const Eigen::VectorXd all_set = model->evaluate(input_of(make_point(20000.0, 10.0, 2.0, 0.5, 0.5)));
  EXPECT_EQ(bits(all_set(out_idx::FZ_CLAMPED)), bits(1.0));
  EXPECT_EQ(bits(all_set(out_idx::SR_CLAMPED)), bits(1.0));
  EXPECT_EQ(bits(all_set(out_idx::SA_CLAMPED)), bits(1.0));
  EXPECT_EQ(bits(all_set(out_idx::IA_CLAMPED)), bits(1.0));
  EXPECT_EQ(bits(all_set(out_idx::BELOW_VXLOW)), bits(1.0));
  // Inside every range at speed: all five flags are 0.0.
  const Eigen::VectorXd none_set = model->evaluate(input_of(make_point(FZ_NOMINAL, 0.1, 0.05, 0.02, 16.6)));
  EXPECT_EQ(bits(none_set(out_idx::FZ_CLAMPED)), bits(0.0));
  EXPECT_EQ(bits(none_set(out_idx::SR_CLAMPED)), bits(0.0));
  EXPECT_EQ(bits(none_set(out_idx::SA_CLAMPED)), bits(0.0));
  EXPECT_EQ(bits(none_set(out_idx::IA_CLAMPED)), bits(0.0));
  EXPECT_EQ(bits(none_set(out_idx::BELOW_VXLOW)), bits(0.0));
  // Only the slip ratio exceeds its range.
  const Eigen::VectorXd only_kappa = model->evaluate(input_of(make_point(FZ_NOMINAL, 10.0, 0.05, 0.02, 16.6)));
  EXPECT_EQ(bits(only_kappa(out_idx::SR_CLAMPED)), bits(1.0));
  EXPECT_EQ(bits(only_kappa(out_idx::FZ_CLAMPED)), bits(0.0));
  EXPECT_EQ(bits(only_kappa(out_idx::SA_CLAMPED)), bits(0.0));
  EXPECT_EQ(bits(only_kappa(out_idx::IA_CLAMPED)), bits(0.0));
  EXPECT_EQ(bits(only_kappa(out_idx::BELOW_VXLOW)), bits(0.0));
}

TEST(pac2002_model, CheckedModel_NegativeVx_ThrowsModelFailure) {
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(
      mf::test::fixture_s(), combined_method::COSINE, mf::model_options{.throw_if_model_failure = true});
  ASSERT_TRUE(model.has_value());
  // A valid point passes.
  EXPECT_NO_THROW((void)model->evaluate(input_of(make_point(FZ_NOMINAL, 0.1, 0.05))));
  bool thrown = false;
  try {
    (void)model->evaluate(input_of(make_point(FZ_NOMINAL, 0.1, 0.05, 0.0, -1.0)));
  } catch (const mf::model_failure &failure) {
    thrown = true;
    EXPECT_EQ(failure.cause().code, mf::error_code::INVALID_INPUT);
    EXPECT_EQ(failure.cause().where.key, "VX");
    EXPECT_FALSE(failure.row().has_value());
  }
  EXPECT_TRUE(thrown);
}

TEST(pac2002_model, ValidateInput_NegativeVx_DomainMessage) {
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(mf::test::fixture_s());
  ASSERT_TRUE(model.has_value());
  const std::expected<void, mf::error> result =
      mf::validate_input(*model, input_of(make_point(FZ_NOMINAL, 0.1, 0.05, 0.0, -1.0)));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
  EXPECT_EQ(result.error().message, "VX = -1 m/s is outside the PAC2002 domain (VX >= 0)");
}

// ---- vectors V2-V22 ---------------------------------------------------------------------------------------------

TEST(pac2002_model, Evaluate_V2_CombinedAtZeroAlpha) {
  const outputs_t out = eval(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.1, 0.0));
  EXPECT_NEAR(out.fx_n, 3935.48, printed_tol(2));
  EXPECT_NEAR(out.fy_n, 0.0, ZERO_TOL);
}

TEST(pac2002_model, Evaluate_V3_PureLateralAtZeroKappa) {
  const outputs_t out = eval(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.0, 0.05));
  EXPECT_NEAR(out.fy_n, -2376.40, printed_tol(2));
  EXPECT_NEAR(out.fx_n, 0.0, ZERO_TOL);
}

TEST(pac2002_model, Evaluate_V4_Pvx1) {
  const outputs_t out = eval(s_with({{&coeffs::PVX1, 0.01}}), make_point(FZ_NOMINAL, 0.0, 0.0));
  EXPECT_TRUE(rel_near_result(out.fx_n, 40.0, REL_TOL_CLOSED_FORM));
}

TEST(pac2002_model, Evaluate_V5_RollingResistance) {
  const outputs_t out = eval(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.1, 0.05));
  EXPECT_TRUE(rel_near_result(out.my_nm, -12.0, REL_TOL_CLOSED_FORM));
}

TEST(pac2002_model, Evaluate_V6_OddSymmetry) {
  const parameters_t parameters = mf::test::fixture_s();
  for (const double slip : {0.001, 0.02, 0.1, 0.5, 1.0}) {
    SCOPED_TRACE(slip);
    const outputs_t positive = eval(parameters, make_point(FZ_NOMINAL, slip, 0.0));
    const outputs_t negative = eval(parameters, make_point(FZ_NOMINAL, -slip, 0.0));
    EXPECT_GT(positive.fx_n, 0.0);
    EXPECT_TRUE(rel_near_result(negative.fx_n, -positive.fx_n, REL_TOL_CLOSED_FORM));
    for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
      const outputs_t left = eval(parameters, make_point(FZ_NOMINAL, 0.0, slip), mode);
      const outputs_t right = eval(parameters, make_point(FZ_NOMINAL, 0.0, -slip), mode);
      EXPECT_LT(left.fy_n, 0.0);
      EXPECT_TRUE(rel_near_result(right.fy_n, -left.fy_n, REL_TOL_CLOSED_FORM));
      EXPECT_TRUE(rel_near_result(right.mz_nm, -left.mz_nm, REL_TOL_CLOSED_FORM));
    }
  }
}

TEST(pac2002_model, Evaluate_V8_Lmux05) {
  const outputs_t out = eval(s_with({{&coeffs::LMUX, 0.5}}), make_point(FZ_NOMINAL, 0.1, 0.0));
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
  const outputs_t out = eval(parameters, make_point(FZ_NOMINAL, 0.0, 0.0, 0.05));
  EXPECT_NEAR(out.fy_n, 0.0, ZERO_TOL);
  EXPECT_TRUE(rel_near_result(out.mx_nm, -18.0, REL_TOL_CLOSED_FORM));
}

TEST(pac2002_model, Evaluate_V10_MyFromShifts) {
  parameters_t parameters = s_with({{&coeffs::QSY1, 0.0}, {&coeffs::PHX1, 0.001}, {&coeffs::PVX1, 0.01}});
  parameters.metadata.fittyp = 5;
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(parameters);
  ASSERT_TRUE(model.has_value());
  EXPECT_TRUE(model->uses_my_from_shifts());
  const outputs_t out = outputs_of(model->evaluate(input_of(make_point(FZ_NOMINAL, 0.0, 0.0))));
  EXPECT_TRUE(rel_near_result(out.my_nm, -36.0, REL_TOL_CLOSED_FORM));
}

TEST(pac2002_model, Evaluate_MyFromShiftsCombinations) {
  const point_t point = make_point(FZ_NOMINAL, 0.0, 0.0);

  // No FITTYP: the QSY form with QSY1 = QSY2 = 0 gives zero.
  const parameters_t no_fittyp = my_from_shifts_parameters(std::nullopt, 0.0, 0.0);
  EXPECT_FALSE(selects_my_from_shifts(no_fittyp));
  EXPECT_NEAR(eval(no_fittyp, point).my_nm, 0.0, 1e-12);

  // FITTYP 6: the QSY form.
  const parameters_t fittyp6 = my_from_shifts_parameters(6, 0.0, 0.0);
  EXPECT_FALSE(selects_my_from_shifts(fittyp6));
  EXPECT_NEAR(eval(fittyp6, point).my_nm, 0.0, 1e-12);

  // FITTYP 5 with QSY1 != 0: the QSY form.
  const parameters_t qsy1_set = my_from_shifts_parameters(5, 0.01, 0.0);
  EXPECT_FALSE(selects_my_from_shifts(qsy1_set));
  EXPECT_TRUE(rel_near_result(eval(qsy1_set, point).my_nm, -12.0, REL_TOL_CLOSED_FORM));

  // FITTYP 5 with QSY1 = 0 and QSY2 != 0: the QSY form, My = -R0*Fz*QSY2*Fx/FNOMIN = -0.03*Fx.
  const parameters_t qsy2_set = my_from_shifts_parameters(5, 0.0, 0.1);
  EXPECT_FALSE(selects_my_from_shifts(qsy2_set));
  const outputs_t qsy2_out = eval(qsy2_set, point);
  EXPECT_GT(qsy2_out.fx_n, 1.0);
  EXPECT_TRUE(rel_near_result(qsy2_out.my_nm, -0.03 * qsy2_out.fx_n, REL_TOL_CLOSED_FORM));
  EXPECT_GT(std::abs(qsy2_out.my_nm - (-36.0)), 1.0);

  // FITTYP 5 with QSY1 = QSY2 = 0: the shift form.
  const parameters_t both_zero = my_from_shifts_parameters(5, 0.0, 0.0);
  EXPECT_TRUE(selects_my_from_shifts(both_zero));
  EXPECT_TRUE(rel_near_result(eval(both_zero, point).my_nm, -36.0, REL_TOL_CLOSED_FORM));
}

TEST(pac2002_model, Evaluate_V11_UncombinedTrail) {
  const outputs_t out = eval(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.0, 0.05), slip_mode::UNCOMBINED);
  EXPECT_NEAR(-out.mz_nm / out.fy_n, 0.0254434, 5e-8);
  EXPECT_NEAR(out.mz_nm, 60.46, printed_tol(2));
}

TEST(pac2002_model, Evaluate_V12_CombinedFx) {
  const outputs_t out = eval(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.1, 0.1));
  EXPECT_NEAR(out.fx_n, 2782.80, printed_tol(2));
}

TEST(pac2002_model, Evaluate_V13_CombinedFy) {
  const outputs_t out = eval(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.1, 0.05));
  EXPECT_NEAR(out.fy_n, -1680.37, printed_tol(2));
}

TEST(pac2002_model, Evaluate_V14_KappaInducedSideForce) {
  parameters_t parameters = mf::test::fixture_s();
  parameters.coefficients.RBY1 = 0.0;
  parameters.coefficients.RVY1 = 0.1;
  parameters.coefficients.RVY5 = 1.0;
  parameters.coefficients.RVY6 = 10.0;
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.05);
  const outputs_t out = eval(parameters, point);
  // Gyk = 1 (RBY1 = 0), so Fy = Fy0 + SVyk with SVyk = 400*sin(atan(1)).
  const double svyk_n = 400.0 * std::sin(std::numbers::pi / 4.0);
  EXPECT_TRUE(rel_near_result(out.fy_n, state_of(parameters, point).lat.fy0_n(0) + svyk_n, REL_TOL_CLOSED_FORM));
  // The printed -2093.56 is the sum of two rounded terms, hence the tolerance of one unit of the last digit.
  EXPECT_NEAR(out.fy_n, -2093.56, 2.0 * printed_tol(2));
}

TEST(pac2002_model, Evaluate_V15_ExClamp) {
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.0);
  const outputs_t clamped = eval(s_with({{&coeffs::PEX1, 1.5}}), point);
  const outputs_t at_one = eval(s_with({{&coeffs::PEX1, 1.0}}), point);
  const outputs_t below = eval(s_with({{&coeffs::PEX1, 0.999}}), point);
  // V15: bit-equal to PEX1 = 1.0.
  expect_same_bits(clamped, at_one);
  EXPECT_NEAR(clamped.fx_n, 3603.08, printed_tol(2));
  EXPECT_NE(bits(below.fx_n), bits(at_one.fx_n));
}

TEST(pac2002_model, Evaluate_EClamps_EyEtExaEyk) {
  // Ey: pure lateral force; Et: aligning trail; Exa: Gxa in Fx; Eyk: Gyk in Fy. 1.5 is bit-equal to 1.0.
  expect_curvature_clamp(&coeffs::PEY1, make_point(FZ_NOMINAL, 0.0, 0.05));
  expect_curvature_clamp(&coeffs::QEZ1, make_point(FZ_NOMINAL, 0.0, 0.05));
  expect_curvature_clamp(&coeffs::REX1, make_point(FZ_NOMINAL, 0.1, 0.1));
  expect_curvature_clamp(&coeffs::REY1, make_point(FZ_NOMINAL, 0.1, 0.05));
}

TEST(pac2002_model, Evaluate_V16_GammaX) {
  const parameters_t parameters = s_with({{&coeffs::PDX3, 10.0}, {&coeffs::LGAX, 0.5}});
  const outputs_t out = eval(parameters, make_point(FZ_NOMINAL, 0.1, 0.0, 0.1));
  // mu_x = 1 - PDX3*(gamma*LGAX)^2 = 0.975, so Dx = 3900.
  constexpr double dx = 3900.0;
  const double bx = 80000.0 / ((1.5 * dx) + 1e-6);
  const double expected = dx * std::sin(1.5 * std::atan(bx * 0.1));
  EXPECT_TRUE(rel_near_result(out.fx_n, expected, REL_TOL_CLOSED_FORM));
}

TEST(pac2002_model, Evaluate_V17_FyPrimeAtActualCamber) {
  const parameters_t parameters = s_with({{&coeffs::PHY3, 0.1}});
  for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
    const outputs_t out = eval(parameters, make_point(FZ_NOMINAL, 0.0, 0.0, 0.1), mode);
    EXPECT_NEAR(out.fy_n, -550.02, printed_tol(2));
    EXPECT_NEAR(out.mz_nm, 16.50, printed_tol(2));
  }
}

TEST(pac2002_model, Evaluate_V18_Lfzo12) {
  const parameters_t parameters = s_with({{&coeffs::LFZO, 1.2}});
  const point_t point = make_point(FZ_NOMINAL, 0.0, 0.05);
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(parameters);
  ASSERT_TRUE(model.has_value());
  const kernels::slip_state<kernels::point_lane> state = mf::test::slip_state_at(model->coefficients(), point);
  EXPECT_TRUE(rel_near_result(state.aln.dt_m(0), 0.03, REL_TOL_CLOSED_FORM));
  EXPECT_NEAR(state.lat.ky0_n(0), -61132.08, printed_tol(2));
  const double cos_alpha = std::cos(point.alpha_rad);
  const double trail_m = kernels::t(state.aln, state.aln.alpha_t, mf::test::lane(cos_alpha))(0);
  const double expected_mz =
      -(trail_m * state.lat.fy0_n(0)) + kernels::mzr(state.aln, state.aln.alpha_r, mf::test::lane(cos_alpha))(0);
  const outputs_t pure = run(parameters, point, slip_mode::UNCOMBINED, combined_method::COSINE);
  EXPECT_TRUE(rel_near_result(pure.fy_n, state.lat.fy0_n(0), REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(pure.mz_nm, expected_mz, REL_TOL_CLOSED_FORM));
  const outputs_t combined = outputs_of(model->evaluate(input_of(point)));
  EXPECT_TRUE(rel_near_result(combined.mz_nm, expected_mz, REL_TOL_CLOSED_FORM));
}

TEST(pac2002_model, Evaluate_V19_P6_BeyondLimits_EqualsAtLimit) {
  const parameters_t parameters = mf::test::fixture_s_rng();
  const point_t base = make_point(FZ_NOMINAL, 0.1, 0.05, 0.02);
  const auto check = [&parameters](const point_t &beyond, const point_t &at_limit, double outputs_t::*flag) {
    const outputs_t clamped = eval(parameters, beyond);
    const outputs_t reference = eval(parameters, at_limit);
    // V19 / P6: bit-equal to the evaluation at the limit; the exact limit itself is not flagged.
    expect_same_bits(clamped, reference);
    EXPECT_EQ(bits(clamped.*flag), bits(1.0));
    EXPECT_EQ(bits(reference.*flag), bits(0.0));
  };
  check(with(base, &point_t::kappa, 10.0), with(base, &point_t::kappa, 1.0), &outputs_t::kappa_clamped);
  check(with(base, &point_t::alpha_rad, 2.0), with(base, &point_t::alpha_rad, 1.5), &outputs_t::alpha_clamped);
  check(with(base, &point_t::gamma_rad, 0.5), with(base, &point_t::gamma_rad, 0.2), &outputs_t::gamma_clamped);
  check(with(base, &point_t::fz_n, 20000.0), with(base, &point_t::fz_n, 10000.0), &outputs_t::fz_clamped);
  check(with(base, &point_t::fz_n, 1e-9), with(base, &point_t::fz_n, 200.0), &outputs_t::fz_clamped);
}

TEST(pac2002_model, Evaluate_V20_TinyFz_Finite) {
  const parameters_t parameters = mf::test::fixture_s();
  for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
    EXPECT_TRUE(all_finite(eval(parameters, make_point(1e-9, 0.1, 0.05, 0.02), mode)));
    const outputs_t pure_slip = eval(parameters, make_point(1e-9, 0.1, 0.0), mode);
    EXPECT_TRUE(all_finite(pure_slip));
    EXPECT_LE(std::abs(pure_slip.fx_n), 1e-9);
  }
}

TEST(pac2002_model, Evaluate_FzTiny1em300_Finite) {
  const parameters_t parameters = mf::test::fixture_s();
  for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
    const outputs_t out = eval(parameters, make_point(1e-300, 0.1, 0.05, 0.02), mode);
    EXPECT_TRUE(all_finite(out));
    EXPECT_LE(std::abs(out.fx_n), 1e-9);
  }
}

TEST(pac2002_model, Evaluate_Fz1e300_Finite) {
  // Both modes stay finite at Fz = 1e300; My = -R0 * Fz * QSY1 = -0.3 * 1e300 * 0.01 (fixture S, QSY2..4 = 0).
  for (const slip_mode mode : {slip_mode::COMBINED, slip_mode::UNCOMBINED}) {
    const outputs_t out = eval(mf::test::fixture_s(), make_point(1e300, 0.1, 0.05, 0.02), mode);
    EXPECT_TRUE(all_finite(out));
    EXPECT_GT(out.fx_n, 1e299);
    EXPECT_TRUE(rel_near_result(out.my_nm, -0.3 * 1e300 * 0.01, REL_TOL_CLOSED_FORM));
  }
}

TEST(pac2002_model, Evaluate_V21_SpeedTerms) {
  const parameters_t parameters = s_with({{&coeffs::QSY3, 0.1}, {&coeffs::QSY4, 0.01}});
  const outputs_t out = eval(parameters, make_point(FZ_NOMINAL, 0.1, 0.05));
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
      // Bit-equal to +0.0.
      const outputs_t out = eval(parameters, make_point(fz, 0.1, 0.05, 0.02), mode);
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
  const outputs_t combined = eval(parameters, point, slip_mode::COMBINED);
  const outputs_t pure = eval(parameters, point, slip_mode::UNCOMBINED);
  const kernels::slip_state<kernels::point_lane> state = state_of(parameters, point);
  const double arm_m = kernels::s(parameters.coefficients, state.load, mf::test::lane(combined.fy_n),
                                  mf::test::lane(point.gamma_rad))(0);
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
  const outputs_t out = eval(parameters, point, slip_mode::UNCOMBINED);
  const kernels::slip_state<kernels::point_lane> state = state_of(parameters, point);
  // P15: bit-equal to the pure-slip kernels.
  EXPECT_EQ(bits(out.fx_n), bits(state.lon.fx0_n(0)));
  EXPECT_EQ(bits(out.fy_n), bits(state.lat.fy0_n(0)));
}

TEST(pac2002_model, Evaluate_Uncombined_MxMyFromPureForces) {
  parameters_t parameters = mf::test::fixture_s();
  parameters.coefficients.QSX1 = 0.01;
  parameters.coefficients.QSX3 = 0.1;
  parameters.coefficients.QSY2 = 0.1;
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.05, 0.02);
  const kernels::slip_state<kernels::point_lane> state = state_of(parameters, point);
  const coeffs &c = parameters.coefficients;
  const double expected_mx =
      kernels::mx(c, mf::test::lane(point.fz_n), state.lat.fy0_n, mf::test::lane(point.gamma_rad))(0);
  const double expected_my =
      kernels::my(c, mf::test::lane(point.fz_n), state.lon.fx0_n, mf::test::lane(point.vx_mps))(0);
  const outputs_t pure = eval(parameters, point, slip_mode::UNCOMBINED);
  EXPECT_TRUE(rel_near_result(pure.mx_nm, expected_mx, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(pure.my_nm, expected_my, REL_TOL_CLOSED_FORM));
  // The combined forces differ from the pure ones, so Mx and My differ too.
  const outputs_t combined = eval(parameters, point, slip_mode::COMBINED);
  EXPECT_GT(std::abs(combined.mx_nm - pure.mx_nm), 1e-3);
  EXPECT_GT(std::abs(combined.my_nm - pure.my_nm), 1e-3);
}

TEST(pac2002_model, Evaluate_LmuyHalf_HalvesDyDoublesBy) {
  const point_t point = make_point(FZ_NOMINAL, 0.0, 0.05);
  const std::expected<mf::pac2002_model, mf::error> base = mf::pac2002_model::create(mf::test::fixture_s());
  const std::expected<mf::pac2002_model, mf::error> half = mf::pac2002_model::create(
      s_with({{&coeffs::LMUY, 0.5}}), combined_method::COSINE, mf::model_options{.mode = slip_mode::UNCOMBINED});
  ASSERT_TRUE(base.has_value());
  ASSERT_TRUE(half.has_value());
  const kernels::slip_state<kernels::point_lane> base_state = mf::test::slip_state_at(base->coefficients(), point);
  const kernels::slip_state<kernels::point_lane> half_state = mf::test::slip_state_at(half->coefficients(), point);
  EXPECT_TRUE(rel_near_result(half_state.lat.dy_n(0), 0.5 * base_state.lat.dy_n(0), REL_TOL_CLOSED_FORM));
  // By = Ky/(Cy*Dy + eps): the eps term perturbs the exact doubling at the 1e-9 level.
  EXPECT_TRUE(rel_near_result(half_state.lat.by(0), 2.0 * base_state.lat.by(0), 1e-8));
  const outputs_t half_out = outputs_of(half->evaluate(input_of(point)));
  EXPECT_EQ(bits(half_out.fy_n), bits(half_state.lat.fy0_n(0)));
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

  const outputs_t out = eval_ellipse(parameters, point);
  const kernels::slip_state<kernels::point_lane> state = state_of(parameters, point);
  const kernels::ellipse_ratios<kernels::point_lane> ratios =
      kernels::friction_ellipse_ratios(state, mf::test::lane(point.kappa), mf::test::lane(point.alpha_rad));
  const double expected_fx = state.lon.fx0_n(0) * ratios.rx(0);
  const double expected_fy = state.lat.fy0_n(0) * ratios.ry(0);
  EXPECT_TRUE(rel_near_result(out.fx_n, expected_fx, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(out.fy_n, expected_fy, REL_TOL_CLOSED_FORM));

  // Mz with F'y = Fy (SVyk = 0).
  const coeffs &c = parameters.coefficients;
  const double cos_alpha = std::cos(point.alpha_rad);
  const kernels::point_lane ky_guarded_n = kernels::add_epsilon(state.lat.ky_n);
  const kernels::point_lane kappa_lane = mf::test::lane(point.kappa);
  const kernels::point_lane cos_alpha_lane = mf::test::lane(cos_alpha);
  const kernels::point_lane ate = kernels::alpha_eq(state.aln.alpha_t, kappa_lane, state.lon.kx_n, ky_guarded_n);
  const kernels::point_lane are = kernels::alpha_eq(state.aln.alpha_r, kappa_lane, state.lon.kx_n, ky_guarded_n);
  const double arm_m = kernels::s(c, state.load, mf::test::lane(expected_fy), mf::test::lane(point.gamma_rad))(0);
  const double trail_m = kernels::t(state.aln, ate, cos_alpha_lane)(0);
  const double expected_mz =
      -(trail_m * expected_fy) + kernels::mzr(state.aln, are, cos_alpha_lane)(0) + (arm_m * expected_fx);
  EXPECT_TRUE(rel_near_result(out.mz_nm, expected_mz, REL_TOL_CLOSED_FORM));

  // The ellipse differs from the cosine method.
  const outputs_t cosine = eval(parameters, point);
  EXPECT_GT(std::abs(out.fx_n - cosine.fx_n), 1.0);
  EXPECT_GT(std::abs(out.fy_n - cosine.fy_n), 1.0);
}

TEST(pac2002_model, Evaluate_FrictionEllipse_UncombinedIgnoresMethod) {
  const parameters_t parameters = mf::test::fixture_s();
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.05);
  const outputs_t ellipse = run(parameters, point, slip_mode::UNCOMBINED, combined_method::FRICTION_ELLIPSE);
  const outputs_t cosine = run(parameters, point, slip_mode::UNCOMBINED, combined_method::COSINE);
  expect_same_bits(ellipse, cosine);
}

// ---- domain and corner cases ------------------------------------------------------------------------------------

TEST(pac2002_model, Evaluate_ZeroVx_AcceptedFinite) {
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.05, 0.02, 0.0);
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(mf::test::fixture_s());
  ASSERT_TRUE(model.has_value());
  EXPECT_TRUE(mf::validate_input(*model, input_of(point)).has_value());
  const outputs_t out = outputs_of(model->evaluate(input_of(point)));
  EXPECT_TRUE(all_finite(out));
  EXPECT_EQ(bits(out.below_vxlow), bits(1.0));
}

TEST(pac2002_model, Evaluate_NegativeVx_Rejected) {
  const point_t point = make_point(FZ_NOMINAL, 0.1, 0.05, 0.0, -1.0);
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(mf::test::fixture_s());
  ASSERT_TRUE(model.has_value());
  const std::expected<void, mf::error> result = mf::validate_input(*model, input_of(point));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
}

TEST(pac2002_model, Evaluate_NanInput_RejectedByValidateInput) {
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(mf::test::fixture_s());
  ASSERT_TRUE(model.has_value());
  const point_t point = make_point(FZ_NOMINAL, std::numeric_limits<double>::quiet_NaN(), 0.05);
  const std::expected<void, mf::error> result = mf::validate_input(*model, input_of(point));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "SR");
  EXPECT_NE(result.error().message.find("SR is not finite"), std::string::npos);
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
    const outputs_t pure = eval(parameters, make_point(FZ_NOMINAL, 0.0, alpha), slip_mode::UNCOMBINED);
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
    const outputs_t reference = eval(parameters, make_point(FZ_NOMINAL, 0.0, 0.0), mode);
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
  const std::expected<mf::pac2002_model, mf::error> model = mf::pac2002_model::create(parameters);
  ASSERT_TRUE(model.has_value());
  const Eigen::VectorXd output = model->evaluate(input_of(make_point(1e308, 0.1, 0.05)));
  const std::expected<void, mf::error> result = mf::check_output(*model, output);
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
  const outputs_t out = eval(parameters, make_point(FZ_NOMINAL, 0.1, 0.0, 0.1));
  // F'y = Fy - SVyk = 0, so Mz = 0 even though Fy = SVyk != 0.
  const double svyk_n = (1.0 - (1.0 * 0.1 * 0.1)) * FZ_NOMINAL * 0.1 * std::sin(std::numbers::pi / 4.0);
  EXPECT_TRUE(rel_near_result(out.fy_n, svyk_n, 1e-9));
  EXPECT_NEAR(out.mz_nm, 0.0, 1e-12);
}

// ---- characteristics --------------------------------------------------------------------------------------------
// Closed-form oracles are plain-double functions of this file; they never call the kernels or the model.
// Reference numbers: docs/research/test-data.md.

namespace {
  using char_idx = mf::pac2002_model::characteristic_index;

  // Relative tolerance of the stationary slips (root conditioning) and of the printed reference numbers.
  constexpr double CHAR_PEAK_TOL = 1e-10;
  constexpr double CHAR_TABLE_TOL = 1e-10;
  // Relative tolerance of the printed reference numbers that carry 10 significant digits.
  constexpr double CHAR_SHORT_TABLE_TOL = 1e-9;
  // Scales of near_scaled for the batch-versus-point comparison: slopes in N and N/rad, peak slips in rad and 1.
  constexpr double CHAR_SLOPE_SCALE = 1e4;
  constexpr double CHAR_PEAK_SCALE = 0.1;
  constexpr double CHAR_FX_STIFFNESS_N = 80000.0;
  constexpr double CHAR_NAN = std::numeric_limits<double>::quiet_NaN();

  // D*sin(C*atan(B*x - E*(B*x - atan(B*x)))) of fixture S at Fz 4000 and its derivative in x.
  struct char_curve {
    double b;
    double c;
    double d;
    double e;
  };

  double char_force(const char_curve &k, double x) {
    const double bx = k.b * x;
    const double phi = bx - (k.e * (bx - std::atan(bx)));
    return k.d * std::sin(k.c * std::atan(phi));
  }

  double char_slope(const char_curve &k, double x) {
    const double bx = k.b * x;
    const double phi = bx - (k.e * (bx - std::atan(bx)));
    const double dphi = k.b - (k.e * (k.b - (k.b / (1.0 + (bx * bx)))));
    return k.d * k.c * std::cos(k.c * std::atan(phi)) * dphi / (1.0 + (phi * phi));
  }

  // Ky0 = PKY1*Fz0*sin(2*atan(Fz/(PKY2*Fz0))) of fixture S.
  double char_ky0() {
    return -15.0 * FZ_NOMINAL * std::sin(2.0 * std::atan(1.0 / 1.5));
  }

  // Bx = Kx/(Cx*Dx + eps), By = Ky0/(Cy*Dy + eps).
  char_curve char_curve_x(double cx = 1.5) {
    const double bx = CHAR_FX_STIFFNESS_N / ((cx * FZ_NOMINAL) + kernels::EPSILON);
    return char_curve{.b = bx, .c = cx, .d = FZ_NOMINAL, .e = 0.0};
  }

  char_curve char_curve_y(double cy = 1.3) {
    return char_curve{.b = char_ky0() / ((cy * FZ_NOMINAL) + kernels::EPSILON), .c = cy, .d = FZ_NOMINAL, .e = 0.0};
  }

  // Combined-slip weights of fixture S.
  double char_gxa(double alpha) {
    return 1.0 / std::sqrt(1.0 + (100.0 * alpha * alpha));
  }

  double char_gyk(double kappa) {
    return 1.0 / std::sqrt(1.0 + (100.0 * kappa * kappa));
  }

  // The first stationary slips of fixture S: x = sqrt(3)/Bx (C = 1.5, E = 0) and x = tan(pi/(2*Cy))/|By|.
  double char_sr_peak() {
    return std::numbers::sqrt3 / char_curve_x().b;
  }

  double char_sa_peak() {
    return std::tan(std::numbers::pi / 2.6) / std::abs(char_curve_y().b);
  }

  // Friction-ellipse reference of fixture S_ell (all shifts 0), Fz-free magnitudes.
  struct char_ellipse {
    double fx = 0.0;
    double fy = 0.0;
    double dfx = 0.0;
    double dfy = 0.0;
  };

  char_ellipse char_ellipse_at(double kappa, double alpha) {
    const char_curve cx = char_curve_x();
    const char_curve cy = char_curve_y();
    const double g = char_force(cx, kappa);
    const double dg = char_slope(cx, kappa);
    const double h = char_force(cy, alpha);
    const double dh = char_slope(cy, alpha);
    const double mxm = FZ_NOMINAL;
    const double mym = FZ_NOMINAL;
    const double a_star = std::sin(alpha);
    const double sgn_kc = kappa >= 0.0 ? 1.0 : -1.0;
    const double sgn_a = a_star >= 0.0 ? 1.0 : -1.0;
    const double rx_root = std::sqrt((kappa * kappa * mym * mym) + (g * g * a_star * a_star));
    const double ry_root = std::sqrt((kappa * kappa * h * h) + (a_star * a_star * mxm * mxm));
    const double rx = std::abs(kappa) * mym / rx_root;
    const double ry = std::abs(a_star) * mxm / ry_root;
    const double drx = mym * a_star * a_star * ((sgn_kc * g * g) - (std::abs(kappa) * g * dg)) / std::pow(rx_root, 3);
    const double dry =
        mxm * kappa * kappa * ((sgn_a * std::cos(alpha) * h * h) - (std::abs(a_star) * h * dh)) / std::pow(ry_root, 3);
    return char_ellipse{.fx = g * rx, .fy = h * ry, .dfx = (dg * rx) + (g * drx), .dfy = (dh * ry) + (h * dry)};
  }

  // The characteristics of one point of a model made from `parameters`.
  Eigen::VectorXd characteristics_of(const parameters_t &parameters, const point_t &point,
                                     slip_mode mode = slip_mode::COMBINED,
                                     combined_method method = combined_method::COSINE) {
    const std::optional<mf::pac2002_model> model = make_model(parameters, mode, method);
    if (!model.has_value()) {
      return Eigen::VectorXd::Constant(mf::mf5x::CHARACTERISTICS_LENGTH, CHAR_NAN);
    }
    return model->evaluate_characteristics(input_of(point));
  }

  // The batch characteristics of `points` of a model made from `parameters`.
  Eigen::MatrixXd characteristics_batch_of(const parameters_t &parameters, const std::vector<point_t> &points,
                                           slip_mode mode = slip_mode::COMBINED,
                                           combined_method method = combined_method::COSINE) {
    const std::optional<mf::pac2002_model> model = make_model(parameters, mode, method);
    if (!model.has_value()) {
      return Eigen::MatrixXd::Constant(static_cast<Eigen::Index>(points.size()), mf::mf5x::CHARACTERISTICS_LENGTH,
                                       CHAR_NAN);
    }
    return model->evaluate_characteristics_batch(mf::test::inputs_of(points));
  }

  void expect_char(const Eigen::VectorXd &values, char_idx which, double expected, double tol, const char *label) {
    EXPECT_TRUE(rel_near_result(values(which), expected, tol)) << label;
  }

  // Bit-equality (documented exception to "no EXPECT_EQ on doubles"), for exact zeros and tangent identities.
  void expect_char_bits(const Eigen::VectorXd &values, char_idx which, double expected, const char *label) {
    EXPECT_EQ(bits(values(which)), bits(expected)) << label;
  }

  void expect_char_nan(const Eigen::VectorXd &values, char_idx which, const char *label) {
    EXPECT_TRUE(std::isnan(values(which))) << label;
  }

  bool is_peak_slip(Eigen::Index j) {
    return j == mf::mf5x::SA_AT_PEAK_FY || j == mf::mf5x::SR_AT_PEAK_FX;
  }

  // A batch value of channel `j` matches the point value (NaN matches NaN).
  ::testing::AssertionResult batch_value_matches(double actual, double expected, Eigen::Index j) {
    if (std::isnan(expected)) {
      if (std::isnan(actual)) {
        return ::testing::AssertionSuccess();
      }
      return ::testing::AssertionFailure() << "actual " << actual << ", expected NaN";
    }
    const double scale = is_peak_slip(j) ? CHAR_PEAK_SCALE : CHAR_SLOPE_SCALE;
    return mf::test::near_scaled(actual, expected, mf::test::BATCH_REL_TOL, scale);
  }

  // The batch result agrees with the point results row by row (NaN equals NaN).
  void expect_batch_matches_point(const parameters_t &parameters, const std::vector<point_t> &points, slip_mode mode,
                                  combined_method method) {
    const Eigen::MatrixXd batch = characteristics_batch_of(parameters, points, mode, method);
    ASSERT_EQ(batch.rows(), static_cast<Eigen::Index>(points.size()));
    ASSERT_EQ(batch.cols(), mf::mf5x::CHARACTERISTICS_LENGTH);
    for (std::size_t r = 0; r < points.size(); ++r) {
      const Eigen::VectorXd expected = characteristics_of(parameters, points[r], mode, method);
      const auto row = static_cast<Eigen::Index>(r);
      for (Eigen::Index j = 0; j < expected.size(); ++j) {
        SCOPED_TRACE(std::format("row {} characteristic {}", r, j));
        EXPECT_TRUE(batch_value_matches(batch(row, j), expected(j), j));
      }
    }
  }

  // The operating points of the characteristics vectors C1-C5, C13.
  std::vector<point_t> characteristic_points() {
    std::vector<point_t> points;
    points.push_back(make_point(FZ_NOMINAL, 0.1, 0.0));
    points.push_back(make_point(FZ_NOMINAL, 0.0, 0.05));
    points.push_back(make_point(FZ_NOMINAL, 0.1, 0.1));
    points.push_back(make_point(FZ_NOMINAL, 0.0, 0.0));
    points.push_back(make_point(FZ_NOMINAL, 0.0, -0.05));
    points.push_back(make_point(FZ_NOMINAL, -0.1, 0.0));
    points.push_back(make_point(0.0, 0.1, 0.05));
    points.push_back(make_point(-100.0, 0.0, 0.0));
    return points;
  }

  // The characteristics rows of `points` of fixture S with the friction ellipse, point form and batch form.
  std::vector<Eigen::VectorXd> ellipse_point_rows(const std::vector<point_t> &points) {
    std::vector<Eigen::VectorXd> rows;
    rows.reserve(points.size());
    for (const point_t &point : points) {
      rows.push_back(
          characteristics_of(mf::test::fixture_s(), point, slip_mode::COMBINED, combined_method::FRICTION_ELLIPSE));
    }
    return rows;
  }

  std::vector<Eigen::VectorXd> ellipse_batch_rows(const std::vector<point_t> &points) {
    const Eigen::MatrixXd batch =
        characteristics_batch_of(mf::test::fixture_s(), points, slip_mode::COMBINED, combined_method::FRICTION_ELLIPSE);
    std::vector<Eigen::VectorXd> rows;
    rows.reserve(points.size());
    for (Eigen::Index r = 0; r < batch.rows(); ++r) {
      rows.emplace_back(batch.row(r).transpose());
    }
    return rows;
  }
} // namespace

TEST(pac2002_model, EvaluateCharacteristics_CVectors_MatchClosedForms) {
  const parameters_t fixture = mf::test::fixture_s();
  const char_curve cx = char_curve_x();
  const char_curve cy = char_curve_y();

  // C1: pure longitudinal slip; the secant at SA = 0 is the tangent.
  const Eigen::VectorXd c1 = characteristics_of(fixture, make_point(FZ_NOMINAL, 0.1, 0.0));
  expect_char(c1, mf::mf5x::DFX_DSR, char_slope(cx, 0.1) * char_gxa(0.0), REL_TOL_CLOSED_FORM, "C1 dfx_dsr");
  expect_char(c1, mf::mf5x::FX_OVER_SR, char_force(cx, 0.1) / 0.1, REL_TOL_CLOSED_FORM, "C1 fx_over_sr");
  expect_char(c1, mf::mf5x::DFY_DSA, char_slope(cy, 0.0) * char_gyk(0.1), REL_TOL_CLOSED_FORM, "C1 dfy_dsa");
  expect_char_bits(c1, mf::mf5x::FY_OVER_SA, c1(mf::mf5x::DFY_DSA), "C1 secant at SA 0 is the tangent");
  expect_char(c1, mf::mf5x::DFY_DSA, 5200.0 * cy.b / std::numbers::sqrt2, REL_TOL_CLOSED_FORM, "C1 5200*By/sqrt(2)");
  expect_char(c1, mf::mf5x::DFY_DSA, -39162.83710434, CHAR_TABLE_TOL, "C1 table dfy_dsa");
  expect_char(c1, mf::mf5x::DFX_DSR, 5151.900623800, CHAR_TABLE_TOL, "C1 table dfx_dsr");
  expect_char(c1, mf::mf5x::FX_OVER_SR, 39354.79640314, CHAR_TABLE_TOL, "C1 table fx_over_sr");
  expect_char(c1, mf::mf5x::SR_AT_PEAK_FX, char_sr_peak(), CHAR_PEAK_TOL, "C1 sr_peak");
  expect_char(c1, mf::mf5x::SA_AT_PEAK_FY, char_sa_peak(), CHAR_PEAK_TOL, "C1 sa_peak");
  expect_char(c1, mf::mf5x::DY, FZ_NOMINAL, REL_TOL_CLOSED_FORM, "C1 dy");
  expect_char(c1, mf::mf5x::DX, FZ_NOMINAL, REL_TOL_CLOSED_FORM, "C1 dx");

  // C2: pure lateral slip; the secant at SR = 0 is the tangent.
  const Eigen::VectorXd c2 = characteristics_of(fixture, make_point(FZ_NOMINAL, 0.0, 0.05));
  expect_char(c2, mf::mf5x::DFY_DSA, char_slope(cy, 0.05) * char_gyk(0.0), REL_TOL_CLOSED_FORM, "C2 dfy_dsa");
  expect_char(c2, mf::mf5x::FY_OVER_SA, char_force(cy, 0.05) / 0.05, REL_TOL_CLOSED_FORM, "C2 fy_over_sa");
  expect_char(c2, mf::mf5x::DFX_DSR, char_slope(cx, 0.0) * char_gxa(0.05), REL_TOL_CLOSED_FORM, "C2 dfx_dsr");
  expect_char_bits(c2, mf::mf5x::FX_OVER_SR, c2(mf::mf5x::DFX_DSR), "C2 secant at SR 0 is the tangent");
  expect_char(c2, mf::mf5x::DFX_DSR, 6000.0 * cx.b / std::sqrt(1.25), REL_TOL_CLOSED_FORM, "C2 6000*Bx/sqrt(1.25)");
  expect_char(c2, mf::mf5x::DFX_DSR, 71554.17526807, CHAR_TABLE_TOL, "C2 table dfx_dsr");
  expect_char(c2, mf::mf5x::DFY_DSA, -34707.69217580, CHAR_TABLE_TOL, "C2 table dfy_dsa");
  expect_char(c2, mf::mf5x::FY_OVER_SA, -47527.96304983, CHAR_TABLE_TOL, "C2 table fy_over_sa");
  expect_char(c2, mf::mf5x::SA_AT_PEAK_FY, char_sa_peak(), CHAR_PEAK_TOL, "C2 sa_peak");
  expect_char(c2, mf::mf5x::SR_AT_PEAK_FX, char_sr_peak(), CHAR_PEAK_TOL, "C2 sr_peak");

  // C3: combined slip, weighted slopes and the same peaks.
  const Eigen::VectorXd c3 = characteristics_of(fixture, make_point(FZ_NOMINAL, 0.1, 0.1));
  expect_char(c3, mf::mf5x::DFX_DSR, char_slope(cx, 0.1) / std::numbers::sqrt2, REL_TOL_CLOSED_FORM, "C3 dfx_dsr");
  expect_char(c3, mf::mf5x::DFY_DSA, char_slope(cy, 0.1) / std::numbers::sqrt2, REL_TOL_CLOSED_FORM, "C3 dfy_dsa");
  expect_char(c3, mf::mf5x::FX_OVER_SR, char_force(cx, 0.1) / std::numbers::sqrt2 / 0.1, REL_TOL_CLOSED_FORM,
              "C3 fx_over_sr");
  expect_char(c3, mf::mf5x::FY_OVER_SA, char_force(cy, 0.1) / std::numbers::sqrt2 / 0.1, REL_TOL_CLOSED_FORM,
              "C3 fy_over_sa");
  expect_char(c3, mf::mf5x::DFX_DSR, 3642.943867088, CHAR_TABLE_TOL, "C3 table dfx_dsr");
  expect_char(c3, mf::mf5x::DFY_DSA, -8938.282121032, CHAR_TABLE_TOL, "C3 table dfy_dsa");
  expect_char(c3, mf::mf5x::SR_AT_PEAK_FX, char_sr_peak(), CHAR_PEAK_TOL, "C3 sr_peak");
  expect_char(c3, mf::mf5x::SA_AT_PEAK_FY, char_sa_peak(), CHAR_PEAK_TOL, "C3 sa_peak");

  // C4: zero slips; both secants are the tangents of the row.
  const Eigen::VectorXd c4 = characteristics_of(fixture, make_point(FZ_NOMINAL, 0.0, 0.0));
  expect_char(c4, mf::mf5x::DFX_DSR, 6000.0 * cx.b, REL_TOL_CLOSED_FORM, "C4 dfx_dsr");
  expect_char(c4, mf::mf5x::DFY_DSA, 5200.0 * cy.b, REL_TOL_CLOSED_FORM, "C4 dfy_dsa");
  expect_char_bits(c4, mf::mf5x::FX_OVER_SR, c4(mf::mf5x::DFX_DSR), "C4 fx_over_sr");
  expect_char_bits(c4, mf::mf5x::FY_OVER_SA, c4(mf::mf5x::DFY_DSA), "C4 fy_over_sa");
  expect_char(c4, mf::mf5x::DFX_DSR, 79999.99998667, CHAR_TABLE_TOL, "C4 table dfx_dsr");
  expect_char(c4, mf::mf5x::DFY_DSA, -55384.61537396, CHAR_TABLE_TOL, "C4 table dfy_dsa");
  expect_char(c4, mf::mf5x::SA_AT_PEAK_FY, char_sa_peak(), CHAR_PEAK_TOL, "C4 sa_peak");
  expect_char(c4, mf::mf5x::SR_AT_PEAK_FX, char_sr_peak(), CHAR_PEAK_TOL, "C4 sr_peak");

  // C5 and C13: negative slips mirror the peaks; the slopes and secants are even.
  const Eigen::VectorXd c5 = characteristics_of(fixture, make_point(FZ_NOMINAL, 0.0, -0.05));
  expect_char(c5, mf::mf5x::SA_AT_PEAK_FY, -char_sa_peak(), CHAR_PEAK_TOL, "C5 sa_peak");
  expect_char(c5, mf::mf5x::DFY_DSA, char_slope(cy, 0.05), REL_TOL_CLOSED_FORM, "C5 dfy_dsa");
  expect_char(c5, mf::mf5x::FY_OVER_SA, char_force(cy, 0.05) / 0.05, REL_TOL_CLOSED_FORM, "C5 fy_over_sa");
  expect_char(c5, mf::mf5x::SR_AT_PEAK_FX, char_sr_peak(), CHAR_PEAK_TOL, "C5 sr_peak");
  const Eigen::VectorXd c13 = characteristics_of(fixture, make_point(FZ_NOMINAL, -0.1, 0.0));
  expect_char(c13, mf::mf5x::SR_AT_PEAK_FX, -char_sr_peak(), CHAR_PEAK_TOL, "C13 sr_peak");
  expect_char(c13, mf::mf5x::DFX_DSR, char_slope(cx, 0.1), REL_TOL_CLOSED_FORM, "C13 dfx_dsr");
  expect_char(c13, mf::mf5x::FX_OVER_SR, char_force(cx, 0.1) / 0.1, REL_TOL_CLOSED_FORM, "C13 fx_over_sr");
  expect_char_bits(c13, mf::mf5x::FY_OVER_SA, c13(mf::mf5x::DFY_DSA), "C13 fy_over_sa");

  // C7: SA = 2 rad is clamped to ALPMAX = 1.5: zero slope, the secant is the clamped force over the raw slip.
  const parameters_t ranged = mf::test::fixture_s_rng();
  const Eigen::VectorXd c7 = characteristics_of(ranged, make_point(FZ_NOMINAL, 0.0, 2.0));
  expect_char_bits(c7, mf::mf5x::DFY_DSA, 0.0, "C7 dfy_dsa");
  expect_char(c7, mf::mf5x::FY_OVER_SA, char_force(cy, 1.5) / 2.0, REL_TOL_CLOSED_FORM, "C7 fy_over_sa");
  expect_char(c7, mf::mf5x::FY_OVER_SA, -1849.837475483, CHAR_TABLE_TOL, "C7 table fy_over_sa");
  expect_char(c7, mf::mf5x::DFX_DSR, 6000.0 * cx.b / std::sqrt(226.0), REL_TOL_CLOSED_FORM, "C7 dfx_dsr");
  expect_char_bits(c7, mf::mf5x::FX_OVER_SR, c7(mf::mf5x::DFX_DSR), "C7 fx_over_sr");
  expect_char(c7, mf::mf5x::DFX_DSR, 5321.520841015, CHAR_TABLE_TOL, "C7 table dfx_dsr");
  expect_char(c7, mf::mf5x::SA_AT_PEAK_FY, char_sa_peak(), CHAR_PEAK_TOL, "C7 sa_peak");
  expect_char(c7, mf::mf5x::SR_AT_PEAK_FX, char_sr_peak(), CHAR_PEAK_TOL, "C7 sr_peak");

  // C8: no contact (Fz 0 and -100, and a range that clamps Fz to 0): exact zeros in the six channels, NaN peaks.
  parameters_t unloaded = mf::test::fixture_s_rng();
  unloaded.limits(mf::mf5x::FZMIN) = -1000.0;
  unloaded.limits(mf::mf5x::FZMAX) = 0.0;
  const std::array<Eigen::VectorXd, 6> no_contact{
      characteristics_of(fixture, make_point(0.0, 0.1, 0.05)),
      characteristics_of(fixture, make_point(0.0, 0.0, 0.0)),
      characteristics_of(fixture, make_point(-100.0, 0.1, 0.05)),
      characteristics_of(fixture, make_point(-100.0, 0.0, 0.0)),
      characteristics_of(unloaded, make_point(100.0, 0.1, 0.05)),
      characteristics_of(unloaded, make_point(100.0, 0.0, 0.0)),
  };
  for (const Eigen::VectorXd &row : no_contact) {
    expect_char_bits(row, mf::mf5x::DFY_DSA, 0.0, "C8 dfy_dsa");
    expect_char_bits(row, mf::mf5x::DFX_DSR, 0.0, "C8 dfx_dsr");
    expect_char_bits(row, mf::mf5x::FY_OVER_SA, 0.0, "C8 fy_over_sa");
    expect_char_bits(row, mf::mf5x::FX_OVER_SR, 0.0, "C8 fx_over_sr");
    expect_char_bits(row, mf::mf5x::DY, 0.0, "C8 dy");
    expect_char_bits(row, mf::mf5x::DX, 0.0, "C8 dx");
    expect_char_nan(row, mf::mf5x::SA_AT_PEAK_FY, "C8 sa_peak");
    expect_char_nan(row, mf::mf5x::SR_AT_PEAK_FX, "C8 sr_peak");
  }

  // C12: the peak lies beyond ALPMAX = 0.1, so the slip-angle peak is NaN and everything else is unchanged.
  parameters_t short_range = mf::test::fixture_s_rng();
  short_range.limits(mf::mf5x::ALPMAX) = 0.1;
  const Eigen::VectorXd c12 = characteristics_of(short_range, make_point(FZ_NOMINAL, 0.0, 0.05));
  expect_char_nan(c12, mf::mf5x::SA_AT_PEAK_FY, "C12 sa_peak");
  expect_char(c12, mf::mf5x::SR_AT_PEAK_FX, char_sr_peak(), CHAR_PEAK_TOL, "C12 sr_peak");
  expect_char(c12, mf::mf5x::DFY_DSA, char_slope(cy, 0.05), REL_TOL_CLOSED_FORM, "C12 dfy_dsa");
}

TEST(pac2002_model, EvaluateCharacteristicsBatch_CVectors_EqualPoint) {
  const std::vector<point_t> points = characteristic_points();
  expect_batch_matches_point(mf::test::fixture_s(), points, slip_mode::COMBINED, combined_method::COSINE);
  expect_batch_matches_point(mf::test::fixture_s(), points, slip_mode::UNCOMBINED, combined_method::COSINE);
  expect_batch_matches_point(mf::test::fixture_s(), points, slip_mode::COMBINED, combined_method::FRICTION_ELLIPSE);
  const std::vector<point_t> clamped_points{
      make_point(FZ_NOMINAL, 0.0, 2.0),
      make_point(FZ_NOMINAL, 5.0, 0.0),
  };
  expect_batch_matches_point(mf::test::fixture_s_rng(), clamped_points, slip_mode::COMBINED, combined_method::COSINE);
}

TEST(pac2002_model, EvaluateCharacteristics_Ellipse_C10ClosedForms) {
  struct ellipse_row {
    double kappa;
    double alpha;
    double sr_peak;
    double sa_peak;
  };
  const std::array<ellipse_row, 2> rows{
      ellipse_row{.kappa = 0.1, .alpha = 0.05, .sr_peak = 0.1675533315318, .sa_peak = 0.4281129082399},
      ellipse_row{.kappa = 0.05, .alpha = 0.1, .sr_peak = 0.2445156130364, .sa_peak = 0.2983217748151},
  };
  for (const ellipse_row &row : rows) {
    const Eigen::VectorXd r = characteristics_of(mf::test::fixture_s(), make_point(FZ_NOMINAL, row.kappa, row.alpha),
                                                 slip_mode::COMBINED, combined_method::FRICTION_ELLIPSE);
    const char_ellipse e = char_ellipse_at(row.kappa, row.alpha);
    expect_char(r, mf::mf5x::DFX_DSR, e.dfx, REL_TOL_CLOSED_FORM, "dfx_dsr");
    expect_char(r, mf::mf5x::DFY_DSA, e.dfy, REL_TOL_CLOSED_FORM, "dfy_dsa");
    expect_char(r, mf::mf5x::FX_OVER_SR, e.fx / row.kappa, REL_TOL_CLOSED_FORM, "fx_over_sr");
    expect_char(r, mf::mf5x::FY_OVER_SA, e.fy / row.alpha, REL_TOL_CLOSED_FORM, "fy_over_sa");
    expect_char(r, mf::mf5x::SR_AT_PEAK_FX, row.sr_peak, CHAR_PEAK_TOL, "table sr_peak");
    expect_char(r, mf::mf5x::SA_AT_PEAK_FY, row.sa_peak, CHAR_PEAK_TOL, "table sa_peak");
    expect_char(r, mf::mf5x::DY, FZ_NOMINAL, REL_TOL_CLOSED_FORM, "dy");
    expect_char(r, mf::mf5x::DX, FZ_NOMINAL, REL_TOL_CLOSED_FORM, "dx");
  }
  const Eigen::VectorXd first = characteristics_of(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.1, 0.05),
                                                   slip_mode::COMBINED, combined_method::FRICTION_ELLIPSE);
  expect_char(first, mf::mf5x::DFX_DSR, 10599.59198473, CHAR_TABLE_TOL, "table dfx_dsr");
  expect_char(first, mf::mf5x::DFY_DSA, -27161.23002030, CHAR_TABLE_TOL, "table dfy_dsa");
}

TEST(pac2002_model, EvaluateCharacteristics_Ellipse_ZeroSlipRowsUseDirectionalLimit) {
  const char_curve cx = char_curve_x();
  const char_curve cy = char_curve_y();
  const std::vector<point_t> points{
      make_point(FZ_NOMINAL, 0.0, 0.05),
      make_point(FZ_NOMINAL, 0.1, 0.0),
      make_point(FZ_NOMINAL, 0.0, 0.0),
  };

  const auto expect_rows = [&](const std::vector<Eigen::VectorXd> &rows, const char *which) {
    SCOPED_TRACE(which);
    ASSERT_EQ(rows.size(), 3U);
    // C14: kappa = 0, alpha = 0.05; the limit of rx along the kappa seed, ry identically 1.
    const double slope_x0 = char_slope(cx, 0.0);
    const double sin_alpha = std::sin(0.05);
    const double slope_sin = slope_x0 * sin_alpha;
    const double rx_limit = FZ_NOMINAL / std::sqrt((FZ_NOMINAL * FZ_NOMINAL) + (slope_sin * slope_sin));
    expect_char(rows[0], mf::mf5x::DFX_DSR, slope_x0 * rx_limit, REL_TOL_CLOSED_FORM, "C14 dfx_dsr");
    expect_char_bits(rows[0], mf::mf5x::FX_OVER_SR, rows[0](mf::mf5x::DFX_DSR), "C14 fx_over_sr is the tangent");
    expect_char(rows[0], mf::mf5x::DFX_DSR, 56580.32736, CHAR_SHORT_TABLE_TOL, "C14 table dfx_dsr");
    expect_char(rows[0], mf::mf5x::DFY_DSA, char_slope(cy, 0.05), REL_TOL_CLOSED_FORM, "C14 dfy_dsa");
    expect_char(rows[0], mf::mf5x::FY_OVER_SA, char_force(cy, 0.05) / 0.05, REL_TOL_CLOSED_FORM, "C14 fy_over_sa");
    // C15: kappa = 0.1, alpha = 0; the limit of ry along the alpha seed, rx identically 1.
    const double slope_y0 = char_slope(cy, 0.0);
    const double ry_limit = FZ_NOMINAL / std::sqrt((FZ_NOMINAL * FZ_NOMINAL) + (0.1 * slope_y0 * 0.1 * slope_y0));
    expect_char(rows[1], mf::mf5x::DFY_DSA, slope_y0 * ry_limit, REL_TOL_CLOSED_FORM, "C15 dfy_dsa");
    expect_char_bits(rows[1], mf::mf5x::FY_OVER_SA, rows[1](mf::mf5x::DFY_DSA), "C15 fy_over_sa is the tangent");
    expect_char(rows[1], mf::mf5x::DFY_DSA, -32427.16913, CHAR_SHORT_TABLE_TOL, "C15 table dfy_dsa");
    expect_char(rows[1], mf::mf5x::DFX_DSR, char_slope(cx, 0.1), REL_TOL_CLOSED_FORM, "C15 dfx_dsr");
    expect_char(rows[1], mf::mf5x::FX_OVER_SR, char_force(cx, 0.1) / 0.1, REL_TOL_CLOSED_FORM, "C15 fx_over_sr");
    // (0, 0): both ratios are 0/0 and the slopes are the pure slopes.
    expect_char(rows[2], mf::mf5x::DFX_DSR, 6000.0 * cx.b, REL_TOL_CLOSED_FORM, "origin dfx_dsr");
    expect_char(rows[2], mf::mf5x::DFY_DSA, 5200.0 * cy.b, REL_TOL_CLOSED_FORM, "origin dfy_dsa");
    expect_char_bits(rows[2], mf::mf5x::FX_OVER_SR, rows[2](mf::mf5x::DFX_DSR), "origin fx_over_sr");
    expect_char_bits(rows[2], mf::mf5x::FY_OVER_SA, rows[2](mf::mf5x::DFY_DSA), "origin fy_over_sa");
  };

  expect_rows(ellipse_point_rows(points), "point");
  expect_rows(ellipse_batch_rows(points), "batch");
}

TEST(pac2002_model, EvaluateCharacteristics_Uncombined_PureCurves) {
  const Eigen::VectorXd r = characteristics_of(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.1, 0.1),
                                               slip_mode::UNCOMBINED, combined_method::COSINE);
  const char_curve cx = char_curve_x();
  const char_curve cy = char_curve_y();
  expect_char(r, mf::mf5x::DFX_DSR, char_slope(cx, 0.1), REL_TOL_CLOSED_FORM, "dfx_dsr");
  expect_char(r, mf::mf5x::DFY_DSA, char_slope(cy, 0.1), REL_TOL_CLOSED_FORM, "dfy_dsa");
  expect_char(r, mf::mf5x::FX_OVER_SR, char_force(cx, 0.1) / 0.1, REL_TOL_CLOSED_FORM, "fx_over_sr");
  expect_char(r, mf::mf5x::FY_OVER_SA, char_force(cy, 0.1) / 0.1, REL_TOL_CLOSED_FORM, "fy_over_sa");
  expect_char(r, mf::mf5x::SR_AT_PEAK_FX, char_sr_peak(), CHAR_PEAK_TOL, "sr_peak");
  expect_char(r, mf::mf5x::SA_AT_PEAK_FY, char_sa_peak(), CHAR_PEAK_TOL, "sa_peak");

  const Eigen::VectorXd origin = characteristics_of(mf::test::fixture_s(), make_point(FZ_NOMINAL, 0.0, 0.0),
                                                    slip_mode::UNCOMBINED, combined_method::COSINE);
  expect_char_bits(origin, mf::mf5x::FY_OVER_SA, origin(mf::mf5x::DFY_DSA), "origin fy_over_sa");
  expect_char_bits(origin, mf::mf5x::FX_OVER_SR, origin(mf::mf5x::DFX_DSR), "origin fx_over_sr");
}

TEST(pac2002_model, EvaluateCharacteristics_CombinedAtZeroKappa_EqualsUncombined) {
  const point_t point = make_point(FZ_NOMINAL, 0.0, 0.05);
  const Eigen::VectorXd combined = characteristics_of(mf::test::fixture_s(), point, slip_mode::COMBINED);
  const Eigen::VectorXd pure = characteristics_of(mf::test::fixture_s(), point, slip_mode::UNCOMBINED);
  expect_char(combined, mf::mf5x::DFY_DSA, pure(mf::mf5x::DFY_DSA), REL_TOL_CLOSED_FORM, "dfy_dsa");
  expect_char(combined, mf::mf5x::SA_AT_PEAK_FY, pure(mf::mf5x::SA_AT_PEAK_FY), CHAR_PEAK_TOL, "sa_peak");
}

TEST(pac2002_model, EvaluateCharacteristics_PartialBlock70Rows_EqualPoint) {
  constexpr Eigen::Index rows = (2 * kernels::BLOCK_ROWS) + 6;
  std::vector<point_t> points;
  for (Eigen::Index r = 0; r < rows; ++r) {
    const auto position = static_cast<std::ptrdiff_t>(static_cast<std::size_t>(r) % mf::test::REGIME_POINTS.size());
    points.push_back(*std::next(mf::test::REGIME_POINTS.begin(), position));
  }
  expect_batch_matches_point(mf::test::fixture_s(), points, slip_mode::COMBINED, combined_method::COSINE);
  expect_batch_matches_point(mf::test::fixture_s(), points, slip_mode::COMBINED, combined_method::FRICTION_ELLIPSE);
}
