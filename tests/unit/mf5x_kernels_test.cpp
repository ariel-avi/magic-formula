#include "../support/fixtures.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <initializer_list>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <numbers>
#include <random>
#include <utility>

namespace {
  namespace kernels = mf::detail;
  using coeffs = mf::mf5x_coefficients;
  using member_ptr = double coeffs::*;
  using mf::test::bits;
  using mf::test::printed_tol;
  using mf::test::rel_near_result;
  using mf::test::REL_TOL_CLOSED_FORM;

  constexpr double FZ_NOMINAL = 4000.0;
  constexpr double SPEED_MPS = 10.0;

  // Fixture S coefficients with the listed members replaced.
  coeffs s_with(std::initializer_list<std::pair<member_ptr, double>> overrides) {
    coeffs c = mf::test::fixture_s().coefficients;
    for (const auto &[member, value] : overrides) {
      c.*member = value;
    }
    return c;
  }

  mf::operating_point make_point(double fz, double kappa, double alpha, double gamma) {
    return mf::operating_point{.fz_n = fz, .kappa = kappa, .alpha_rad = alpha, .gamma_rad = gamma, .vx_mps = SPEED_MPS};
  }

  kernels::longitudinal_terms longitudinal(const coeffs &c, double fz, double kappa, double gamma = 0.0) {
    return kernels::pure_longitudinal(c, kernels::vertical_load(c, fz), fz, kappa, gamma);
  }

  kernels::lateral_terms lateral(const coeffs &c, double fz, double alpha, double gamma = 0.0) {
    return kernels::pure_lateral(c, kernels::vertical_load(c, fz), fz, alpha, gamma);
  }

  kernels::slip_state state_at(const coeffs &c, double fz, double kappa, double alpha, double gamma = 0.0) {
    return kernels::evaluate_slip_state(c, make_point(fz, kappa, alpha, gamma));
  }

  kernels::combined_forces combined_at(const coeffs &c, double fz, double kappa, double alpha) {
    return kernels::cosine_combined_forces(c, state_at(c, fz, kappa, alpha), make_point(fz, kappa, alpha, 0.0));
  }

  // True when the ratio lies in (0, 1] up to rounding.
  bool is_unit_ratio(double ratio) {
    constexpr double slack = 1e-12;
    return (ratio > 0.0) && (ratio <= (1.0 + slack));
  }

  // True when `reduced` has the sign of `pure` and no larger magnitude up to rounding.
  bool is_reduction_of(double reduced, double pure) {
    constexpr double slack = 1e-12;
    const bool same_sign = std::signbit(reduced) == std::signbit(pure);
    return same_sign && (std::abs(reduced) <= (std::abs(pure) * (1.0 + slack)));
  }

  // Coefficients that put non-zero shifts into the pure forces, so the friction-ellipse centre is not the origin.
  coeffs ellipse_coefficients() {
    return s_with({{&coeffs::PHX1, 0.001}, {&coeffs::PVX1, 0.01}, {&coeffs::PHY1, 0.002}, {&coeffs::PVY1, 0.01}});
  }

  // The raw IEEE-754 bit patterns of a term struct (all members are doubles, so there is no padding).
  template <class Terms_T>
  std::array<std::uint64_t, sizeof(Terms_T) / sizeof(std::uint64_t)> raw_bits(const Terms_T &terms) {
    return std::bit_cast<std::array<std::uint64_t, sizeof(Terms_T) / sizeof(std::uint64_t)>>(terms);
  }

  // The printed MSC friction-ellipse form (pac2002.md section 5.4) with |mu_act| and the 1/Fz factors. beta is
  // computed as atan2(|alpha*|, |kappa_c|), which equals acos(|kappa_c|/sqrt(kappa_c^2 + alpha*^2)) without the
  // precision loss of acos near 1.
  kernels::ellipse_ratios printed_form_ratios(const kernels::slip_state &s, double fz, double kappa, double alpha) {
    const auto square = [](double v) { return v * v; };
    const double kappa_c = kappa + s.lon.shx + (s.lon.svx_n / kernels::add_epsilon(s.lon.kx_n));
    const double alpha_c = alpha + s.lat.shy + (s.lat.svy_n / kernels::add_epsilon(s.lat.ky_n));
    const double alpha_star = std::sin(alpha_c);
    const double tan_beta = std::tan(std::atan2(std::abs(alpha_star), std::abs(kappa_c)));
    const double mu_x_act = std::abs(s.lon.fx0_n - s.lon.svx_n) / fz;
    const double mu_y_act = std::abs(s.lat.fy0_n - s.lat.svy_n) / fz;
    const double mu_x_max = std::abs(s.lon.dx_n) / fz;
    const double mu_y_max = std::abs(s.lat.dy_n) / fz;
    const double mu_x = 1.0 / std::sqrt(square(1.0 / mu_x_act) + square(tan_beta / mu_y_max));
    const double mu_y = tan_beta / std::sqrt(square(1.0 / mu_x_max) + square(tan_beta / mu_y_act));
    return kernels::ellipse_ratios{.rx = mu_x / mu_x_act, .ry = mu_y / mu_y_act};
  }
} // namespace

// ---- sgn+, epsilon, curvature limit, Magic Formula shapes -------------------------------------------------------

TEST(mf5x_kernels, SgnPlus_SignedZeros_ReturnPlusOne) {
  EXPECT_DOUBLE_EQ(kernels::sgn_plus(0.0), 1.0);
  EXPECT_DOUBLE_EQ(kernels::sgn_plus(-0.0), 1.0);
}

TEST(mf5x_kernels, SgnPlus_TinyNegative_ReturnsMinusOne) {
  EXPECT_DOUBLE_EQ(kernels::sgn_plus(-1e-300), -1.0);
}

TEST(mf5x_kernels, SgnPlus_Positive_ReturnsPlusOne) {
  EXPECT_DOUBLE_EQ(kernels::sgn_plus(1e-300), 1.0);
  EXPECT_DOUBLE_EQ(kernels::sgn_plus(3.0), 1.0);
}

