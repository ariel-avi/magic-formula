#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <array>
#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/detail/dual_lane.hpp>
#include <magic_formula/detail/lanes.hpp>
#include <magic_formula/detail/mf5x_characteristics.hpp>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_forms.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <numbers>
#include <vector>

// Closed-form oracles are plain-double functions of this file (std::atan/sin/cos/sqrt); they never call the kernels.
// Reference numbers: docs/research/test-data.md.
namespace {
  namespace kernels = mf::detail;
  using kernels::block_lane;
  using kernels::point_lane;
  using mf::test::bits;
  using mf::test::lanes_of;
  using mf::test::near_scaled;
  using mf::test::rel_near_result;

  // Relative tolerance of closed forms that include the guard epsilon exactly as the kernel does.
  constexpr double SLOPE_TOL = mf::test::REL_TOL_CLOSED_FORM;
  // Relative tolerance of the stationary slips (root conditioning).
  constexpr double PEAK_TOL = 1e-10;
  // Relative tolerance against the printed reference numbers (13 significant digits).
  constexpr double TABLE_TOL = 1e-10;
  // Stationarity: the analytic slope at a found peak is within this fraction of the stiffness.
  constexpr double STATIONARY_TOL = 1e-9;
  // Half distance used to show that the slope changes sign at a peak.
  constexpr double SIGN_PROBE = 1e-6;
  // Exact equality up to the sign of zero.
  constexpr double EXACT_TOL = 0.0;

  constexpr double FZ_N = 4000.0;
  constexpr double FX_STIFFNESS_N = 80000.0;
  constexpr double INF = std::numeric_limits<double>::infinity();
  constexpr double SQRT3 = std::numbers::sqrt3;

  // The eight channels of one row.
  struct chars {
    double dfy_dsa = 0.0;
    double dfx_dsr = 0.0;
    double fy_over_sa = 0.0;
    double fx_over_sr = 0.0;
    double dy = 0.0;
    double dx = 0.0;
    double sa_peak = 0.0;
    double sr_peak = 0.0;
  };

  // A model setup: coefficients and limits, forms and slip mode.
  struct setup {
    mf::mf5x_parameters params = mf::test::fixture_s();
    mf::mf5x_forms forms{};
    mf::slip_mode mode = mf::slip_mode::COMBINED;
  };

  mf::mf5x_forms ellipse_forms() {
    mf::mf5x_forms forms;
    forms.combined = mf::combined_method::FRICTION_ELLIPSE;
    return forms;
  }

  setup ellipse_setup() {
    setup s;
    s.forms = ellipse_forms();
    return s;
  }

  setup uncombined_setup() {
    setup s;
    s.mode = mf::slip_mode::UNCOMBINED;
    return s;
  }

  setup ranged_setup() {
    setup s;
    s.params = mf::test::fixture_s_rng();
    return s;
  }

  mf::test::point pt(double kappa, double alpha, double fz = FZ_N) {
    return mf::test::point{.fz_n = fz, .kappa = kappa, .alpha_rad = alpha, .gamma_rad = 0.0, .vx_mps = 10.0};
  }

  template <class Lane_T> chars chars_at(const kernels::characteristic_lanes<Lane_T> &r, Eigen::Index i) {
    chars out;
    out.dfy_dsa = r.dfy_dsa_n_per_rad(i);
    out.dfx_dsr = r.dfx_dsr_n(i);
    out.fy_over_sa = r.fy_over_sa_n_per_rad(i);
    out.fx_over_sr = r.fx_over_sr_n(i);
    out.dy = r.dy_n(i);
    out.dx = r.dx_n(i);
    out.sa_peak = r.sa_at_peak_fy_rad(i);
    out.sr_peak = r.sr_at_peak_fx(i);
    return out;
  }

  // Generic filling of input lanes from points (one row per point).
  template <class Lane_T> kernels::input_lanes<Lane_T> make_inputs(const std::vector<mf::test::point> &points) {
    const auto rows = static_cast<Eigen::Index>(points.size());
    kernels::input_lanes<Lane_T> in;
    in.fz_n = Lane_T::Zero(rows);
    in.alpha_rad = Lane_T::Zero(rows);
    in.kappa = Lane_T::Zero(rows);
    in.gamma_rad = Lane_T::Zero(rows);
    in.vx_mps = Lane_T::Zero(rows);
    for (Eigen::Index i = 0; i < rows; ++i) {
      const mf::test::point &p = points[static_cast<std::size_t>(i)];
      in.fz_n(i) = p.fz_n;
      in.alpha_rad(i) = p.alpha_rad;
      in.kappa(i) = p.kappa;
      in.gamma_rad(i) = p.gamma_rad;
      in.vx_mps(i) = p.vx_mps;
    }
    return in;
  }

  // One chunk per point for a point lane, one chunk with all points for a block lane.
  template <class Lane_T> std::vector<std::vector<mf::test::point>> chunks_of(const std::vector<mf::test::point> &pts) {
    std::vector<std::vector<mf::test::point>> out;
    if constexpr (Lane_T::RowsAtCompileTime == 1) {
      for (const mf::test::point &p : pts) {
        out.push_back(std::vector<mf::test::point>{p});
      }
    } else {
      out.push_back(pts);
    }
    return out;
  }

  chars run_point(const setup &s, const mf::test::point &p) {
    const kernels::characteristic_lanes<point_lane> r =
        kernels::mf5x_characteristics<point_lane>(s.params.coefficients, s.forms, s.mode, s.params.limits, lanes_of(p));
    return chars_at(r, 0);
  }