TEST(mf5x_kernels, AddEpsilon_Positive_AddsEpsilon) {
  EXPECT_TRUE(rel_near_result(kernels::add_epsilon(2.0), 2.0 + 1e-6, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, AddEpsilon_Negative_SubtractsEpsilon) {
  EXPECT_TRUE(rel_near_result(kernels::add_epsilon(-2.0), -2.0 - 1e-6, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, AddEpsilon_SignedZero_ReturnsPlusEpsilon) {
  EXPECT_TRUE(rel_near_result(kernels::add_epsilon(0.0), 1e-6, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(kernels::add_epsilon(-0.0), 1e-6, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, LimitCurvature_AboveOne_ReturnsOne) {
  EXPECT_DOUBLE_EQ(kernels::limit_curvature(1.5), 1.0);
}

TEST(mf5x_kernels, LimitCurvature_AtOrBelowOne_Unchanged) {
  EXPECT_DOUBLE_EQ(kernels::limit_curvature(1.0), 1.0);
  EXPECT_DOUBLE_EQ(kernels::limit_curvature(0.999), 0.999);
  EXPECT_DOUBLE_EQ(kernels::limit_curvature(-3.0), -3.0);
}

TEST(mf5x_kernels, SineMagicFormula_ClosedForm) {
  constexpr double b = 2.0;
  constexpr double c = 1.3;
  constexpr double d = 1000.0;
  constexpr double e = 0.5;
  constexpr double x = 0.1;
  const double bx = b * x;
  const double expected = d * std::sin(c * std::atan(bx - (e * (bx - std::atan(bx)))));
  EXPECT_TRUE(rel_near_result(kernels::sine_magic_formula(b, c, d, e, x), expected, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, CosineMagicFormula_ZeroB_ReturnsOne) {
  EXPECT_TRUE(rel_near_result(kernels::cosine_magic_formula(0.0, 1.2, 0.3, 0.7), 1.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, CosineMagicFormula_ClosedForm) {
  constexpr double b = 4.0;
  constexpr double c = 1.1;
  constexpr double e = -0.4;
  constexpr double x = 0.2;
  const double bx = b * x;
  const double expected = std::cos(c * std::atan(bx - (e * (bx - std::atan(bx)))));
  EXPECT_TRUE(rel_near_result(kernels::cosine_magic_formula(b, c, e, x), expected, REL_TOL_CLOSED_FORM));
}

// ---- vertical load, pure longitudinal and pure lateral ----------------------------------------------------------

TEST(mf5x_kernels, VerticalLoad_AtFnomin_DfzZero) {
  const kernels::load_terms load = kernels::vertical_load(s_with({}), FZ_NOMINAL);
  // V1: bit-equal to zero.
  EXPECT_EQ(bits(load.dfz), bits(0.0));
  EXPECT_TRUE(rel_near_result(load.fz0_prime_n, 4000.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, VerticalLoad_Lfzo12_DfzMinusOneSixth) {
  const kernels::load_terms load = kernels::vertical_load(s_with({{&coeffs::LFZO, 1.2}}), FZ_NOMINAL);
  EXPECT_TRUE(rel_near_result(load.dfz, -1.0 / 6.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(load.fz0_prime_n, 4800.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, PureLongitudinal_Kappa01_MatchesV2) {
  const kernels::longitudinal_terms t = longitudinal(s_with({}), FZ_NOMINAL, 0.1);
  EXPECT_NEAR(t.fx0_n, 3935.48, printed_tol(2));
  EXPECT_TRUE(rel_near_result(t.fx0_n, 4000.0 * std::sin(1.5 * std::atan(4.0 / 3.0)), 1e-9));
  EXPECT_TRUE(rel_near_result(t.cx, 1.5, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.dx_n, 4000.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.kx_n, 80000.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, PureLongitudinal_Pvx1_SvxOutsideSine_MatchesV4) {
  const kernels::longitudinal_terms t = longitudinal(s_with({{&coeffs::PVX1, 0.01}}), FZ_NOMINAL, 0.0);
  EXPECT_TRUE(rel_near_result(t.fx0_n, 40.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.svx_n, 40.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, PureLongitudinal_SlopeAtOrigin_MatchesV7) {
  const coeffs c = s_with({});
  constexpr double h = 1e-7;
  const double plus = longitudinal(c, FZ_NOMINAL, h).fx0_n;
  const double minus = longitudinal(c, FZ_NOMINAL, -h).fx0_n;
  EXPECT_TRUE(rel_near_result((plus - minus) / (2.0 * h), 80000.0, 1e-6));
}

TEST(mf5x_kernels, PureLongitudinal_Lmux05_MatchesV8) {
  const kernels::longitudinal_terms t = longitudinal(s_with({{&coeffs::LMUX, 0.5}}), FZ_NOMINAL, 0.1);
  EXPECT_TRUE(rel_near_result(t.dx_n, 2000.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.bx, 80.0 / 3.0, 1e-9));
}

TEST(mf5x_kernels, PureLongitudinal_Pdx3Lgax05_MatchesV16) {
  const coeffs c = s_with({{&coeffs::PDX3, 10.0}, {&coeffs::LGAX, 0.5}});
  const kernels::longitudinal_terms t = longitudinal(c, FZ_NOMINAL, 0.1, 0.1);
  EXPECT_TRUE(rel_near_result(t.gamma_x, 0.1 * 0.5, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.dx_n, 3900.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, PureLongitudinal_KxAtFnomin_EqualsPkx1FnominLkx) {
  const coeffs c = s_with({{&coeffs::PKX2, 3.0}, {&coeffs::PKX3, 0.5}, {&coeffs::LKX, 1.1}});
  const kernels::longitudinal_terms t = longitudinal(c, FZ_NOMINAL, 0.0);
  EXPECT_TRUE(rel_near_result(t.kx_n, 20.0 * 4000.0 * 1.1, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, PureLongitudinal_LoadDependence_ClosedForm) {
  // Fz = 5000 gives dfz = 0.25.
  const coeffs c = s_with({{&coeffs::PDX2, 0.1},
                           {&coeffs::PKX2, 4.0},
                           {&coeffs::PKX3, 0.4},
                           {&coeffs::PHX1, 0.001},
                           {&coeffs::PHX2, 0.004},
                           {&coeffs::PVX1, 0.01},
                           {&coeffs::PVX2, 0.04}});
  const kernels::longitudinal_terms t = longitudinal(c, 5000.0, 0.0);
  EXPECT_TRUE(rel_near_result(t.mu_x, 1.025, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.dx_n, 5125.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.kx_n, 5000.0 * 21.0 * std::exp(0.1), REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.shx, 0.002, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.svx_n, 100.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.kappa_x, 0.002, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, PureLongitudinal_PeakMagnitude_EqualsDx) {
  // P10: max |Fx0 - SVx| over kappa equals mu_x*Fz when Cx > 1.
  const coeffs c = s_with({{&coeffs::PVX1, 0.01}});
  constexpr double step_count = 20000.0;
  double peak = 0.0;
  double dx = 0.0;
  for (int i = -20000; i <= 20000; ++i) {
    const kernels::longitudinal_terms t = longitudinal(c, FZ_NOMINAL, static_cast<double>(i) / step_count);
    peak = std::max(peak, std::abs(t.fx0_n - t.svx_n));
    dx = t.dx_n;
  }
  EXPECT_TRUE(rel_near_result(peak, dx, 1e-6));
  EXPECT_LE(peak, dx * (1.0 + 1e-12));
}

TEST(mf5x_kernels, PureLongitudinal_ExClamp_V15) {
  const kernels::longitudinal_terms clamped = longitudinal(s_with({{&coeffs::PEX1, 1.5}}), FZ_NOMINAL, 0.1);
  const kernels::longitudinal_terms at_one = longitudinal(s_with({{&coeffs::PEX1, 1.0}}), FZ_NOMINAL, 0.1);
  const kernels::longitudinal_terms below = longitudinal(s_with({{&coeffs::PEX1, 0.999}}), FZ_NOMINAL, 0.1);
  // V15: bit-equal to PEX1 = 1.0.
  EXPECT_EQ(bits(clamped.ex), bits(at_one.ex));
  EXPECT_EQ(bits(clamped.fx0_n), bits(at_one.fx0_n));
  EXPECT_NEAR(clamped.fx0_n, 3603.08, printed_tol(2));
  EXPECT_NE(bits(below.ex), bits(at_one.ex));
  EXPECT_NE(bits(below.fx0_n), bits(at_one.fx0_n));
}

TEST(mf5x_kernels, PureLongitudinal_Pex4_CurvatureDependsOnSlipSign) {
  const coeffs c = s_with({{&coeffs::PEX1, 0.5}, {&coeffs::PEX4, 0.5}});
  EXPECT_TRUE(rel_near_result(longitudinal(c, FZ_NOMINAL, 0.1).ex, 0.25, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(longitudinal(c, FZ_NOMINAL, -0.1).ex, 0.75, REL_TOL_CLOSED_FORM));
  // sgn+(-0.0) = +1.
  EXPECT_TRUE(rel_near_result(longitudinal(c, FZ_NOMINAL, -0.0).ex, 0.25, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, PureLongitudinal_ZeroCxDx_GuardedFinite) {
  const coeffs c = s_with({{&coeffs::PCX1, 0.0}, {&coeffs::PVX1, 0.01}});
  const kernels::longitudinal_terms t = longitudinal(c, FZ_NOMINAL, 0.1);
  EXPECT_TRUE(std::isfinite(t.bx));
  EXPECT_TRUE(rel_near_result(t.bx, 80000.0 / 1e-6, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(std::isfinite(t.fx0_n));
  // sin(0) = 0 exactly (Cx = 0), so Fx0 is the vertical shift.
  EXPECT_TRUE(rel_near_result(t.fx0_n, t.svx_n, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, PureLateral_Alpha005_MatchesV3) {
  const kernels::lateral_terms t = lateral(s_with({}), FZ_NOMINAL, 0.05);
  EXPECT_NEAR(t.ky0_n, -55384.615, 0.0005);
  EXPECT_NEAR(t.by, -10.650888, 5e-7);
  EXPECT_NEAR(t.fy0_n, -2376.40, printed_tol(2));
}

TEST(mf5x_kernels, PureLateral_Lfzo12_MatchesV18) {
  const kernels::lateral_terms t = lateral(s_with({{&coeffs::LFZO, 1.2}}), FZ_NOMINAL, 0.05);
  EXPECT_NEAR(t.ky0_n, -61132.08, printed_tol(2));
}

TEST(mf5x_kernels, PureLateral_KyMaximum_AtPky2Fnomin) {
  // P9: |Ky| reaches |PKY1*FNOMIN*LKY| at Fz = PKY2*FNOMIN.
  const coeffs c = s_with({{&coeffs::LKY, 1.1}});
  const double fz_peak = 1.5 * 4000.0;
  const double ky_peak = std::abs(lateral(c, fz_peak, 0.0).ky0_n);
  EXPECT_TRUE(rel_near_result(ky_peak, std::abs(-15.0 * 4000.0 * 1.1), REL_TOL_CLOSED_FORM));
  EXPECT_GT(ky_peak, std::abs(lateral(c, fz_peak * 0.99, 0.0).ky0_n));
  EXPECT_GT(ky_peak, std::abs(lateral(c, fz_peak * 1.01, 0.0).ky0_n));
}

TEST(mf5x_kernels, PureLateral_Phy3_MatchesV17) {
  const kernels::lateral_terms t = lateral(s_with({{&coeffs::PHY3, 0.1}}), FZ_NOMINAL, 0.0, 0.1);
  EXPECT_TRUE(rel_near_result(t.gamma_y, 0.1, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.shy, 0.01, REL_TOL_CLOSED_FORM));
  EXPECT_NEAR(t.fy0_n, -550.02, printed_tol(2));
}

TEST(mf5x_kernels, PureLateral_EyClamp) {
  const kernels::lateral_terms clamped = lateral(s_with({{&coeffs::PEY1, 1.5}}), FZ_NOMINAL, 0.05);
  const kernels::lateral_terms at_one = lateral(s_with({{&coeffs::PEY1, 1.0}}), FZ_NOMINAL, 0.05);
  const kernels::lateral_terms below = lateral(s_with({{&coeffs::PEY1, 0.999}}), FZ_NOMINAL, 0.05);
  EXPECT_EQ(bits(clamped.ey), bits(at_one.ey));
  EXPECT_EQ(bits(clamped.fy0_n), bits(at_one.fy0_n));
  EXPECT_NE(bits(below.ey), bits(at_one.ey));
  EXPECT_NE(bits(below.fy0_n), bits(at_one.fy0_n));
}

TEST(mf5x_kernels, PureLateral_Pey3_CurvatureDependsOnSlipAngleSign) {
  const coeffs c = s_with({{&coeffs::PEY1, 0.5}, {&coeffs::PEY3, 0.5}});
  EXPECT_TRUE(rel_near_result(lateral(c, FZ_NOMINAL, 0.05).ey, 0.25, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(lateral(c, FZ_NOMINAL, -0.05).ey, 0.75, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, PureLateral_Camber_Pdy3Lgay) {
  const coeffs c = s_with({{&coeffs::PDY3, 10.0}, {&coeffs::LGAY, 0.5}});
  const kernels::lateral_terms t = lateral(c, FZ_NOMINAL, 0.0, 0.1);
  EXPECT_TRUE(rel_near_result(t.gamma_y, 0.1 * 0.5, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.dy_n, 3900.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.mu_y, kernels::lateral_friction(c, kernels::vertical_load(c, FZ_NOMINAL), 0.05),
                              REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, PureLateral_Pvy3_VerticalShiftFollowsCamber) {
  const coeffs c = s_with({{&coeffs::PVY3, 0.1}});
  const kernels::lateral_terms t = lateral(c, FZ_NOMINAL, 0.0, 0.1);
  EXPECT_TRUE(rel_near_result(t.svy_n, 4000.0 * 0.1 * 0.1, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, PureLateral_LoadDependence_ClosedForm) {
  // Fz = 5000 gives dfz = 0.25.
  const coeffs c = s_with({{&coeffs::PDY2, -0.1},
                           {&coeffs::PHY1, 0.002},
                           {&coeffs::PHY2, 0.004},
                           {&coeffs::PVY1, 0.01},
                           {&coeffs::PVY2, 0.04}});
  const kernels::lateral_terms t = lateral(c, 5000.0, 0.0);
  EXPECT_TRUE(rel_near_result(t.mu_y, 0.975, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.dy_n, 4875.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.shy, 0.003, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.svy_n, 100.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.alpha_y, 0.003, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, PureLateral_Lky_ScalesKyLinearly) {
  const double base = lateral(s_with({}), FZ_NOMINAL, 0.05).ky0_n;
  const double scaled = lateral(s_with({{&coeffs::LKY, 0.7}}), FZ_NOMINAL, 0.05).ky0_n;
  EXPECT_TRUE(rel_near_result(scaled, 0.7 * base, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, PureLateral_KyZeroViaPky3_ByZero) {
  const coeffs c = s_with({{&coeffs::PKY3, 1.0}, {&coeffs::PVY1, 0.01}});
  const kernels::lateral_terms t = lateral(c, FZ_NOMINAL, 0.05, 1.0);
  // Ky = Ky0*(1 - 1*1) is a product with an exact zero, so it is zero; sin(0) = 0 leaves Fy0 = SVy.
  constexpr double zero_tol = 1e-12;
  EXPECT_NEAR(t.ky_n, 0.0, zero_tol);
  EXPECT_NEAR(t.by, 0.0, zero_tol);
  EXPECT_TRUE(rel_near_result(t.fy0_n, t.svy_n, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, PureLateral_NegativeCyDy_EpsilonSubtracted) {
  const kernels::lateral_terms t = lateral(s_with({{&coeffs::PDY1, -1.0}}), FZ_NOMINAL, 0.05);
  EXPECT_LT(t.cy * t.dy_n, 0.0);
  // Equal to Ky/(Cy*Dy - 1e-6); a relative tolerance covers fused multiply-add targets.
  EXPECT_TRUE(rel_near_result(t.by, t.ky_n / ((t.cy * t.dy_n) - 1e-6), 1e-15));
}

TEST(mf5x_kernels, EvaluateSlipState_ComposesStages) {
  const coeffs c = s_with({{&coeffs::PHX1, 0.001}, {&coeffs::PVY1, 0.01}, {&coeffs::QDZ6, 0.01}});
  const mf::operating_point p = make_point(3500.0, 0.07, 0.04, 0.03);
  const kernels::slip_state s = kernels::evaluate_slip_state(c, p);
  const kernels::load_terms load = kernels::vertical_load(c, p.fz_n);
  const kernels::longitudinal_terms lon = kernels::pure_longitudinal(c, load, p.fz_n, p.kappa, p.gamma_rad);
  const kernels::lateral_terms lat = kernels::pure_lateral(c, load, p.fz_n, p.alpha_rad, p.gamma_rad);
  const kernels::aligning_terms aln = kernels::aligning(c, load, lat, p.fz_n, p.alpha_rad, p.gamma_rad);
  EXPECT_EQ(raw_bits(s.load), raw_bits(load));
  EXPECT_EQ(raw_bits(s.lon), raw_bits(lon));
  EXPECT_EQ(raw_bits(s.lat), raw_bits(lat));
  EXPECT_EQ(raw_bits(s.aln), raw_bits(aln));
}

// ---- aligning moment, equivalent slip angle, combined slip -------------------------------------------------------

TEST(mf5x_kernels, Aligning_Alpha005_MatchesV11) {
  const coeffs c = s_with({});
  const double cos_alpha = std::cos(0.05);
  const kernels::slip_state s = state_at(c, FZ_NOMINAL, 0.0, 0.05);
  const double t = kernels::trail(s.aln, s.aln.alpha_t, cos_alpha);
  const double mz0 = -(t * s.lat.fy0_n) + kernels::residual_moment(s.aln, s.aln.alpha_r, cos_alpha);
  EXPECT_NEAR(t, 0.0254434, 5e-8);
  EXPECT_NEAR(mz0, 60.46, printed_tol(2));
}

TEST(mf5x_kernels, Aligning_Lfzo12_DtIs003) {
  const kernels::slip_state s = state_at(s_with({{&coeffs::LFZO, 1.2}}), FZ_NOMINAL, 0.0, 0.05);
  EXPECT_TRUE(rel_near_result(s.aln.dt_m, 0.03, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(s.load.dfz, -1.0 / 6.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Aligning_Shf_UsesGuardedKy) {
  const kernels::slip_state s = state_at(s_with({{&coeffs::PVY1, 0.01}}), FZ_NOMINAL, 0.0, 0.05);
  const double expected = s.lat.shy + (s.lat.svy_n / kernels::add_epsilon(s.lat.ky_n));
  EXPECT_EQ(bits(s.aln.shf), bits(expected));
  EXPECT_TRUE(rel_near_result(s.aln.alpha_r, 0.05 + s.aln.shf, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Aligning_EtClamp) {
  const kernels::slip_state clamped = state_at(s_with({{&coeffs::QEZ1, 1.5}}), FZ_NOMINAL, 0.0, 0.05);
  const kernels::slip_state at_one = state_at(s_with({{&coeffs::QEZ1, 1.0}}), FZ_NOMINAL, 0.0, 0.05);
  const kernels::slip_state below = state_at(s_with({{&coeffs::QEZ1, 0.999}}), FZ_NOMINAL, 0.0, 0.05);
  EXPECT_EQ(bits(clamped.aln.et), bits(at_one.aln.et));
  EXPECT_NE(bits(below.aln.et), bits(at_one.aln.et));
}

TEST(mf5x_kernels, Aligning_Et_ClosedForm) {
  const kernels::slip_state s = state_at(s_with({{&coeffs::QEZ1, 0.5}, {&coeffs::QEZ4, 0.2}}), FZ_NOMINAL, 0.0, 0.05);
  const double expected = 0.5 * (1.0 + (0.2 * (2.0 / std::numbers::pi) * std::atan(10.0 * 1.2 * 0.05)));
  EXPECT_TRUE(rel_near_result(s.aln.et, expected, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Aligning_CamberAndLoad_ClosedForm) {
  // Fz = 5000 gives dfz = 0.25; gamma = 0.1 with LGAZ = 1.
  const coeffs c = s_with({{&coeffs::QBZ2, 4.0},
                           {&coeffs::QBZ4, 1.0},
                           {&coeffs::QBZ5, 2.0},
                           {&coeffs::QBZ9, 2.0},
                           {&coeffs::QBZ10, 0.5},
                           {&coeffs::QDZ2, 0.4},
                           {&coeffs::QDZ3, 1.0},
                           {&coeffs::QDZ4, 10.0},
                           {&coeffs::QDZ6, 0.01},
                           {&coeffs::QDZ7, 0.04},
                           {&coeffs::QDZ8, 0.02},
                           {&coeffs::QDZ9, 0.08},
                           {&coeffs::QHZ1, 0.01},
                           {&coeffs::QHZ2, 0.04},
                           {&coeffs::QHZ3, 0.1},
                           {&coeffs::QHZ4, 0.4}});
  const kernels::slip_state s = state_at(c, 5000.0, 0.0, 0.05, 0.1);
  EXPECT_TRUE(rel_near_result(s.aln.gamma_z, 0.1, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(s.aln.bt, 14.3, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(s.aln.dt_m, 0.09, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(s.aln.sht, 0.04, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(s.aln.alpha_t, 0.09, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(s.aln.dr_nm, 36.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(s.aln.br, 2.0 + (0.5 * s.lat.by * s.lat.cy), REL_TOL_CLOSED_FORM));
  EXPECT_DOUBLE_EQ(s.aln.cr, 1.0);
  EXPECT_DOUBLE_EQ(s.aln.ct, 1.2);
}

TEST(mf5x_kernels, Trail_EvenInSlip) {
  // P5.
  const coeffs c = s_with({{&coeffs::QEZ1, 0.5}, {&coeffs::QEZ4, 0.2}});
  const kernels::slip_state s = state_at(c, FZ_NOMINAL, 0.0, 0.05);
  for (const double x : {0.01, 0.05, 0.3, 1.0}) {
    EXPECT_TRUE(rel_near_result(kernels::trail(s.aln, -x, 0.9), kernels::trail(s.aln, x, 0.9), REL_TOL_CLOSED_FORM));
  }
}

TEST(mf5x_kernels, ResidualMoment_EvenInSlip) {
  // P5.
  const coeffs c = s_with({{&coeffs::QBZ10, 1.0}, {&coeffs::QDZ6, 0.01}});
  const kernels::slip_state s = state_at(c, FZ_NOMINAL, 0.0, 0.05);
  EXPECT_GT(std::abs(s.aln.dr_nm), 0.0);
  for (const double x : {0.01, 0.05, 0.3, 1.0}) {
    EXPECT_TRUE(rel_near_result(kernels::residual_moment(s.aln, -x, 0.9), kernels::residual_moment(s.aln, x, 0.9),
                                REL_TOL_CLOSED_FORM));
  }
}

TEST(mf5x_kernels, EquivalentSlipAngle_ZeroKappa_ReturnsInput) {
  // The tolerance covers the tan/atan round trip (a few ulp).
  for (const double a : {0.05, -0.05}) {
    EXPECT_TRUE(rel_near_result(kernels::equivalent_slip_angle(a, 0.0, 8e4, -5.5e4), a, 1e-14));
  }
}

TEST(mf5x_kernels, EquivalentSlipAngle_SignAtZero_UsesSgnPlus) {
  // sgn+(+0.0) = sgn+(-0.0) = +1; the sign flips only for a < 0.
  EXPECT_GT(kernels::equivalent_slip_angle(0.0, 0.1, 8e4, -5.5e4), 0.0);
  EXPECT_GT(kernels::equivalent_slip_angle(-0.0, 0.1, 8e4, -5.5e4), 0.0);
  EXPECT_LT(kernels::equivalent_slip_angle(-1e-9, 0.1, 8e4, -5.5e4), 0.0);
  EXPECT_GT(kernels::equivalent_slip_angle(1e-9, 0.1, 8e4, -5.5e4), 0.0);
}

TEST(mf5x_kernels, EquivalentSlipAngle_ClosedForm) {
  const double tan_a = std::tan(0.1);
  const double ratio = 8e4 / -5.5e4;
  const double expected = std::atan(std::sqrt((tan_a * tan_a) + (ratio * ratio * 0.05 * 0.05)));
  EXPECT_TRUE(rel_near_result(kernels::equivalent_slip_angle(0.1, 0.05, 8e4, -5.5e4), expected, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, LongitudinalWeighting_ZeroB_ReturnsOne) {
  const coeffs c = s_with({{&coeffs::RBX1, 0.0}});
  const double gxa = kernels::longitudinal_weighting(c, kernels::vertical_load(c, FZ_NOMINAL), 0.1, 0.1);
  EXPECT_TRUE(rel_near_result(gxa, 1.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, LongitudinalWeighting_MatchesV12Factor) {
  const coeffs c = s_with({});
  const double gxa = kernels::longitudinal_weighting(c, kernels::vertical_load(c, FZ_NOMINAL), 0.1, 0.1);
  EXPECT_TRUE(rel_near_result(gxa, std::cos(std::numbers::pi / 4.0), REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, LateralWeighting_ZeroB_ReturnsOne) {
  const coeffs c = s_with({{&coeffs::RBY1, 0.0}});
  const double gyk = kernels::lateral_weighting(c, kernels::vertical_load(c, FZ_NOMINAL), 0.1, 0.05);
  EXPECT_TRUE(rel_near_result(gyk, 1.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, LateralWeighting_MatchesV13Factor) {
  const coeffs c = s_with({});
  const double gyk = kernels::lateral_weighting(c, kernels::vertical_load(c, FZ_NOMINAL), 0.1, 0.05);
  EXPECT_TRUE(rel_near_result(gyk, std::cos(std::numbers::pi / 4.0), REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Weighting_ExaEykClamp) {
  const auto gxa = [](double rex1) {
    const coeffs c = s_with({{&coeffs::REX1, rex1}});
    return kernels::longitudinal_weighting(c, kernels::vertical_load(c, FZ_NOMINAL), 0.1, 0.1);
  };
  const auto gyk = [](double rey1) {
    const coeffs c = s_with({{&coeffs::REY1, rey1}});
    return kernels::lateral_weighting(c, kernels::vertical_load(c, FZ_NOMINAL), 0.1, 0.05);
  };
  EXPECT_EQ(bits(gxa(1.5)), bits(gxa(1.0)));
  EXPECT_NE(bits(gxa(0.999)), bits(gxa(1.0)));
  EXPECT_EQ(bits(gyk(1.5)), bits(gyk(1.0)));
  EXPECT_NE(bits(gyk(0.999)), bits(gyk(1.0)));
}

TEST(mf5x_kernels, Weighting_NotBoundedByOne) {
  // No G <= 1 assumption: a horizontal shift makes the numerator exceed the denominator.
  const coeffs c = s_with({{&coeffs::RHX1, 0.1}});
  const double gxa = kernels::longitudinal_weighting(c, kernels::vertical_load(c, FZ_NOMINAL), 0.1, -0.1);
  EXPECT_GT(gxa, 1.0);
  EXPECT_TRUE(rel_near_result(gxa, 1.0 / std::cos(std::numbers::pi / 4.0), REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, KappaInducedSideForce_MatchesV14) {
  const coeffs c = s_with({{&coeffs::RBY1, 0.0}, {&coeffs::RVY1, 0.1}, {&coeffs::RVY5, 1.0}, {&coeffs::RVY6, 10.0}});
  const kernels::load_terms load = kernels::vertical_load(c, FZ_NOMINAL);
  const double mu_y = kernels::lateral_friction(c, load, 0.0);
  EXPECT_NEAR(kernels::kappa_induced_side_force(c, load, mu_y, FZ_NOMINAL, 0.1, 0.05, 0.0), 282.843, 0.0005);
}

TEST(mf5x_kernels, KappaInducedSideForce_ZeroCoefficients_ReturnsZero) {
  const coeffs c = s_with({});
  const kernels::load_terms load = kernels::vertical_load(c, FZ_NOMINAL);
  EXPECT_NEAR(kernels::kappa_induced_side_force(c, load, 1.0, FZ_NOMINAL, 0.1, 0.05, 0.1), 0.0, 1e-12);
}

TEST(mf5x_kernels, KappaInducedSideForce_CamberAndLoad_ClosedForm) {
  // Fz = 5000 gives dfz = 0.25; DVyk = mu_y*Fz*(RVY1 + RVY2*dfz + RVY3*gamma)*cos(atan(RVY4*alpha)).
  const coeffs c = s_with({{&coeffs::RVY1, 0.1},
                           {&coeffs::RVY2, 0.2},
                           {&coeffs::RVY3, 0.5},
                           {&coeffs::RVY4, 2.0},
                           {&coeffs::RVY5, 1.0},
                           {&coeffs::RVY6, 10.0},
                           {&coeffs::LVYKA, 0.8}});
  const kernels::load_terms load = kernels::vertical_load(c, 5000.0);
  const double dvyk = 1.0 * 5000.0 * (0.1 + (0.2 * 0.25) + (0.5 * 0.1)) * std::cos(std::atan(2.0 * 0.05));
  const double expected = dvyk * std::sin(std::atan(10.0 * 0.1)) * 0.8;
  EXPECT_TRUE(rel_near_result(kernels::kappa_induced_side_force(c, load, 1.0, 5000.0, 0.1, 0.05, 0.1), expected,
                              REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, CosineCombinedForces_MatchesV12V13) {
  const coeffs c = s_with({});
  const kernels::combined_forces both = combined_at(c, FZ_NOMINAL, 0.1, 0.1);
  EXPECT_NEAR(both.fx_n, 2782.80, printed_tol(2));
  const kernels::combined_forces lateral_case = combined_at(c, FZ_NOMINAL, 0.1, 0.05);
  EXPECT_NEAR(lateral_case.fy_n, -1680.37, printed_tol(2));
  EXPECT_NEAR(lateral_case.svyk_n, 0.0, 1e-12);
}

TEST(mf5x_kernels, CosineCombinedForces_AddsSvykToFy) {
  const coeffs c = s_with({{&coeffs::RBY1, 0.0}, {&coeffs::RVY1, 0.1}, {&coeffs::RVY5, 1.0}, {&coeffs::RVY6, 10.0}});
  const mf::operating_point p = make_point(FZ_NOMINAL, 0.1, 0.05, 0.0);
  const kernels::slip_state s = kernels::evaluate_slip_state(c, p);
  const kernels::combined_forces forces = kernels::cosine_combined_forces(c, s, p);
  // V14: SVyk = 400*sin(atan(1)) and Fy = Fy0 + SVyk (Gyk = 1 because RBY1 = 0).
  EXPECT_TRUE(rel_near_result(forces.svyk_n, 400.0 * std::sin(std::numbers::pi / 4.0), REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(forces.fy_n, s.lat.fy0_n + forces.svyk_n, REL_TOL_CLOSED_FORM));
}

// ---- friction ellipse -------------------------------------------------------------------------------------------

TEST(mf5x_kernels, FrictionEllipse_FourQuadrants_SignsAndMagnitudes) {
  // P1.
  const coeffs c = ellipse_coefficients();
  for (const double kappa : {0.1, -0.1}) {
    for (const double alpha : {0.05, -0.05}) {
      const kernels::slip_state s = state_at(c, FZ_NOMINAL, kappa, alpha);
      const kernels::ellipse_ratios ratios = kernels::friction_ellipse_ratios(s, kappa, alpha);
      SCOPED_TRACE(std::format("kappa {} alpha {}", kappa, alpha));
      EXPECT_TRUE(is_unit_ratio(ratios.rx));
      EXPECT_TRUE(is_unit_ratio(ratios.ry));
      EXPECT_TRUE(is_reduction_of(ratios.rx * s.lon.fx0_n, s.lon.fx0_n));
      EXPECT_TRUE(is_reduction_of(ratios.ry * s.lat.fy0_n, s.lat.fy0_n));
    }
  }
}

TEST(mf5x_kernels, FrictionEllipse_MatchesPrintedFormWithAbsMu) {
  // P2: 1000 fixed-seed points; the tolerance 1e-10 covers the cancellation inherent in the printed form.
  const coeffs c = ellipse_coefficients();
  std::mt19937 rng(20240229U);
  std::uniform_real_distribution<double> kappa_dist(-0.5, 0.5);
  std::uniform_real_distribution<double> alpha_dist(-0.3, 0.3);
  std::uniform_real_distribution<double> fz_dist(500.0, 8000.0);
  for (int i = 0; i < 1000; ++i) {
    const double kappa = kappa_dist(rng);
    const double alpha = alpha_dist(rng);
    const double fz = fz_dist(rng);
    const kernels::slip_state s = state_at(c, fz, kappa, alpha);
    const kernels::ellipse_ratios actual = kernels::friction_ellipse_ratios(s, kappa, alpha);
    const kernels::ellipse_ratios expected = printed_form_ratios(s, fz, kappa, alpha);
    EXPECT_TRUE(rel_near_result(actual.rx, expected.rx, 1e-10)) << "point " << i;
    EXPECT_TRUE(rel_near_result(actual.ry, expected.ry, 1e-10)) << "point " << i;
  }
}

TEST(mf5x_kernels, FrictionEllipse_DegeneratePoints_RatioOne) {
  // P16: kappa_c = 0 and alpha* = 0 give the 0/0 ratio 1.
  const coeffs c = s_with({});
  const kernels::ellipse_ratios origin = kernels::friction_ellipse_ratios(state_at(c, FZ_NOMINAL, 0.0, 0.0), 0.0, 0.0);
  EXPECT_DOUBLE_EQ(origin.rx, 1.0);
  EXPECT_DOUBLE_EQ(origin.ry, 1.0);
}

TEST(mf5x_kernels, FrictionEllipse_OneZeroSlip_RatioZeroAndOne) {
  // A hand-built state whose pure forces are non-zero at the shifted origin: kappa_c = 0 gives rx = 0 and ry = 1.
  kernels::slip_state s;
  s.lon.kx_n = 80000.0;
  s.lon.fx0_n = 1000.0;
  s.lon.dx_n = 4000.0;
  s.lat.ky_n = -55000.0;
  s.lat.fy0_n = -2000.0;
  s.lat.dy_n = 4000.0;
  const kernels::ellipse_ratios no_kappa = kernels::friction_ellipse_ratios(s, 0.0, 0.05);
  EXPECT_NEAR(no_kappa.rx, 0.0, 1e-12);
  EXPECT_NEAR(no_kappa.ry, 1.0, 1e-12);
  // alpha* = 0 gives ry = 0 and rx = 1.
  const kernels::ellipse_ratios no_alpha = kernels::friction_ellipse_ratios(s, 0.1, 0.0);
  EXPECT_NEAR(no_alpha.rx, 1.0, 1e-12);
  EXPECT_NEAR(no_alpha.ry, 0.0, 1e-12);
}

TEST(mf5x_kernels, FrictionEllipse_ZeroKx_GuardedFinite) {
  const coeffs c = s_with({{&coeffs::PKX1, 0.0}, {&coeffs::PVX1, 0.01}});
  const kernels::slip_state s = state_at(c, FZ_NOMINAL, 0.1, 0.05);
  const kernels::ellipse_ratios ratios = kernels::friction_ellipse_ratios(s, 0.1, 0.05);
  EXPECT_TRUE(std::isfinite(ratios.rx));
  EXPECT_TRUE(std::isfinite(ratios.ry));
  EXPECT_GE(ratios.rx, 0.0);
  EXPECT_LE(ratios.rx, 1.0 + 1e-12);
}

// ---- moment arm, Mx, My -----------------------------------------------------------------------------------------

TEST(mf5x_kernels, MomentArm_ClosedForm) {
  const coeffs c = s_with(
      {{&coeffs::SSZ1, 0.1}, {&coeffs::SSZ2, 0.2}, {&coeffs::SSZ3, 0.3}, {&coeffs::SSZ4, 0.4}, {&coeffs::LS, 0.9}});
  const kernels::load_terms load = kernels::vertical_load(c, FZ_NOMINAL);
  // R0*(SSZ1 + SSZ2*Fy/FNOMIN + (SSZ3 + SSZ4*dfz)*gamma)*LS with dfz = 0.
  const double expected = 0.3 * (0.1 + (0.2 * -1000.0 / 4000.0) + (0.3 * 0.05)) * 0.9;
  EXPECT_TRUE(rel_near_result(kernels::moment_arm(c, load, -1000.0, 0.05), expected, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(kernels::moment_arm(c, load, -1000.0, 0.05), 0.01755, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, MomentArm_Lfzo12_NormalisedByFnomin) {
  const coeffs c = s_with(
      {{&coeffs::LFZO, 1.2}, {&coeffs::SSZ1, 0.1}, {&coeffs::SSZ2, 0.2}, {&coeffs::SSZ3, 0.3}, {&coeffs::SSZ4, 0.4}});
  const kernels::load_terms load = kernels::vertical_load(c, FZ_NOMINAL);
  const double dfz = -1.0 / 6.0;
  const double expected = 0.3 * (0.1 + (0.2 * -1000.0 / 4000.0) + ((0.3 + (0.4 * dfz)) * 0.05));
  const double with_f0_prime = 0.3 * (0.1 + (0.2 * -1000.0 / 4800.0) + ((0.3 + (0.4 * dfz)) * 0.05));
  const double actual = kernels::moment_arm(c, load, -1000.0, 0.05);
  EXPECT_TRUE(rel_near_result(actual, expected, REL_TOL_CLOSED_FORM));
  EXPECT_FALSE(rel_near_result(actual, with_f0_prime, 1e-6));
}

TEST(mf5x_kernels, OverturningMoment_MatchesV9) {
  const coeffs c = s_with({{&coeffs::QSX1, 0.01}, {&coeffs::QSX2, 0.5}, {&coeffs::QSX3, 0.1}});
  EXPECT_TRUE(rel_near_result(kernels::overturning_moment(c, FZ_NOMINAL, 0.0, 0.05), -18.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, OverturningMoment_ClosedForm) {
  // P11 with LVMX = 0.8, LMX = 1.1 and Fy = 500.
  const coeffs c = s_with(
      {{&coeffs::QSX1, 0.01}, {&coeffs::QSX2, 0.5}, {&coeffs::QSX3, 0.1}, {&coeffs::LVMX, 0.8}, {&coeffs::LMX, 1.1}});
  const double expected = 0.3 * 4000.0 * ((0.01 * 0.8) - (0.5 * 0.05) + (0.1 * 500.0 / 4000.0)) * 1.1;
  EXPECT_TRUE(rel_near_result(kernels::overturning_moment(c, FZ_NOMINAL, 500.0, 0.05), expected, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(expected, -5.94, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, RollingResistance_MatchesV5) {
  const double my = kernels::rolling_resistance_moment(s_with({}), FZ_NOMINAL, 0.0, SPEED_MPS);
  EXPECT_TRUE(rel_near_result(my, -12.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, RollingResistance_SpeedTerms_MatchesV21) {
  const coeffs c = s_with({{&coeffs::QSY3, 0.1}, {&coeffs::QSY4, 0.01}});
  const double my = kernels::rolling_resistance_moment(c, FZ_NOMINAL, 0.0, SPEED_MPS);
  const double ratio = 10.0 / 16.6;
  const double expected = -1200.0 * (0.01 + (0.1 * ratio) + (0.01 * std::pow(ratio, 4.0)));
  EXPECT_TRUE(rel_near_result(my, expected, REL_TOL_CLOSED_FORM));
  EXPECT_NEAR(my, -85.87, printed_tol(2));
}

TEST(mf5x_kernels, RollingResistance_NegativeSpeed_UsesAbsoluteValueForLinearTerm) {
  const coeffs c = s_with({{&coeffs::QSY3, 0.1}});
  const double forward = kernels::rolling_resistance_moment(c, FZ_NOMINAL, 0.0, SPEED_MPS);
  const double backward = kernels::rolling_resistance_moment(c, FZ_NOMINAL, 0.0, -SPEED_MPS);
  EXPECT_TRUE(rel_near_result(backward, forward, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, RollingResistance_LinearInFx) {
  // P12: the slope of My_out versus Fx is -R0*Fz*QSY2*LMY/FNOMIN.
  const coeffs c = s_with({{&coeffs::QSY2, 0.1}, {&coeffs::LMY, 0.9}});
  const double at_zero = kernels::rolling_resistance_moment(c, FZ_NOMINAL, 0.0, SPEED_MPS);
  const double at_1000 = kernels::rolling_resistance_moment(c, FZ_NOMINAL, 1000.0, SPEED_MPS);
  const double expected_slope = -0.3 * 4000.0 * 0.1 * 0.9 / 4000.0;
  EXPECT_TRUE(rel_near_result((at_1000 - at_zero) / 1000.0, expected_slope, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, RollingResistanceEq88_MatchesV10) {
  const coeffs c = s_with({{&coeffs::PHX1, 0.001}, {&coeffs::PVX1, 0.01}});
  const kernels::longitudinal_terms lon = longitudinal(c, FZ_NOMINAL, 0.0);
  EXPECT_TRUE(rel_near_result(kernels::rolling_resistance_moment_eq88(c, lon), -36.0, REL_TOL_CLOSED_FORM));
}