  std::vector<chars> run_block(const setup &s, const std::vector<mf::test::point> &points) {
    const kernels::characteristic_lanes<block_lane> r = kernels::mf5x_characteristics<block_lane>(
        s.params.coefficients, s.forms, s.mode, s.params.limits, make_inputs<block_lane>(points));
    std::vector<chars> out;
    out.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
      out.push_back(chars_at(r, static_cast<Eigen::Index>(i)));
    }
    return out;
  }

  // Scales (absolute floor of near_scaled) of the channels: slopes in N/rad and N, peaks in rad and 1.
  constexpr double SLOPE_SCALE = 1e4;
  constexpr double PEAK_SCALE = 0.1;

  void expect_channel_match(double block, double point, double scale, const char *label) {
    if (std::isnan(point)) {
      EXPECT_TRUE(std::isnan(block)) << label;
    } else {
      EXPECT_TRUE(near_scaled(block, point, mf::test::BATCH_REL_TOL, scale)) << label;
    }
  }

  void expect_chars_match(const chars &block, const chars &point) {
    expect_channel_match(block.dfy_dsa, point.dfy_dsa, SLOPE_SCALE, "dfy_dsa");
    expect_channel_match(block.dfx_dsr, point.dfx_dsr, SLOPE_SCALE, "dfx_dsr");
    expect_channel_match(block.fy_over_sa, point.fy_over_sa, SLOPE_SCALE, "fy_over_sa");
    expect_channel_match(block.fx_over_sr, point.fx_over_sr, SLOPE_SCALE, "fx_over_sr");
    expect_channel_match(block.dy, point.dy, SLOPE_SCALE, "dy");
    expect_channel_match(block.dx, point.dx, SLOPE_SCALE, "dx");
    expect_channel_match(block.sa_peak, point.sa_peak, PEAK_SCALE, "sa_peak");
    expect_channel_match(block.sr_peak, point.sr_peak, PEAK_SCALE, "sr_peak");
  }

  // The point-lane result of `p`; the same row inside a mixed block must agree.
  chars run(const setup &s, const mf::test::point &p) {
    const chars single = run_point(s, p);
    const std::vector<mf::test::point> mixed{pt(0.07, -0.04), p, pt(0.1, 0.05, 0.0)};
    expect_chars_match(run_block(s, mixed)[1], single);
    return single;
  }

  void expect_rel(double actual, double expected, double tol, const char *label) {
    EXPECT_TRUE(rel_near_result(actual, expected, tol)) << label;
  }

  void expect_zero(double actual, const char *label) {
    EXPECT_NEAR(actual, 0.0, EXACT_TOL) << label;
  }

  void expect_bits(double actual, double expected, const char *label) {
    EXPECT_EQ(bits(actual), bits(expected)) << label;
  }

  // ---- closed-form oracles of fixture S (Fz 4000, dfz 0, gamma 0) ----

  // D*sin(C*atan(B*x - E*(B*x - atan(B*x)))) and its derivative in x.
  struct curve {
    double b;
    double c;
    double d;
    double e;
  };

  double curve_force(const curve &k, double x) {
    const double bx = k.b * x;
    const double phi = bx - (k.e * (bx - std::atan(bx)));
    return k.d * std::sin(k.c * std::atan(phi));
  }

  double curve_slope(const curve &k, double x) {
    const double bx = k.b * x;
    const double phi = bx - (k.e * (bx - std::atan(bx)));
    const double dphi = k.b - (k.e * (k.b - (k.b / (1.0 + (bx * bx)))));
    return k.d * k.c * std::cos(k.c * std::atan(phi)) * dphi / (1.0 + (phi * phi));
  }

  // Ky0 = PKY1*Fz0*sin(2*atan(Fz/(PKY2*Fz0))) of fixture S.
  double ky0_n_per_rad() {
    return -15.0 * FZ_N * std::sin(2.0 * std::atan(1.0 / 1.5));
  }

  // Bx = Kx/(Cx*Dx + eps), By = Ky0/(Cy*Dy + eps).
  curve curve_x(double cx = 1.5, double ex = 0.0) {
    return curve{.b = FX_STIFFNESS_N / ((cx * FZ_N) + kernels::EPSILON), .c = cx, .d = FZ_N, .e = ex};
  }

  curve curve_y(double cy = 1.3, double ey = 0.0) {
    return curve{.b = ky0_n_per_rad() / ((cy * FZ_N) + kernels::EPSILON), .c = cy, .d = FZ_N, .e = ey};
  }

  // Combined-slip weights of fixture S: Gxa(alpha) and Gyk(kappa).
  double gxa_of(double alpha) {
    return 1.0 / std::sqrt(1.0 + (100.0 * alpha * alpha));
  }

  double gyk_of(double kappa) {
    return 1.0 / std::sqrt(1.0 + (100.0 * kappa * kappa));
  }

  // The analytic slope dFy/dalpha (kappa and the other inputs fixed) at `alpha`, from the dual kernels.
  using dual_planar = kernels::planar_forces<kernels::dual_lane<point_lane>>;

  double lateral_slope_at(const setup &s, const mf::test::point &p, double alpha) {
    const auto in = kernels::seed_alpha(lanes_of(p), mf::test::lane(alpha), mf::test::lane(1.0));
    const dual_planar r = kernels::mf5x_planar_forces(s.params.coefficients, s.forms.combined, s.mode, in);
    return r.fy_n.derivative(0);
  }

  double longitudinal_slope_at(const setup &s, const mf::test::point &p, double kappa) {
    const auto in = kernels::seed_kappa(lanes_of(p), mf::test::lane(kappa), mf::test::lane(1.0));
    const dual_planar r = kernels::mf5x_planar_forces(s.params.coefficients, s.forms.combined, s.mode, in);
    return r.fx_n.derivative(0);
  }

  // The slope vanishes at `peak` and changes sign across it.
  void expect_lateral_peak(const setup &s, const mf::test::point &p, double peak) {
    ASSERT_FALSE(std::isnan(peak));
    EXPECT_LE(std::abs(lateral_slope_at(s, p, peak)), STATIONARY_TOL * std::abs(ky0_n_per_rad()));
    const double before = lateral_slope_at(s, p, peak - SIGN_PROBE);
    const double after = lateral_slope_at(s, p, peak + SIGN_PROBE);
    EXPECT_LT(before * after, 0.0);
  }

  void expect_longitudinal_peak(const setup &s, const mf::test::point &p, double peak) {
    ASSERT_FALSE(std::isnan(peak));
    EXPECT_LE(std::abs(longitudinal_slope_at(s, p, peak)), STATIONARY_TOL * FX_STIFFNESS_N);
    const double before = longitudinal_slope_at(s, p, peak - SIGN_PROBE);
    const double after = longitudinal_slope_at(s, p, peak + SIGN_PROBE);
    EXPECT_LT(before * after, 0.0);
  }

  // |u| with (1 - E)*|u| + E*atan|u| = sqrt(3) (the peak of a C = 1.5 curve), by plain bisection.
  double stationary_u(double e) {
    constexpr int steps = 200;
    constexpr double search_end = 100.0;
    double lo = 0.0;
    double hi = search_end;
    for (int i = 0; i < steps; ++i) {
      const double mid = 0.5 * (lo + hi);
      const double residual = ((1.0 - e) * mid) + (e * std::atan(mid)) - SQRT3;
      if (residual < 0.0) {
        lo = mid;
      } else {
        hi = mid;
      }
    }
    return 0.5 * (lo + hi);
  }

  // Friction-ellipse reference of fixture S_ell (all shifts 0), written with the force magnitudes only
  // (no Fz dependence beyond the curves).
  struct ellipse_reference {
    double fx = 0.0;
    double fy = 0.0;
    double dfx = 0.0;
    double dfy = 0.0;
  };

  ellipse_reference ellipse_at(double kappa, double alpha) {
    const curve cx = curve_x();
    const curve cy = curve_y();
    const double g = curve_force(cx, kappa);
    const double dg = curve_slope(cx, kappa);
    const double h = curve_force(cy, alpha);
    const double dh = curve_slope(cy, alpha);
    const double mxm = FZ_N;
    const double mym = FZ_N;
    const double kc = kappa;
    const double a_star = std::sin(alpha);
    const double sgn_kc = kc >= 0.0 ? 1.0 : -1.0;
    const double sgn_a = a_star >= 0.0 ? 1.0 : -1.0;
    const double rx_root = std::sqrt((kc * kc * mym * mym) + (g * g * a_star * a_star));
    const double ry_root = std::sqrt((kc * kc * h * h) + (a_star * a_star * mxm * mxm));
    const double rx = std::abs(kc) * mym / rx_root;
    const double ry = std::abs(a_star) * mxm / ry_root;
    const double drx = mym * a_star * a_star * ((sgn_kc * g * g) - (std::abs(kc) * g * dg)) / std::pow(rx_root, 3);
    const double dry =
        mxm * kc * kc * ((sgn_a * std::cos(alpha) * h * h) - (std::abs(a_star) * h * dh)) / std::pow(ry_root, 3);
    return ellipse_reference{.fx = g * rx, .fy = h * ry, .dfx = (dg * rx) + (g * drx), .dfy = (dh * ry) + (h * dry)};
  }
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// mf5x_planar_forces
// ---------------------------------------------------------------------------------------------------------------

namespace {
  std::vector<mf::test::point> planar_points() {
    std::vector<mf::test::point> out;
    out.push_back(pt(0.1, 0.0));
    out.push_back(pt(0.0, 0.05));
    out.push_back(pt(0.1, 0.1));
    out.push_back(pt(0.1, 0.05));
    out.push_back(pt(0.05, 0.1));
    out.push_back(pt(0.0, 0.0));
    out.push_back(pt(-0.07, -0.03));
    out.push_back(pt(0.02, -0.2));
    out.push_back(pt(0.3, 0.2));
    out.push_back(pt(0.0, 0.05, 3000.0));
    return out;
  }

  void expect_same(double actual, double expected, Eigen::Index i) {
    EXPECT_EQ(bits(actual), bits(expected)) << i;
  }

  template <class Lane_T>
  void expect_planar_rows(const kernels::planar_forces<Lane_T> &planar, const kernels::force_lanes<Lane_T> &evaluated,
                          bool expect_pure) {
    for (Eigen::Index i = 0; i < planar.fx_n.size(); ++i) {
      expect_same(planar.fx_n(i), evaluated.fx_n(i), i);
      expect_same(planar.fy_n(i), evaluated.fy_n(i), i);
      if (expect_pure) {
        expect_same(planar.fx_n(i), planar.lon.fx0_n(i), i);
        expect_same(planar.fy_n(i), planar.lat.fy0_n(i), i);
      }
    }
  }

  template <class Lane_T>
  void expect_planar_matches_evaluate(const mf::mf5x_forms &forms, mf::slip_mode mode, bool expect_pure) {
    const mf::mf5x_coefficients c = mf::test::fixture_s().coefficients;
    for (const std::vector<mf::test::point> &chunk : chunks_of<Lane_T>(planar_points())) {
      const kernels::input_lanes<Lane_T> in = make_inputs<Lane_T>(chunk);
      const kernels::planar_forces<Lane_T> planar = kernels::mf5x_planar_forces(c, forms.combined, mode, in);
      const kernels::force_lanes<Lane_T> evaluated = kernels::mf5x_evaluate(c, forms, mode, in);
      expect_planar_rows<Lane_T>(planar, evaluated, expect_pure);
    }
  }

  template <class Lane_T>
  void expect_dual_rows(const kernels::planar_forces<kernels::dual_lane<Lane_T>> &dual,
                        const kernels::planar_forces<Lane_T> &plain) {
    for (Eigen::Index i = 0; i < plain.fx_n.size(); ++i) {
      expect_same(dual.fx_n.value(i), plain.fx_n(i), i);
      expect_same(dual.fy_n.value(i), plain.fy_n(i), i);
      expect_same(dual.lat.dy_n.value(i), plain.lat.dy_n(i), i);
      expect_same(dual.lon.dx_n.value(i), plain.lon.dx_n(i), i);
    }
  }

  template <class Lane_T> void expect_dual_values_equal_plain(const mf::mf5x_forms &forms, mf::slip_mode mode) {
    const mf::mf5x_coefficients c = mf::test::fixture_s().coefficients;
    for (const std::vector<mf::test::point> &chunk : chunks_of<Lane_T>(planar_points())) {
      const kernels::input_lanes<Lane_T> in = make_inputs<Lane_T>(chunk);
      const Lane_T one = Lane_T::Constant(in.fz_n.size(), 1.0);
      const kernels::planar_forces<Lane_T> plain = kernels::mf5x_planar_forces(c, forms.combined, mode, in);
      const kernels::planar_forces<kernels::dual_lane<Lane_T>> by_alpha =
          kernels::mf5x_planar_forces(c, forms.combined, mode, kernels::seed_alpha(in, in.alpha_rad, one));
      const kernels::planar_forces<kernels::dual_lane<Lane_T>> by_kappa =
          kernels::mf5x_planar_forces(c, forms.combined, mode, kernels::seed_kappa(in, in.kappa, one));
      expect_dual_rows<Lane_T>(by_alpha, plain);
      expect_dual_rows<Lane_T>(by_kappa, plain);
    }
  }
} // namespace

TEST(mf5x_planar_forces, Uncombined_PointAndBlock_EqualsPureForcesBitwise) {
  const mf::mf5x_forms forms;
  expect_planar_matches_evaluate<point_lane>(forms, mf::slip_mode::UNCOMBINED, true);
  expect_planar_matches_evaluate<block_lane>(forms, mf::slip_mode::UNCOMBINED, true);
}

TEST(mf5x_planar_forces, CombinedCosine_PointAndBlock_EqualsEvaluateForcesBitwise) {
  const mf::mf5x_forms forms{};
  expect_planar_matches_evaluate<point_lane>(forms, mf::slip_mode::COMBINED, false);
  expect_planar_matches_evaluate<block_lane>(forms, mf::slip_mode::COMBINED, false);
}

TEST(mf5x_planar_forces, CombinedEllipse_PointAndBlock_EqualsEvaluateForcesBitwise) {
  const mf::mf5x_forms forms = ellipse_forms();
  expect_planar_matches_evaluate<point_lane>(forms, mf::slip_mode::COMBINED, false);
  expect_planar_matches_evaluate<block_lane>(forms, mf::slip_mode::COMBINED, false);
}

TEST(mf5x_planar_forces, DualLanes_AllModesAndMethods_ValueBitEqualsPlain) {
  const mf::mf5x_forms cosine{};
  const mf::mf5x_forms ellipse = ellipse_forms();
  expect_dual_values_equal_plain<point_lane>(cosine, mf::slip_mode::UNCOMBINED);
  expect_dual_values_equal_plain<point_lane>(cosine, mf::slip_mode::COMBINED);
  expect_dual_values_equal_plain<point_lane>(ellipse, mf::slip_mode::COMBINED);
  expect_dual_values_equal_plain<block_lane>(cosine, mf::slip_mode::UNCOMBINED);
  expect_dual_values_equal_plain<block_lane>(cosine, mf::slip_mode::COMBINED);
  expect_dual_values_equal_plain<block_lane>(ellipse, mf::slip_mode::COMBINED);
}

namespace {
  // combine() with the friction ellipse on `Lane_T` lanes (plain or dual) at the given operating points.
  template <class Lane_T>
  kernels::combined_forces<Lane_T> ellipse_combine(const mf::mf5x_coefficients &c,
                                                   const kernels::input_lanes<Lane_T> &in) {
    kernels::slip_state<Lane_T> state{};
    state.load = kernels::vertical_load(c, in.fz_n);
    state.lon = kernels::fx0(c, state.load, in.fz_n, in.kappa, in.gamma_rad);
    state.lat = kernels::fy0(c, state.load, in.fz_n, in.alpha_rad, in.gamma_rad);
    return kernels::combine(c, mf::combined_method::FRICTION_ELLIPSE, state, in);
  }

  template <class Lane_T> void expect_ellipse_combine_matches_closed_form() {
    const mf::mf5x_coefficients c = mf::test::fixture_s().coefficients;
    const std::vector<mf::test::point> points{pt(0.1, 0.05), pt(0.05, 0.1), pt(-0.07, -0.03)};
    for (const std::vector<mf::test::point> &chunk : chunks_of<Lane_T>(points)) {
      const kernels::input_lanes<Lane_T> in = make_inputs<Lane_T>(chunk);
      const Lane_T one = Lane_T::Constant(in.fz_n.size(), 1.0);
      const kernels::combined_forces<Lane_T> plain = ellipse_combine<Lane_T>(c, in);
      const auto dual = ellipse_combine<kernels::dual_lane<Lane_T>>(c, kernels::seed_alpha(in, in.alpha_rad, one));
      for (Eigen::Index i = 0; i < in.fz_n.size(); ++i) {
        const ellipse_reference e = ellipse_at(in.kappa(i), in.alpha_rad(i));
        expect_rel(plain.fx_n(i), e.fx, SLOPE_TOL, "plain fx");
        expect_rel(plain.fy_n(i), e.fy, SLOPE_TOL, "plain fy");
        expect_rel(dual.fy_n.derivative(i), e.dfy, SLOPE_TOL, "dual dfy_dalpha");
        expect_same(dual.fx_n.value(i), plain.fx_n(i), i);
        expect_same(dual.fy_n.value(i), plain.fy_n(i), i);
        expect_same(plain.svyk_n(i), 0.0, i);
        expect_same(dual.svyk_n.value(i), 0.0, i);
      }
    }
  }
} // namespace

TEST(mf5x_planar_forces, CombineFrictionEllipse_PlainAndDualPointLanes_EqualsClosedForm) {
  expect_ellipse_combine_matches_closed_form<point_lane>();
}

TEST(mf5x_planar_forces, CombineFrictionEllipse_PlainAndDualBlockLanes_EqualsClosedForm) {
  expect_ellipse_combine_matches_closed_form<block_lane>();
}

// ---------------------------------------------------------------------------------------------------------------
// seed_alpha, seed_kappa, peak_search_end
// ---------------------------------------------------------------------------------------------------------------

namespace {
  // Inputs with distinct values per field, so a mixed-up field shows.
  const std::vector<mf::test::point> SEED_POINTS{
      mf::test::point{.fz_n = 4000.0, .kappa = 0.1, .alpha_rad = 0.05, .gamma_rad = 0.03, .vx_mps = 12.0},
      mf::test::point{.fz_n = 3000.0, .kappa = -0.02, .alpha_rad = 0.2, .gamma_rad = -0.04, .vx_mps = 7.0},
  };

  // One dual lane: value equals `value` and derivative equals `derivative`, row by row.
  template <class Lane_T>
  void expect_dual(const kernels::dual_lane<Lane_T> &d, const Lane_T &value, const Lane_T &derivative) {
    for (Eigen::Index i = 0; i < value.size(); ++i) {
      expect_same(d.value(i), value(i), i);
      expect_same(d.derivative(i), derivative(i), i);
    }
  }

  template <class Lane_T>
  void expect_seeded(const kernels::input_lanes<kernels::dual_lane<Lane_T>> &d, const kernels::input_lanes<Lane_T> &in,
                     const Lane_T &swept, const Lane_T &seed, bool seed_alpha_axis) {
    const Lane_T zero = Lane_T::Zero(in.fz_n.size());
    expect_dual<Lane_T>(d.fz_n, in.fz_n, zero);
    expect_dual<Lane_T>(d.gamma_rad, in.gamma_rad, zero);
    expect_dual<Lane_T>(d.vx_mps, in.vx_mps, zero);
    expect_dual<Lane_T>(d.alpha_rad, seed_alpha_axis ? swept : in.alpha_rad, seed_alpha_axis ? seed : zero);
    expect_dual<Lane_T>(d.kappa, seed_alpha_axis ? in.kappa : swept, seed_alpha_axis ? zero : seed);
  }

  template <class Lane_T> void expect_seed(bool seed_alpha_axis) {
    for (const std::vector<mf::test::point> &chunk : chunks_of<Lane_T>(SEED_POINTS)) {
      const kernels::input_lanes<Lane_T> in = make_inputs<Lane_T>(chunk);
      const Eigen::Index n = in.fz_n.size();
      Lane_T swept = Lane_T::Zero(n);
      Lane_T seed = Lane_T::Zero(n);
      for (Eigen::Index i = 0; i < n; ++i) {
        swept(i) = 0.31 + (0.1 * static_cast<double>(i));
        seed(i) = 2.0 - (3.0 * static_cast<double>(i));
      }
      const kernels::input_lanes<kernels::dual_lane<Lane_T>> d =
          seed_alpha_axis ? kernels::seed_alpha(in, swept, seed) : kernels::seed_kappa(in, swept, seed);
      expect_seeded<Lane_T>(d, in, swept, seed, seed_alpha_axis);
    }
  }

  constexpr double PI_HALF = std::numbers::pi / 2.0;

  double end_of(double side, double lower, double upper, double extent) {
    return kernels::peak_search_end(mf::test::lane(side), lower, upper, extent)(0);
  }
} // namespace

TEST(seed_alpha, SeedsAlphaOnly_OtherDerivativesZero) {
  expect_seed<point_lane>(true);
  expect_seed<block_lane>(true);
}

TEST(seed_kappa, SeedsKappaOnly_OtherDerivativesZero) {
  expect_seed<point_lane>(false);
  expect_seed<block_lane>(false);
}

TEST(peak_search_end, InfiniteBounds_BothSides_SignedExtent) {
  expect_bits(end_of(1.0, -INF, INF, PI_HALF), PI_HALF, "+1");
  expect_bits(end_of(-1.0, -INF, INF, PI_HALF), -PI_HALF, "-1");
}

TEST(peak_search_end, FiniteBoundInsideExtent_ReturnsBound) {
  expect_bits(end_of(1.0, -INF, 0.1, PI_HALF), 0.1, "upper");
  expect_bits(end_of(-1.0, -0.1, INF, PI_HALF), -0.1, "lower");
}

TEST(peak_search_end, BoundBeyondExtent_ReturnsExtent) {
  expect_bits(end_of(1.0, -INF, 3.0, PI_HALF), PI_HALF, "upper");
  expect_bits(end_of(-1.0, -3.0, INF, PI_HALF), -PI_HALF, "lower");
}

TEST(peak_search_end, WrongSideBound_ReturnsZero) {
  expect_zero(end_of(1.0, -INF, -0.1, PI_HALF), "upper below 0");
  expect_zero(end_of(-1.0, 0.2, INF, PI_HALF), "lower above 0");
}

TEST(peak_search_end, BlockLane_MixedSides_PerLaneEnds) {
  block_lane side = block_lane::Zero(4);
  side << 1.0, -1.0, 1.0, -1.0;
  const block_lane ends = kernels::peak_search_end(side, -0.2, 0.3, 1.0);
  expect_bits(ends(0), 0.3, "lane 0");
  expect_bits(ends(1), -0.2, "lane 1");
  expect_bits(ends(2), 0.3, "lane 2");
  expect_bits(ends(3), -0.2, "lane 3");
}

// ---------------------------------------------------------------------------------------------------------------
// mf5x_characteristics
// ---------------------------------------------------------------------------------------------------------------

TEST(mf5x_characteristics, C1_PureLongitudinal_ClosedForms) {
  const chars r = run(setup{}, pt(0.1, 0.0));
  const curve cx = curve_x();
  const curve cy = curve_y();
  expect_rel(r.dfx_dsr, curve_slope(cx, 0.1) * gxa_of(0.0), SLOPE_TOL, "dfx_dsr");
  expect_rel(r.fx_over_sr, curve_force(cx, 0.1) / 0.1, SLOPE_TOL, "fx_over_sr");
  expect_rel(r.dfy_dsa, curve_slope(cy, 0.0) * gyk_of(0.1), SLOPE_TOL, "dfy_dsa");
  expect_bits(r.fy_over_sa, r.dfy_dsa, "secant at alpha 0 is the tangent");
  expect_rel(r.sr_peak, SQRT3 / curve_x().b, PEAK_TOL, "sr_peak");
  expect_rel(r.sa_peak, std::tan(std::numbers::pi / 2.6) / std::abs(curve_y().b), PEAK_TOL, "sa_peak");
  expect_rel(r.dy, FZ_N, SLOPE_TOL, "dy");
  expect_rel(r.dx, FZ_N, SLOPE_TOL, "dx");
  expect_rel(r.dfy_dsa, 5200.0 * curve_y().b / std::numbers::sqrt2, SLOPE_TOL, "5200*By/sqrt(2)");
  expect_rel(r.dfy_dsa, -39162.83710434, TABLE_TOL, "table dfy_dsa");
  expect_rel(r.dfx_dsr, 5151.900623800, TABLE_TOL, "table dfx_dsr");
  expect_rel(r.fx_over_sr, 39354.79640314, TABLE_TOL, "table fx_over_sr");
}

TEST(mf5x_characteristics, C2_PureLateral_ClosedForms) {
  const chars r = run(setup{}, pt(0.0, 0.05));
  const curve cx = curve_x();
  const curve cy = curve_y();
  expect_rel(r.dfy_dsa, curve_slope(cy, 0.05) * gyk_of(0.0), SLOPE_TOL, "dfy_dsa");
  expect_rel(r.fy_over_sa, curve_force(cy, 0.05) / 0.05, SLOPE_TOL, "fy_over_sa");
  expect_rel(r.dfx_dsr, curve_slope(cx, 0.0) * gxa_of(0.05), SLOPE_TOL, "dfx_dsr");
  expect_bits(r.fx_over_sr, r.dfx_dsr, "secant at kappa 0 is the tangent");
  expect_rel(r.dfx_dsr, 6000.0 * curve_x().b / std::sqrt(1.25), SLOPE_TOL, "6000*Bx/sqrt(1.25)");
  expect_rel(r.dfx_dsr, 71554.17526807, TABLE_TOL, "table dfx_dsr");
  expect_rel(r.dfy_dsa, -34707.69217580, TABLE_TOL, "table dfy_dsa");
  expect_rel(r.fy_over_sa, -47527.96304983, TABLE_TOL, "table fy_over_sa");
  expect_rel(r.sa_peak, std::tan(std::numbers::pi / 2.6) / std::abs(curve_y().b), PEAK_TOL, "sa_peak");
  expect_rel(r.sr_peak, SQRT3 / curve_x().b, PEAK_TOL, "sr_peak");
  expect_rel(r.dy, FZ_N, SLOPE_TOL, "dy");
  expect_rel(r.dx, FZ_N, SLOPE_TOL, "dx");
}

TEST(mf5x_characteristics, C3_Combined_WeightedSlopesSamePeaks) {
  const chars r = run(setup{}, pt(0.1, 0.1));
  const curve cx = curve_x();
  const curve cy = curve_y();
  expect_rel(r.dfx_dsr, curve_slope(cx, 0.1) / std::numbers::sqrt2, SLOPE_TOL, "dfx_dsr");
  expect_rel(r.dfy_dsa, curve_slope(cy, 0.1) / std::numbers::sqrt2, SLOPE_TOL, "dfy_dsa");
  expect_rel(r.fx_over_sr, curve_force(cx, 0.1) / std::numbers::sqrt2 / 0.1, SLOPE_TOL, "fx_over_sr");
  expect_rel(r.fy_over_sa, curve_force(cy, 0.1) / std::numbers::sqrt2 / 0.1, SLOPE_TOL, "fy_over_sa");
  expect_rel(r.dfx_dsr, 3642.943867088, TABLE_TOL, "table dfx_dsr");
  expect_rel(r.dfy_dsa, -8938.282121032, TABLE_TOL, "table dfy_dsa");
  expect_rel(r.sr_peak, SQRT3 / cx.b, PEAK_TOL, "sr_peak");
  expect_rel(r.sa_peak, std::tan(std::numbers::pi / 2.6) / std::abs(cy.b), PEAK_TOL, "sa_peak");
}

TEST(mf5x_characteristics, C4_ZeroSlips_SecantsEqualSlopesAtOrigin) {
  const chars r = run(setup{}, pt(0.0, 0.0));
  expect_rel(r.dfx_dsr, 6000.0 * curve_x().b, SLOPE_TOL, "dfx_dsr");
  expect_rel(r.dfy_dsa, 5200.0 * curve_y().b, SLOPE_TOL, "dfy_dsa");
  expect_bits(r.fx_over_sr, r.dfx_dsr, "fx_over_sr");
  expect_bits(r.fy_over_sa, r.dfy_dsa, "fy_over_sa");
  expect_rel(r.dfx_dsr, 79999.99998667, TABLE_TOL, "table dfx_dsr");
  expect_rel(r.dfy_dsa, -55384.61537396, TABLE_TOL, "table dfy_dsa");
  expect_rel(r.sa_peak, std::tan(std::numbers::pi / 2.6) / std::abs(curve_y().b), PEAK_TOL, "sa_peak");
  expect_rel(r.sr_peak, SQRT3 / curve_x().b, PEAK_TOL, "sr_peak");
}

TEST(mf5x_characteristics, C5_NegativeSlipAngle_NegativeSidePeak) {
  const chars r = run(setup{}, pt(0.0, -0.05));
  expect_rel(r.sa_peak, -std::tan(std::numbers::pi / 2.6) / std::abs(curve_y().b), PEAK_TOL, "sa_peak");
  expect_rel(r.dfy_dsa, curve_slope(curve_y(), 0.05), SLOPE_TOL, "dfy_dsa is even in alpha");
  expect_rel(r.fy_over_sa, curve_force(curve_y(), 0.05) / 0.05, SLOPE_TOL, "fy_over_sa is even in alpha");
  expect_rel(r.sr_peak, SQRT3 / curve_x().b, PEAK_TOL, "sr_peak on the positive side");
}

TEST(mf5x_characteristics, C13_NegativeSlipRatio_NegativeSidePeak) {
  const chars r = run(setup{}, pt(-0.1, 0.0));
  expect_rel(r.sr_peak, -SQRT3 / curve_x().b, PEAK_TOL, "sr_peak");
  expect_rel(r.dfx_dsr, curve_slope(curve_x(), 0.1), SLOPE_TOL, "dfx_dsr is even in kappa");
  expect_rel(r.fx_over_sr, curve_force(curve_x(), 0.1) / 0.1, SLOPE_TOL, "fx_over_sr is even in kappa");
  expect_rel(r.dfy_dsa, -39162.83710434, TABLE_TOL, "dfy_dsa");
  expect_bits(r.fy_over_sa, r.dfy_dsa, "fy_over_sa");
}

TEST(mf5x_characteristics, C6_ShapeAtMostOne_PeaksNaN) {
  setup lateral;
  lateral.params.coefficients.PCY1 = 1.0;
  const chars a = run(lateral, pt(0.1, 0.05));
  EXPECT_TRUE(std::isnan(a.sa_peak));
  expect_rel(a.sr_peak, SQRT3 / curve_x().b, PEAK_TOL, "sr_peak");
  expect_rel(a.dfy_dsa, curve_slope(curve_y(1.0), 0.05) * gyk_of(0.1), SLOPE_TOL, "dfy_dsa");
  expect_rel(a.fy_over_sa, curve_force(curve_y(1.0), 0.05) * gyk_of(0.1) / 0.05, SLOPE_TOL, "fy_over_sa");
  expect_rel(a.dfy_dsa, -21766.78012002, TABLE_TOL, "table dfy_dsa");

  setup longitudinal;
  longitudinal.params.coefficients.PCX1 = 0.9;
  const chars b = run(longitudinal, pt(0.1, 0.05));
  EXPECT_TRUE(std::isnan(b.sr_peak));
  expect_rel(b.sa_peak, std::tan(std::numbers::pi / 2.6) / std::abs(curve_y().b), PEAK_TOL, "sa_peak");
  expect_rel(b.dfx_dsr, curve_slope(curve_x(0.9), 0.1) * gxa_of(0.05), SLOPE_TOL, "dfx_dsr");
  expect_rel(b.fx_over_sr, curve_force(curve_x(0.9), 0.1) * gxa_of(0.05) / 0.1, SLOPE_TOL, "fx_over_sr");
  expect_rel(b.dfx_dsr, 6170.842201907, TABLE_TOL, "table dfx_dsr");
}

TEST(mf5x_characteristics, C7_ClampedSlipAngle_ZeroSlopeClampedSecant) {
  const chars r = run(ranged_setup(), pt(0.0, 2.0));
  expect_zero(r.dfy_dsa, "dfy_dsa");
  expect_rel(r.fy_over_sa, curve_force(curve_y(), 1.5) / 2.0, SLOPE_TOL, "fy_over_sa");
  expect_rel(r.fy_over_sa, -1849.837475483, TABLE_TOL, "table fy_over_sa");
  expect_rel(r.dfx_dsr, 6000.0 * curve_x().b / std::sqrt(226.0), SLOPE_TOL, "dfx_dsr");
  expect_bits(r.fx_over_sr, r.dfx_dsr, "fx_over_sr");
  expect_rel(r.dfx_dsr, 5321.520841015, TABLE_TOL, "table dfx_dsr");
  expect_rel(r.sa_peak, std::tan(std::numbers::pi / 2.6) / std::abs(curve_y().b), PEAK_TOL, "sa_peak");
  expect_rel(r.sr_peak, SQRT3 / curve_x().b, PEAK_TOL, "sr_peak");
}

TEST(mf5x_characteristics, ClampedSlipRatio_ZeroSlope) {
  const chars r = run(ranged_setup(), pt(10.0, 0.0));
  expect_zero(r.dfx_dsr, "dfx_dsr");
  expect_rel(r.fx_over_sr, curve_force(curve_x(), 1.0) / 10.0, SLOPE_TOL, "fx_over_sr");
  expect_bits(r.fy_over_sa, r.dfy_dsa, "fy_over_sa");
  expect_rel(r.sr_peak, SQRT3 / curve_x().b, PEAK_TOL, "sr_peak");
}

TEST(mf5x_characteristics, C8_NoContact_ZeroSlopesZeroPeaksNaN) {
  const setup unranged{};
  setup unloaded = ranged_setup();
  unloaded.params.limits(mf::mf5x::FZMIN) = -1000.0;
  unloaded.params.limits(mf::mf5x::FZMAX) = 0.0;
  const std::vector<mf::test::point> rows{pt(0.1, 0.05, 0.0), pt(0.0, 0.0, 0.0), pt(0.1, 0.05, -100.0),
                                          pt(0.0, 0.0, -100.0)};
  const auto check = [](const chars &r) {
    expect_bits(r.dfy_dsa, 0.0, "dfy_dsa");
    expect_bits(r.dfx_dsr, 0.0, "dfx_dsr");
    expect_bits(r.fy_over_sa, 0.0, "fy_over_sa");
    expect_bits(r.fx_over_sr, 0.0, "fx_over_sr");
    expect_bits(r.dy, 0.0, "dy");
    expect_bits(r.dx, 0.0, "dx");
    EXPECT_TRUE(std::isnan(r.sa_peak));
    EXPECT_TRUE(std::isnan(r.sr_peak));
  };
  for (const mf::test::point &p : rows) {
    check(run(unranged, p));
  }
  check(run(unloaded, pt(0.1, 0.05, 100.0)));
  check(run(unloaded, pt(0.0, 0.0, 100.0)));
}

TEST(mf5x_characteristics, C9_CombinedCosineWithByAndSvyk_EqualsClosedFormDerivative) {
  setup s;
  mf::mf5x_coefficients &c = s.params.coefficients;
  c.RBY2 = 5.0;
  c.RBY3 = 0.02;
  c.RVY1 = 0.05;
  c.RVY4 = 2.0;
  c.RVY5 = 1.0;
  c.RVY6 = 10.0;
  constexpr double kappa = 0.05;
  constexpr double alpha = 0.05;
  const mf::test::point p = pt(kappa, alpha);
  const chars r = run(s, p);

  const curve cx = curve_x();
  const curve cy = curve_y();
  const double fy0 = curve_force(cy, alpha);
  const double dfy0 = curve_slope(cy, alpha);
  const double w = 5.0 * (alpha - 0.02);
  const double byk = 10.0 / std::sqrt(1.0 + (w * w));
  const double dbyk = 10.0 * -w * 5.0 * std::pow(1.0 + (w * w), -1.5);
  const double bk = byk * kappa;
  const double gyk = 1.0 / std::sqrt(1.0 + (bk * bk));
  const double dgyk_dbyk = -byk * kappa * kappa * std::pow(1.0 + (bk * bk), -1.5);
  const double sine_k = std::sin(std::atan(10.0 * kappa));
  const double svyk = 200.0 / std::sqrt(1.0 + (4.0 * alpha * alpha)) * sine_k;
  const double dsvyk = 200.0 * -4.0 * alpha * std::pow(1.0 + (4.0 * alpha * alpha), -1.5) * sine_k;
  expect_rel(r.dfy_dsa, (dfy0 * gyk) + (fy0 * dgyk_dbyk * dbyk) + dsvyk, SLOPE_TOL, "dfy_dsa");
  expect_rel(r.fy_over_sa, ((fy0 * gyk) + svyk) / alpha, SLOPE_TOL, "fy_over_sa");
  expect_rel(r.dfx_dsr, curve_slope(cx, kappa) * gxa_of(alpha), SLOPE_TOL, "dfx_dsr");
  expect_rel(r.fx_over_sr, curve_force(cx, kappa) * gxa_of(alpha) / kappa, SLOPE_TOL, "fx_over_sr");
  expect_rel(r.dfy_dsa, -31436.63872597, TABLE_TOL, "table dfy_dsa");
  expect_rel(r.dfx_dsr, 31486.30521554, TABLE_TOL, "table dfx_dsr");
  expect_rel(r.sa_peak, 0.3972105943213, PEAK_TOL, "table sa_peak");
  expect_rel(r.sr_peak, SQRT3 / cx.b, PEAK_TOL, "sr_peak");
  expect_lateral_peak(s, p, r.sa_peak);
  expect_longitudinal_peak(s, p, r.sr_peak);
}

TEST(mf5x_characteristics, C10_FrictionEllipse_EqualsClosedFormDerivatives) {
  const setup s = ellipse_setup();
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
    const mf::test::point p = pt(row.kappa, row.alpha);
    const chars r = run(s, p);
    const ellipse_reference e = ellipse_at(row.kappa, row.alpha);
    expect_rel(r.dfx_dsr, e.dfx, SLOPE_TOL, "dfx_dsr");
    expect_rel(r.dfy_dsa, e.dfy, SLOPE_TOL, "dfy_dsa");
    expect_rel(r.fx_over_sr, e.fx / row.kappa, SLOPE_TOL, "fx_over_sr");
    expect_rel(r.fy_over_sa, e.fy / row.alpha, SLOPE_TOL, "fy_over_sa");
    expect_rel(r.sr_peak, row.sr_peak, PEAK_TOL, "table sr_peak");
    expect_rel(r.sa_peak, row.sa_peak, PEAK_TOL, "table sa_peak");
    expect_lateral_peak(s, p, r.sa_peak);
    expect_longitudinal_peak(s, p, r.sr_peak);
  }
  const chars first = run(s, pt(0.1, 0.05));
  expect_rel(first.dfx_dsr, 10599.59198473, TABLE_TOL, "table dfx_dsr");
  expect_rel(first.dfy_dsa, -27161.23002030, TABLE_TOL, "table dfy_dsa");
}

TEST(mf5x_characteristics, C14_FrictionEllipseKappaZero_DirectionalLimit) {
  const chars r = run(ellipse_setup(), pt(0.0, 0.05));
  const double slope_x0 = curve_slope(curve_x(), 0.0);
  const double rx_limit = FZ_N / std::sqrt((FZ_N * FZ_N) + (slope_x0 * std::sin(0.05) * slope_x0 * std::sin(0.05)));
  expect_rel(r.dfx_dsr, slope_x0 * rx_limit, SLOPE_TOL, "dfx_dsr");
  expect_bits(r.fx_over_sr, r.dfx_dsr, "fx_over_sr");
  expect_rel(r.dfx_dsr, 56580.32736, 1e-9, "table dfx_dsr");
  expect_rel(r.dfy_dsa, curve_slope(curve_y(), 0.05), SLOPE_TOL, "dfy_dsa");
  expect_rel(r.fy_over_sa, curve_force(curve_y(), 0.05) / 0.05, SLOPE_TOL, "fy_over_sa");
  expect_rel(r.sr_peak, 0.1675533315318, PEAK_TOL, "table sr_peak");
  expect_rel(r.sa_peak, 0.2475646539106, PEAK_TOL, "table sa_peak");
}

TEST(mf5x_characteristics, C15_FrictionEllipseAlphaZero_DirectionalLimit) {
  const chars r = run(ellipse_setup(), pt(0.1, 0.0));
  const double slope_y0 = curve_slope(curve_y(), 0.0);
  const double ry_limit = FZ_N / std::sqrt((FZ_N * FZ_N) + (0.1 * slope_y0 * 0.1 * slope_y0));
  expect_rel(r.dfy_dsa, slope_y0 * ry_limit, SLOPE_TOL, "dfy_dsa");
  expect_bits(r.fy_over_sa, r.dfy_dsa, "fy_over_sa");
  expect_rel(r.dfy_dsa, -32427.16913, 1e-9, "table dfy_dsa");
  expect_rel(r.dfx_dsr, curve_slope(curve_x(), 0.1), SLOPE_TOL, "dfx_dsr");
  expect_rel(r.fx_over_sr, curve_force(curve_x(), 0.1) / 0.1, SLOPE_TOL, "fx_over_sr");
  expect_rel(r.sa_peak, 0.4281129082399, PEAK_TOL, "table sa_peak");
  expect_rel(r.sr_peak, 0.1299038105893, PEAK_TOL, "table sr_peak");
}

TEST(mf5x_characteristics, FrictionEllipseDoubleOrigin_PureSlopes) {
  const chars r = run(ellipse_setup(), pt(0.0, 0.0));
  expect_rel(r.dfx_dsr, 6000.0 * curve_x().b, SLOPE_TOL, "dfx_dsr");
  expect_rel(r.dfy_dsa, 5200.0 * curve_y().b, SLOPE_TOL, "dfy_dsa");
  expect_bits(r.fx_over_sr, r.dfx_dsr, "fx_over_sr");
  expect_bits(r.fy_over_sa, r.dfy_dsa, "fy_over_sa");
}

TEST(mf5x_characteristics, C17_FrictionEllipseKink_RightDerivative) {
  setup s = ellipse_setup();
  s.params.coefficients.PVX1 = 0.01;
  constexpr double alpha = 0.05;
  const kernels::slip_state<point_lane> probe = mf::test::slip_state_at(s.params.coefficients, pt(0.0, alpha));
  const double kappa = -(probe.lon.svx_n(0) / kernels::add_epsilon(probe.lon.kx_n)(0));
  const mf::test::point p = pt(kappa, alpha);
  const kernels::slip_state<point_lane> state = mf::test::slip_state_at(s.params.coefficients, p);
  const chars r = run(s, p);

  const double fx0 = state.lon.fx0_n(0);
  const double svx = state.lon.svx_n(0);
  const double mym = state.lat.dy_n(0);
  const double alpha_c = alpha + state.lat.shy(0) + (state.lat.svy_n(0) / kernels::add_epsilon(state.lat.ky_n)(0));
  expect_rel(r.dfx_dsr, fx0 * mym / (std::abs(fx0 - svx) * std::abs(std::sin(alpha_c))), SLOPE_TOL, "dfx_dsr");
  expect_rel(r.dfx_dsr, 2.519564294794, 1e-9, "table dfx_dsr");
  expect_zero(r.fx_over_sr, "fx_over_sr");
  expect_rel(r.dfy_dsa, curve_slope(curve_y(), alpha), SLOPE_TOL, "dfy_dsa");
  expect_rel(r.fy_over_sa, curve_force(curve_y(), alpha) / alpha, SLOPE_TOL, "fy_over_sa");
}

TEST(mf5x_characteristics, C16_SideAsymmetricCurvature_PeakPerSide) {
  setup s;
  s.params.coefficients.PEX1 = 0.5;
  s.params.coefficients.PEX4 = 0.4;
  const double bx = curve_x().b;
  const chars positive = run(s, pt(0.1, 0.0));
  const chars negative = run(s, pt(-0.1, 0.0));
  expect_rel(positive.sr_peak, stationary_u(0.3) / bx, PEAK_TOL, "positive sr_peak");
  expect_rel(negative.sr_peak, -stationary_u(0.7) / bx, PEAK_TOL, "negative sr_peak");
  expect_rel(positive.sr_peak, 0.1499907427154, 1e-9, "table positive");
  expect_rel(negative.sr_peak, -0.2164861904383, 1e-9, "table negative");
  expect_rel(positive.dfx_dsr, curve_slope(curve_x(1.5, 0.3), 0.1), SLOPE_TOL, "positive dfx_dsr");
  expect_rel(negative.dfx_dsr, curve_slope(curve_x(1.5, 0.7), -0.1), SLOPE_TOL, "negative dfx_dsr");
  expect_rel(positive.fx_over_sr, curve_force(curve_x(1.5, 0.3), 0.1) / 0.1, SLOPE_TOL, "positive fx_over_sr");
  expect_rel(negative.fx_over_sr, curve_force(curve_x(1.5, 0.7), -0.1) / -0.1, SLOPE_TOL, "negative fx_over_sr");
}

TEST(mf5x_characteristics, C11_NegativeCurvature_PeakSatisfiesERelation) {
  setup s;
  s.params.coefficients.PEY1 = -0.5;
  const chars r = run(s, pt(0.0, 0.05));
  const double tangent = std::tan(std::numbers::pi / 2.6);
  const double u = std::abs(curve_y().b) * r.sa_peak;
  const double e = (u - tangent) / (u - std::atan(u));
  expect_rel(e, -0.5, PEAK_TOL, "E relation");
  expect_rel(r.sa_peak, 0.2004969372285, PEAK_TOL, "table sa_peak");
  expect_rel(r.dfy_dsa, curve_slope(curve_y(1.3, -0.5), 0.05), SLOPE_TOL, "dfy_dsa");
  expect_rel(r.fy_over_sa, curve_force(curve_y(1.3, -0.5), 0.05) / 0.05, SLOPE_TOL, "fy_over_sa");
}

TEST(mf5x_characteristics, C12_PeakBeyondRange_NaN) {
  setup s = ranged_setup();
  s.params.limits(mf::mf5x::ALPMAX) = 0.1;
  const chars r = run(s, pt(0.0, 0.05));
  EXPECT_TRUE(std::isnan(r.sa_peak));
  expect_rel(r.sr_peak, SQRT3 / curve_x().b, PEAK_TOL, "sr_peak");
  expect_rel(r.dfy_dsa, curve_slope(curve_y(), 0.05), SLOPE_TOL, "dfy_dsa");
}

TEST(mf5x_characteristics, Uncombined_PureCurveSlopesAndPeaks) {
  const chars r = run(uncombined_setup(), pt(0.1, 0.1));
  expect_rel(r.dfx_dsr, curve_slope(curve_x(), 0.1), SLOPE_TOL, "dfx_dsr");
  expect_rel(r.dfy_dsa, curve_slope(curve_y(), 0.1), SLOPE_TOL, "dfy_dsa");
  expect_rel(r.fx_over_sr, curve_force(curve_x(), 0.1) / 0.1, SLOPE_TOL, "fx_over_sr");
  expect_rel(r.fy_over_sa, curve_force(curve_y(), 0.1) / 0.1, SLOPE_TOL, "fy_over_sa");
  expect_rel(r.sr_peak, SQRT3 / curve_x().b, PEAK_TOL, "sr_peak");
  expect_rel(r.sa_peak, std::tan(std::numbers::pi / 2.6) / std::abs(curve_y().b), PEAK_TOL, "sa_peak");

  const chars at_origin = run(uncombined_setup(), pt(0.0, 0.0));
  expect_bits(at_origin.fy_over_sa, at_origin.dfy_dsa, "fy_over_sa");
  expect_bits(at_origin.fx_over_sr, at_origin.dfx_dsr, "fx_over_sr");
}

TEST(mf5x_characteristics, CombinedAtZeroKappa_EqualsUncombinedLateral) {
  const chars combined = run(setup{}, pt(0.0, 0.05));
  const chars pure = run(uncombined_setup(), pt(0.0, 0.05));
  expect_rel(combined.dfy_dsa, pure.dfy_dsa, SLOPE_TOL, "dfy_dsa");
  expect_rel(combined.fy_over_sa, pure.fy_over_sa, SLOPE_TOL, "fy_over_sa");
  expect_rel(combined.sa_peak, pure.sa_peak, PEAK_TOL, "sa_peak");
}
