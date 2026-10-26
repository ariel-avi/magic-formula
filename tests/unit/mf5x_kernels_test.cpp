#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <gtest/gtest.h>
#include <initializer_list>
#include <magic_formula/detail/lanes.hpp>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_forms.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <numbers>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
  namespace kernels = mf::detail;
  using coeffs = mf::mf5x_coefficients;
  using member_ptr = double coeffs::*;
  using kernels::block_lane;
  using kernels::point_lane;
  using mf::test::bits;
  using mf::test::lane;
  using mf::test::lanes_of;
  using mf::test::near_scaled;
  using mf::test::printed_tol;
  using mf::test::rel_near_result;
  using mf::test::REL_TOL_CLOSED_FORM;

  using point_inputs = kernels::input_lanes<point_lane>;
  using block_inputs = kernels::input_lanes<block_lane>;
  using point_load = kernels::load_terms<point_lane>;
  using point_lon = kernels::longitudinal_terms<point_lane>;
  using point_lat = kernels::lateral_terms<point_lane>;
  using point_aln = kernels::aligning_terms<point_lane>;
  using point_state = kernels::slip_state<point_lane>;
  using point_forces = kernels::combined_forces<point_lane>;

  constexpr double FZ_NOMINAL = 4000.0;
  constexpr double SPEED_MPS = 10.0;
  // Scales of the block-vs-point comparison: forces [N], moments [N*m] (FNOMIN*R0 of fixture S), the rest.
  constexpr double FORCE_SCALE_N = 4000.0;
  constexpr double MOMENT_SCALE_NM = 1200.0;
  constexpr double UNIT_SCALE = 1.0;

  // Fixture S coefficients with the listed members replaced.
  coeffs s_with(std::initializer_list<std::pair<member_ptr, double>> overrides) {
    coeffs c = mf::test::fixture_s().coefficients;
    for (const auto &[member, value] : overrides) {
      c.*member = value;
    }
    return c;
  }

  // Fixture S with every family non-trivial: E crosses 1 with the load, shifts and camber terms are non-zero.
  coeffs rich_coefficients() {
    coeffs c = s_with({});
    c.PEX1 = 1.0;
    c.PEX2 = 0.5;
    c.PEX3 = 0.1;
    c.PEX4 = 0.3;
    c.PDX2 = -0.1;
    c.PDX3 = 2.0;
    c.PKX2 = 1.0;
    c.PKX3 = 0.3;
    c.PHX1 = 0.001;
    c.PHX2 = 0.002;
    c.PVX1 = 0.01;
    c.PVX2 = 0.02;
    c.PEY1 = 1.0;
    c.PEY2 = 0.5;
    c.PEY3 = 0.3;
    c.PEY4 = 0.1;
    c.PDY2 = -0.1;
    c.PDY3 = 1.0;
    c.PKY3 = 0.5;
    c.PHY1 = 0.002;
    c.PHY2 = 0.004;
    c.PHY3 = 0.1;
    c.PVY1 = 0.01;
    c.PVY2 = 0.02;
    c.PVY3 = 0.1;
    c.PVY4 = 0.05;
    c.QBZ2 = 1.0;
    c.QBZ4 = 0.5;
    c.QBZ5 = 0.5;
    c.QBZ9 = 2.0;
    c.QBZ10 = 0.5;
    c.QDZ2 = 0.2;
    c.QDZ3 = 0.5;
    c.QDZ4 = 1.0;
    c.QDZ6 = 0.01;
    c.QDZ7 = 0.02;
    c.QDZ8 = 0.02;
    c.QDZ9 = 0.04;
    c.QEZ1 = 1.0;
    c.QEZ2 = 0.5;
    c.QEZ4 = 0.2;
    c.QEZ5 = 0.1;
    c.QHZ1 = 0.01;
    c.QHZ2 = 0.02;
    c.QHZ3 = 0.05;
    c.QHZ4 = 0.1;
    c.REX1 = 1.0;
    c.REX2 = 0.5;
    c.RHX1 = 0.02;
    c.REY1 = 1.0;
    c.REY2 = 0.5;
    c.RHY1 = 0.02;
    c.RBX2 = 5.0;
    c.RBY2 = 5.0;
    c.RBY3 = 0.01;
    c.RVY1 = 0.1;
    c.RVY2 = 0.05;
    c.RVY3 = 0.2;
    c.RVY4 = 2.0;
    c.RVY5 = 1.0;
    c.RVY6 = 10.0;
    c.SSZ1 = 0.01;
    c.SSZ2 = 0.02;
    c.SSZ3 = 0.03;
    c.SSZ4 = 0.04;
    c.QSX1 = 0.01;
    c.QSX2 = 0.5;
    c.QSX3 = 0.1;
    c.QSY2 = 0.05;
    c.QSY3 = 0.1;
    c.QSY4 = 0.01;
    return c;
  }

  mf::test::point make_point(double fz, double kappa, double alpha, double gamma) {
    mf::test::point p;
    p.fz_n = fz;
    p.kappa = kappa;
    p.alpha_rad = alpha;
    p.gamma_rad = gamma;
    p.vx_mps = SPEED_MPS;
    return p;
  }

  // ---- point-lane wrappers returning plain doubles -----------------------------------------------------------------

  double sgn_plus_of(double x) {
    return kernels::sgn_plus(lane(x))(0);
  }

  double add_epsilon_of(double d) {
    return kernels::add_epsilon(lane(d))(0);
  }

  double limit_curvature_of(double e) {
    return kernels::limit_curvature(lane(e))(0);
  }

  double sine_mf_of(double b, double shape_c, double d, double e, double x) {
    return kernels::sine_magic_formula(lane(b), shape_c, lane(d), lane(e), lane(x))(0);
  }

  double cosine_mf_of(double b, double shape_c, double e, double x) {
    return kernels::cosine_magic_formula(lane(b), shape_c, lane(e), lane(x))(0);
  }

  point_load load_at(const coeffs &c, double fz) {
    return kernels::vertical_load(c, lane(fz));
  }

  double mu_y_of(const coeffs &c, const point_load &load, double gamma_y) {
    return kernels::mu_y(c, load, lane(gamma_y))(0);
  }

  point_lon fx0_at(const coeffs &c, double fz, double kappa, double gamma = 0.0) {
    const point_lane fz_lane = lane(fz);
    return kernels::fx0(c, kernels::vertical_load(c, fz_lane), fz_lane, lane(kappa), lane(gamma));
  }

  point_lat fy0_at(const coeffs &c, double fz, double alpha, double gamma = 0.0) {
    const point_lane fz_lane = lane(fz);
    return kernels::fy0(c, kernels::vertical_load(c, fz_lane), fz_lane, lane(alpha), lane(gamma));
  }

  point_state state_at(const coeffs &c, double fz, double kappa, double alpha, double gamma = 0.0) {
    return kernels::evaluate_slip_state(c, lanes_of(make_point(fz, kappa, alpha, gamma)));
  }

  point_forces combined_at(const coeffs &c, double fz, double kappa, double alpha) {
    const point_inputs p = lanes_of(make_point(fz, kappa, alpha, 0.0));
    return kernels::cosine_combined_forces(c, kernels::evaluate_slip_state(c, p), p);
  }

  double t_of(const point_aln &aln, double x, double cos_factor) {
    return kernels::t(aln, lane(x), lane(cos_factor))(0);
  }

  double mzr_of(const point_aln &aln, double x, double cos_alpha) {
    return kernels::mzr(aln, lane(x), lane(cos_alpha))(0);
  }

  double alpha_eq_of(double a, double kappa, double kx_n, double ky_guarded_n) {
    return kernels::alpha_eq(lane(a), lane(kappa), lane(kx_n), lane(ky_guarded_n))(0);
  }

  double gxa_of(const coeffs &c, const point_load &load, double kappa, double alpha) {
    return kernels::gxa(c, load, lane(kappa), lane(alpha))(0);
  }

  double gyk_of(const coeffs &c, const point_load &load, double kappa, double alpha) {
    return kernels::gyk(c, load, lane(kappa), lane(alpha))(0);
  }

  double svyk_of(const coeffs &c, const point_load &load, double mu_y_value, double fz, double kappa, double alpha,
                 double gamma) {
    return kernels::svyk(c, load, lane(mu_y_value), lane(fz), lane(kappa), lane(alpha), lane(gamma))(0);
  }

  double s_of(const coeffs &c, const point_load &load, double fy, double gamma) {
    return kernels::s(c, load, lane(fy), lane(gamma))(0);
  }

  double mx_of(const coeffs &c, double fz, double fy, double gamma) {
    return kernels::mx(c, lane(fz), lane(fy), lane(gamma))(0);
  }

  double my_of(const coeffs &c, double fz, double fx, double vx) {
    return kernels::my(c, lane(fz), lane(fx), lane(vx))(0);
  }

  struct ratio_values {
    double rx;
    double ry;
  };

  ratio_values ratios_of(const point_state &s, double kappa, double alpha) {
    const kernels::ellipse_ratios<point_lane> r = kernels::friction_ellipse_ratios(s, lane(kappa), lane(alpha));
    return ratio_values{.rx = r.rx(0), .ry = r.ry(0)};
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

  // The friction-ellipse form with |mu_act| and the 1/Fz factors (an independent reference). beta is
  // computed as atan2(|alpha*|, |kappa_c|), which equals acos(|kappa_c|/sqrt(kappa_c^2 + alpha*^2)) without the
  // precision loss of acos near 1.
  ratio_values printed_form_ratios(const point_state &s, double fz, double kappa, double alpha) {
    const auto square = [](double v) { return v * v; };
    const double kappa_c = kappa + s.lon.shx(0) + (s.lon.svx_n(0) / add_epsilon_of(s.lon.kx_n(0)));
    const double alpha_c = alpha + s.lat.shy(0) + (s.lat.svy_n(0) / add_epsilon_of(s.lat.ky_n(0)));
    const double alpha_star = std::sin(alpha_c);
    const double tan_beta = std::tan(std::atan2(std::abs(alpha_star), std::abs(kappa_c)));
    const double mu_x_act = std::abs(s.lon.fx0_n(0) - s.lon.svx_n(0)) / fz;
    const double mu_y_act = std::abs(s.lat.fy0_n(0) - s.lat.svy_n(0)) / fz;
    const double mu_x_max = std::abs(s.lon.dx_n(0)) / fz;
    const double mu_y_max = std::abs(s.lat.dy_n(0)) / fz;
    const double mu_x_red = 1.0 / std::sqrt(square(1.0 / mu_x_act) + square(tan_beta / mu_y_max));
    const double mu_y_red = tan_beta / std::sqrt(square(1.0 / mu_x_max) + square(tan_beta / mu_y_act));
    return ratio_values{.rx = mu_x_red / mu_x_act, .ry = mu_y_red / mu_y_act};
  }

  // ---- block-vs-point machinery ---------------------------------------------------------------------------------

  // The values of one output column of an evaluation (one value per row) and the scale of its comparison.
  struct sampled {
    std::vector<double> values;
    double scale;
  };
  using samples = std::vector<sampled>;

  template <class Lane_T> void append_lane(samples &out, const Lane_T &x, double scale) {
    sampled column{.values = {}, .scale = scale};
    for (Eigen::Index i = 0; i < x.size(); ++i) {
      column.values.push_back(x(i));
    }
    out.push_back(std::move(column));
  }

  template <class Lane_T> void append_load(samples &out, const kernels::load_terms<Lane_T> &t) {
    append_lane(out, t.dfz, UNIT_SCALE);
  }

  template <class Lane_T> void append_lon(samples &out, const kernels::longitudinal_terms<Lane_T> &t) {
    append_lane(out, t.kappa_x, UNIT_SCALE);
    append_lane(out, t.gamma_x, UNIT_SCALE);
    append_lane(out, t.mu_x, UNIT_SCALE);
    append_lane(out, t.dx_n, FORCE_SCALE_N);
    append_lane(out, t.ex, UNIT_SCALE);
    append_lane(out, t.kx_n, FORCE_SCALE_N);
    append_lane(out, t.bx, UNIT_SCALE);
    append_lane(out, t.shx, UNIT_SCALE);
    append_lane(out, t.svx_n, FORCE_SCALE_N);
    append_lane(out, t.fx0_n, FORCE_SCALE_N);
  }

  template <class Lane_T> void append_lat(samples &out, const kernels::lateral_terms<Lane_T> &t) {
    append_lane(out, t.alpha_y, UNIT_SCALE);
    append_lane(out, t.gamma_y, UNIT_SCALE);
    append_lane(out, t.mu_y, UNIT_SCALE);
    append_lane(out, t.dy_n, FORCE_SCALE_N);
    append_lane(out, t.ey, UNIT_SCALE);
    append_lane(out, t.ky0_n, FORCE_SCALE_N);
    append_lane(out, t.ky_n, FORCE_SCALE_N);
    append_lane(out, t.by, UNIT_SCALE);
    append_lane(out, t.shy, UNIT_SCALE);
    append_lane(out, t.svy_n, FORCE_SCALE_N);
    append_lane(out, t.fy0_n, FORCE_SCALE_N);
  }

  template <class Lane_T> void append_aln(samples &out, const kernels::aligning_terms<Lane_T> &t) {
    append_lane(out, t.gamma_z, UNIT_SCALE);
    append_lane(out, t.alpha_t, UNIT_SCALE);
    append_lane(out, t.alpha_r, UNIT_SCALE);
    append_lane(out, t.sht, UNIT_SCALE);
    append_lane(out, t.shf, UNIT_SCALE);
    append_lane(out, t.bt, UNIT_SCALE);
    append_lane(out, t.dt_m, UNIT_SCALE);
    append_lane(out, t.et, UNIT_SCALE);
    append_lane(out, t.br, UNIT_SCALE);
    append_lane(out, t.dr_nm, MOMENT_SCALE_NM);
  }

  template <class Lane_T> void append_state(samples &out, const kernels::slip_state<Lane_T> &s) {
    append_load(out, s.load);
    append_lon(out, s.lon);
    append_lat(out, s.lat);
    append_aln(out, s.aln);
  }

  void expect_same_samples(const samples &actual, const samples &expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t k = 0; k < actual.size(); ++k) {
      ASSERT_EQ(actual[k].values.size(), expected[k].values.size());
      for (std::size_t j = 0; j < actual[k].values.size(); ++j) {
        EXPECT_EQ(bits(actual[k].values[j]), bits(expected[k].values[j])) << "column " << k << " value " << j;
      }
    }
  }

  // The operating points of `regime_grid(BLOCK_ROWS)`; with `contact_only`, only the rows with Fz > 0.
  std::vector<mf::test::point> grid_points(bool contact_only) {
    const Eigen::MatrixXd grid = mf::test::regime_grid(kernels::BLOCK_ROWS);
    std::vector<mf::test::point> points;
    for (Eigen::Index r = 0; r < grid.rows(); ++r) {
      mf::test::point p;
      p.fz_n = grid(r, mf::mf5x::FZ);
      p.kappa = grid(r, mf::mf5x::SR);
      p.alpha_rad = grid(r, mf::mf5x::SA);
      p.gamma_rad = grid(r, mf::mf5x::IA);
      p.vx_mps = grid(r, mf::mf5x::VX);
      if (!contact_only || p.fz_n > 0.0) {
        points.push_back(p);
      }
    }
    return points;
  }

  block_inputs block_inputs_of(const std::vector<mf::test::point> &points) {
    const auto rows = static_cast<Eigen::Index>(points.size());
    block_inputs in;
    in.fz_n = block_lane::Zero(rows);
    in.alpha_rad = block_lane::Zero(rows);
    in.kappa = block_lane::Zero(rows);
    in.gamma_rad = block_lane::Zero(rows);
    in.vx_mps = block_lane::Zero(rows);
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

  // Evaluates `eval` once on the block of all points and once per point on point lanes, and compares every column
  // of every row within near_scaled.
  template <class Eval_T>
  void expect_block_matches_points(const std::vector<mf::test::point> &points, const Eval_T &eval) {
    const samples block = eval(block_inputs_of(points));
    for (std::size_t i = 0; i < points.size(); ++i) {
      const samples single = eval(lanes_of(points[i]));
      ASSERT_EQ(block.size(), single.size());
      for (std::size_t k = 0; k < block.size(); ++k) {
        ASSERT_EQ(block[k].values.size(), points.size());
        ASSERT_EQ(single[k].values.size(), 1U);
        EXPECT_TRUE(near_scaled(block[k].values[i], single[k].values[0], mf::test::BATCH_REL_TOL, block[k].scale))
            << "row " << i << " column " << k;
      }
    }
  }

  // Calls `check` with the block lanes of all points, then with the point lanes of each point.
  template <class Check_T> void on_each_lane_type(const std::vector<mf::test::point> &points, const Check_T &check) {
    check(block_inputs_of(points));
    for (const mf::test::point &p : points) {
      check(lanes_of(p));
    }
  }

  template <class Lane_T> void expect_lanes_near(const Lane_T &actual, const Lane_T &expected, double scale) {
    ASSERT_EQ(actual.size(), expected.size());
    for (Eigen::Index i = 0; i < actual.size(); ++i) {
      EXPECT_TRUE(near_scaled(actual(i), expected(i), mf::test::BATCH_REL_TOL, scale)) << "row " << i;
    }
  }

  template <class Lane_T> void expect_lanes_bits(const Lane_T &actual, const Lane_T &expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (Eigen::Index i = 0; i < actual.size(); ++i) {
      EXPECT_EQ(bits(actual(i)), bits(expected(i))) << "row " << i;
    }
  }

  // The four form combinations that between them take every branch of the form dispatch.
  std::vector<mf::mf5x_forms> all_forms() {
    std::vector<mf::mf5x_forms> forms;
    for (const bool ellipse : {false, true}) {
      for (const bool prime : {false, true}) {
        mf::mf5x_forms f;
        f.trail_cosine = prime ? mf::trail_cosine_form::COS_PRIME : mf::trail_cosine_form::COS_ALPHA;
        f.combined = ellipse ? mf::combined_method::FRICTION_ELLIPSE : mf::combined_method::COSINE;
        f.fy_prime = prime ? mf::fy_prime_form::SVYK0 : mf::fy_prime_form::SVYK;
        f.rolling_resistance = prime ? mf::rolling_resistance_form::SHIFTS : mf::rolling_resistance_form::QSY;
        forms.push_back(f);
      }
    }
    return forms;
  }

  // mf5x_evaluate composed by hand from the individual kernels on one point.
  kernels::force_lanes<point_lane> hand_composition(const coeffs &c, const mf::mf5x_forms &forms, mf::slip_mode mode,
                                                    const point_inputs &p) {
    const point_state state = kernels::evaluate_slip_state(c, p);
    const point_lane cos_alpha = p.alpha_rad.cos();
    point_lane cos_trail = cos_alpha;
    if (forms.trail_cosine == mf::trail_cosine_form::COS_PRIME) {
      const point_lane tan_alpha = p.alpha_rad.tan();
      const double vc = std::hypot(p.vx_mps(0), tan_alpha(0) * p.vx_mps(0));
      cos_trail = lane(p.vx_mps(0) / (vc + kernels::EPSILON));
    }
    kernels::force_lanes<point_lane> out;
    if (mode == mf::slip_mode::UNCOMBINED) {
      out.fx_n = state.lon.fx0_n;
      out.fy_n = state.lat.fy0_n;
      const point_lane trail_m = kernels::t(state.aln, state.aln.alpha_t, cos_trail);
      const point_lane residual_nm = kernels::mzr(state.aln, state.aln.alpha_r, cos_alpha);
      out.mz_nm = -(trail_m * out.fy_n) + residual_nm;
    } else {
      point_forces forces;
      if (forms.combined == mf::combined_method::FRICTION_ELLIPSE) {
        const kernels::ellipse_ratios<point_lane> ratios =
            kernels::friction_ellipse_ratios(state, p.kappa, p.alpha_rad);
        forces.fx_n = state.lon.fx0_n * ratios.rx;
        forces.fy_n = state.lat.fy0_n * ratios.ry;
        forces.svyk_n = point_lane::Zero();
      } else {
        forces = kernels::cosine_combined_forces(c, state, p);
      }
      const point_lane ky_guarded_n = kernels::add_epsilon(state.lat.ky_n);
      const point_lane ate = kernels::alpha_eq(state.aln.alpha_t, p.kappa, state.lon.kx_n, ky_guarded_n);
      const point_lane are = kernels::alpha_eq(state.aln.alpha_r, p.kappa, state.lon.kx_n, ky_guarded_n);
      const point_lane trail_m = kernels::t(state.aln, ate, cos_trail);
      point_lane fy_prime_n = forces.fy_n - forces.svyk_n;
      if (forms.fy_prime == mf::fy_prime_form::SVYK0) {
        const point_lane zero_camber = point_lane::Zero();
        const point_lane mu_zero_camber = kernels::mu_y(c, state.load, zero_camber);
        const point_lane svyk0_n =
            kernels::svyk(c, state.load, mu_zero_camber, p.fz_n, p.kappa, p.alpha_rad, p.gamma_rad);
        fy_prime_n = forces.fy_n - svyk0_n;
      }
      const point_lane arm_m = kernels::s(c, state.load, forces.fy_n, p.gamma_rad);
      const point_lane residual_nm = kernels::mzr(state.aln, are, cos_alpha);
      out.fx_n = forces.fx_n;
      out.fy_n = forces.fy_n;
      out.mz_nm = -(trail_m * fy_prime_n) + residual_nm + (arm_m * forces.fx_n);
    }
    out.mx_nm = kernels::mx(c, p.fz_n, out.fy_n, p.gamma_rad);
    if (forms.rolling_resistance == mf::rolling_resistance_form::SHIFTS) {
      out.my_nm = kernels::my_from_shifts(c, state.lon);
    } else {
      out.my_nm = kernels::my(c, p.fz_n, out.fx_n, p.vx_mps);
    }
    return out;
  }

  void check_evaluate_against_hand_composition(mf::slip_mode mode) {
    const coeffs c = rich_coefficients();
    const std::vector<mf::test::point> points = grid_points(true);
    for (const mf::mf5x_forms &forms : all_forms()) {
      const auto block = kernels::mf5x_evaluate(c, forms, mode, block_inputs_of(points));
      for (std::size_t i = 0; i < points.size(); ++i) {
        const point_inputs p = lanes_of(points[i]);
        const auto actual = kernels::mf5x_evaluate(c, forms, mode, p);
        const auto expected = hand_composition(c, forms, mode, p);
        SCOPED_TRACE(std::format("point {}", i));
        expect_lanes_bits(actual.fx_n, expected.fx_n);
        expect_lanes_bits(actual.fy_n, expected.fy_n);
        expect_lanes_bits(actual.mz_nm, expected.mz_nm);
        expect_lanes_bits(actual.mx_nm, expected.mx_nm);
        expect_lanes_bits(actual.my_nm, expected.my_nm);
        EXPECT_TRUE(near_scaled(block.fx_n(static_cast<Eigen::Index>(i)), actual.fx_n(0), mf::test::BATCH_REL_TOL,
                                FORCE_SCALE_N));
        EXPECT_TRUE(near_scaled(block.fy_n(static_cast<Eigen::Index>(i)), actual.fy_n(0), mf::test::BATCH_REL_TOL,
                                FORCE_SCALE_N));
        EXPECT_TRUE(near_scaled(block.mz_nm(static_cast<Eigen::Index>(i)), actual.mz_nm(0), mf::test::BATCH_REL_TOL,
                                MOMENT_SCALE_NM));
        EXPECT_TRUE(near_scaled(block.mx_nm(static_cast<Eigen::Index>(i)), actual.mx_nm(0), mf::test::BATCH_REL_TOL,
                                MOMENT_SCALE_NM));
        EXPECT_TRUE(near_scaled(block.my_nm(static_cast<Eigen::Index>(i)), actual.my_nm(0), mf::test::BATCH_REL_TOL,
                                MOMENT_SCALE_NM));
      }
    }
  }
} // namespace

// ---- sgn+, epsilon, curvature limit, Magic Formula shapes -------------------------------------------------------

TEST(mf5x_kernels, SgnPlus_SignedZeros_ReturnPlusOne) {
  EXPECT_DOUBLE_EQ(sgn_plus_of(0.0), 1.0);
  EXPECT_DOUBLE_EQ(sgn_plus_of(-0.0), 1.0);
}

TEST(mf5x_kernels, SgnPlus_TinyNegative_ReturnsMinusOne) {
  EXPECT_DOUBLE_EQ(sgn_plus_of(-1e-300), -1.0);
}

TEST(mf5x_kernels, SgnPlus_Positive_ReturnsPlusOne) {
  EXPECT_DOUBLE_EQ(sgn_plus_of(1e-300), 1.0);
  EXPECT_DOUBLE_EQ(sgn_plus_of(3.0), 1.0);
}

TEST(mf5x_kernels, AddEpsilon_Positive_AddsEpsilon) {
  EXPECT_TRUE(rel_near_result(add_epsilon_of(2.0), 2.0 + 1e-6, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, AddEpsilon_Negative_SubtractsEpsilon) {
  EXPECT_TRUE(rel_near_result(add_epsilon_of(-2.0), -2.0 - 1e-6, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, AddEpsilon_SignedZero_ReturnsPlusEpsilon) {
  EXPECT_TRUE(rel_near_result(add_epsilon_of(0.0), 1e-6, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(add_epsilon_of(-0.0), 1e-6, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, LimitCurvature_AboveOne_ReturnsOne) {
  EXPECT_DOUBLE_EQ(limit_curvature_of(1.5), 1.0);
}

TEST(mf5x_kernels, LimitCurvature_AtOrBelowOne_Unchanged) {
  EXPECT_DOUBLE_EQ(limit_curvature_of(1.0), 1.0);
  EXPECT_DOUBLE_EQ(limit_curvature_of(0.999), 0.999);
  EXPECT_DOUBLE_EQ(limit_curvature_of(-3.0), -3.0);
}

TEST(mf5x_kernels, SineMagicFormula_ClosedForm) {
  constexpr double b = 2.0;
  constexpr double c = 1.3;
  constexpr double d = 1000.0;
  constexpr double e = 0.5;
  constexpr double x = 0.1;
  const double bx = b * x;
  const double expected = d * std::sin(c * std::atan(bx - (e * (bx - std::atan(bx)))));
  EXPECT_TRUE(rel_near_result(sine_mf_of(b, c, d, e, x), expected, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, CosineMagicFormula_ZeroB_ReturnsOne) {
  EXPECT_TRUE(rel_near_result(cosine_mf_of(0.0, 1.2, 0.3, 0.7), 1.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, CosineMagicFormula_ClosedForm) {
  constexpr double b = 4.0;
  constexpr double c = 1.1;
  constexpr double e = -0.4;
  constexpr double x = 0.2;
  const double bx = b * x;
  const double expected = std::cos(c * std::atan(bx - (e * (bx - std::atan(bx)))));
  EXPECT_TRUE(rel_near_result(cosine_mf_of(b, c, e, x), expected, REL_TOL_CLOSED_FORM));
}

// ---- vertical load, pure longitudinal and pure lateral ----------------------------------------------------------

TEST(mf5x_kernels, VerticalLoad_AtFnomin_DfzZero) {
  const point_load load = load_at(s_with({}), FZ_NOMINAL);
  // V1: bit-equal to zero.
  EXPECT_EQ(bits(load.dfz(0)), bits(0.0));
  EXPECT_TRUE(rel_near_result(load.fz0_prime_n, 4000.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, VerticalLoad_Lfzo12_DfzMinusOneSixth) {
  const point_load load = load_at(s_with({{&coeffs::LFZO, 1.2}}), FZ_NOMINAL);
  EXPECT_TRUE(rel_near_result(load.dfz(0), -1.0 / 6.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(load.fz0_prime_n, 4800.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Fx0_Kappa01_MatchesV2) {
  const point_lon t = fx0_at(s_with({}), FZ_NOMINAL, 0.1);
  EXPECT_NEAR(t.fx0_n(0), 3935.48, printed_tol(2));
  EXPECT_TRUE(rel_near_result(t.fx0_n(0), 4000.0 * std::sin(1.5 * std::atan(4.0 / 3.0)), 1e-9));
  EXPECT_TRUE(rel_near_result(t.cx, 1.5, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.dx_n(0), 4000.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.kx_n(0), 80000.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Fx0_Pvx1_SvxOutsideSine_MatchesV4) {
  const point_lon t = fx0_at(s_with({{&coeffs::PVX1, 0.01}}), FZ_NOMINAL, 0.0);
  EXPECT_TRUE(rel_near_result(t.fx0_n(0), 40.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.svx_n(0), 40.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Fx0_SlopeAtOrigin_MatchesV7) {
  const coeffs c = s_with({});
  constexpr double h = 1e-7;
  const double plus = fx0_at(c, FZ_NOMINAL, h).fx0_n(0);
  const double minus = fx0_at(c, FZ_NOMINAL, -h).fx0_n(0);
  EXPECT_TRUE(rel_near_result((plus - minus) / (2.0 * h), 80000.0, 1e-6));
}

TEST(mf5x_kernels, Fx0_Lmux05_MatchesV8) {
  const point_lon t = fx0_at(s_with({{&coeffs::LMUX, 0.5}}), FZ_NOMINAL, 0.1);
  EXPECT_TRUE(rel_near_result(t.dx_n(0), 2000.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.bx(0), 80.0 / 3.0, 1e-9));
}

TEST(mf5x_kernels, Fx0_Pdx3Lgax05_MatchesV16) {
  const coeffs c = s_with({{&coeffs::PDX3, 10.0}, {&coeffs::LGAX, 0.5}});
  const point_lon t = fx0_at(c, FZ_NOMINAL, 0.1, 0.1);
  EXPECT_TRUE(rel_near_result(t.gamma_x(0), 0.1 * 0.5, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.dx_n(0), 3900.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Fx0_KxAtFnomin_EqualsPkx1FnominLkx) {
  const coeffs c = s_with({{&coeffs::PKX2, 3.0}, {&coeffs::PKX3, 0.5}, {&coeffs::LKX, 1.1}});
  const point_lon t = fx0_at(c, FZ_NOMINAL, 0.0);
  EXPECT_TRUE(rel_near_result(t.kx_n(0), 20.0 * 4000.0 * 1.1, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Fx0_LoadDependence_ClosedForm) {
  // Fz = 5000 gives dfz = 0.25.
  const coeffs c = s_with({{&coeffs::PDX2, 0.1},
                           {&coeffs::PKX2, 4.0},
                           {&coeffs::PKX3, 0.4},
                           {&coeffs::PHX1, 0.001},
                           {&coeffs::PHX2, 0.004},
                           {&coeffs::PVX1, 0.01},
                           {&coeffs::PVX2, 0.04}});
  const point_lon t = fx0_at(c, 5000.0, 0.0);
  EXPECT_TRUE(rel_near_result(t.mu_x(0), 1.025, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.dx_n(0), 5125.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.kx_n(0), 5000.0 * 21.0 * std::exp(0.1), REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.shx(0), 0.002, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.svx_n(0), 100.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.kappa_x(0), 0.002, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Fx0_PeakMagnitude_EqualsDx) {
  // P10: max |Fx0 - SVx| over kappa equals mu_x*Fz when Cx > 1.
  const coeffs c = s_with({{&coeffs::PVX1, 0.01}});
  constexpr double step_count = 20000.0;
  double peak = 0.0;
  double dx = 0.0;
  for (int i = -20000; i <= 20000; ++i) {
    const point_lon t = fx0_at(c, FZ_NOMINAL, static_cast<double>(i) / step_count);
    peak = std::max(peak, std::abs(t.fx0_n(0) - t.svx_n(0)));
    dx = t.dx_n(0);
  }
  EXPECT_TRUE(rel_near_result(peak, dx, 1e-6));
  EXPECT_LE(peak, dx * (1.0 + 1e-12));
}

TEST(mf5x_kernels, Fx0_ExClamp_V15) {
  const point_lon clamped = fx0_at(s_with({{&coeffs::PEX1, 1.5}}), FZ_NOMINAL, 0.1);
  const point_lon at_one = fx0_at(s_with({{&coeffs::PEX1, 1.0}}), FZ_NOMINAL, 0.1);
  const point_lon below = fx0_at(s_with({{&coeffs::PEX1, 0.999}}), FZ_NOMINAL, 0.1);
  // V15: bit-equal to PEX1 = 1.0.
  EXPECT_EQ(bits(clamped.ex(0)), bits(at_one.ex(0)));
  EXPECT_EQ(bits(clamped.fx0_n(0)), bits(at_one.fx0_n(0)));
  EXPECT_NEAR(clamped.fx0_n(0), 3603.08, printed_tol(2));
  EXPECT_NE(bits(below.ex(0)), bits(at_one.ex(0)));
  EXPECT_NE(bits(below.fx0_n(0)), bits(at_one.fx0_n(0)));
}

TEST(mf5x_kernels, Fx0_Pex4_CurvatureDependsOnSlipSign) {
  const coeffs c = s_with({{&coeffs::PEX1, 0.5}, {&coeffs::PEX4, 0.5}});
  EXPECT_TRUE(rel_near_result(fx0_at(c, FZ_NOMINAL, 0.1).ex(0), 0.25, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(fx0_at(c, FZ_NOMINAL, -0.1).ex(0), 0.75, REL_TOL_CLOSED_FORM));
  // sgn+(-0.0) = +1.
  EXPECT_TRUE(rel_near_result(fx0_at(c, FZ_NOMINAL, -0.0).ex(0), 0.25, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Fx0_ZeroCxDx_GuardedFinite) {
  const coeffs c = s_with({{&coeffs::PCX1, 0.0}, {&coeffs::PVX1, 0.01}});
  const point_lon t = fx0_at(c, FZ_NOMINAL, 0.1);
  EXPECT_TRUE(std::isfinite(t.bx(0)));
  EXPECT_TRUE(rel_near_result(t.bx(0), 80000.0 / 1e-6, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(std::isfinite(t.fx0_n(0)));
  // sin(0) = 0 exactly (Cx = 0), so Fx0 is the vertical shift.
  EXPECT_TRUE(rel_near_result(t.fx0_n(0), t.svx_n(0), REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Fy0_Alpha005_MatchesV3) {
  const point_lat t = fy0_at(s_with({}), FZ_NOMINAL, 0.05);
  EXPECT_NEAR(t.ky0_n(0), -55384.615, 0.0005);
  EXPECT_NEAR(t.by(0), -10.650888, 5e-7);
  EXPECT_NEAR(t.fy0_n(0), -2376.40, printed_tol(2));
}

TEST(mf5x_kernels, Fy0_Lfzo12_MatchesV18) {
  const point_lat t = fy0_at(s_with({{&coeffs::LFZO, 1.2}}), FZ_NOMINAL, 0.05);
  EXPECT_NEAR(t.ky0_n(0), -61132.08, printed_tol(2));
}

TEST(mf5x_kernels, Fy0_KyMaximum_AtPky2Fnomin) {
  // P9: |Ky| reaches |PKY1*FNOMIN*LKY| at Fz = PKY2*FNOMIN.
  const coeffs c = s_with({{&coeffs::LKY, 1.1}});
  const double fz_peak = 1.5 * 4000.0;
  const double ky_peak = std::abs(fy0_at(c, fz_peak, 0.0).ky0_n(0));
  EXPECT_TRUE(rel_near_result(ky_peak, std::abs(-15.0 * 4000.0 * 1.1), REL_TOL_CLOSED_FORM));
  EXPECT_GT(ky_peak, std::abs(fy0_at(c, fz_peak * 0.99, 0.0).ky0_n(0)));
  EXPECT_GT(ky_peak, std::abs(fy0_at(c, fz_peak * 1.01, 0.0).ky0_n(0)));
}

TEST(mf5x_kernels, Fy0_Phy3_MatchesV17) {
  const point_lat t = fy0_at(s_with({{&coeffs::PHY3, 0.1}}), FZ_NOMINAL, 0.0, 0.1);
  EXPECT_TRUE(rel_near_result(t.gamma_y(0), 0.1, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.shy(0), 0.01, REL_TOL_CLOSED_FORM));
  EXPECT_NEAR(t.fy0_n(0), -550.02, printed_tol(2));
}

TEST(mf5x_kernels, Fy0_EyClamp) {
  const point_lat clamped = fy0_at(s_with({{&coeffs::PEY1, 1.5}}), FZ_NOMINAL, 0.05);
  const point_lat at_one = fy0_at(s_with({{&coeffs::PEY1, 1.0}}), FZ_NOMINAL, 0.05);
  const point_lat below = fy0_at(s_with({{&coeffs::PEY1, 0.999}}), FZ_NOMINAL, 0.05);
  EXPECT_EQ(bits(clamped.ey(0)), bits(at_one.ey(0)));
  EXPECT_EQ(bits(clamped.fy0_n(0)), bits(at_one.fy0_n(0)));
  EXPECT_NE(bits(below.ey(0)), bits(at_one.ey(0)));
  EXPECT_NE(bits(below.fy0_n(0)), bits(at_one.fy0_n(0)));
}

TEST(mf5x_kernels, Fy0_Pey3_CurvatureDependsOnSlipAngleSign) {
  const coeffs c = s_with({{&coeffs::PEY1, 0.5}, {&coeffs::PEY3, 0.5}});
  EXPECT_TRUE(rel_near_result(fy0_at(c, FZ_NOMINAL, 0.05).ey(0), 0.25, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(fy0_at(c, FZ_NOMINAL, -0.05).ey(0), 0.75, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Fy0_Camber_Pdy3Lgay) {
  const coeffs c = s_with({{&coeffs::PDY3, 10.0}, {&coeffs::LGAY, 0.5}});
  const point_lat t = fy0_at(c, FZ_NOMINAL, 0.0, 0.1);
  EXPECT_TRUE(rel_near_result(t.gamma_y(0), 0.1 * 0.5, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.dy_n(0), 3900.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.mu_y(0), mu_y_of(c, load_at(c, FZ_NOMINAL), 0.05), REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Fy0_Pvy3_VerticalShiftFollowsCamber) {
  const coeffs c = s_with({{&coeffs::PVY3, 0.1}});
  const point_lat t = fy0_at(c, FZ_NOMINAL, 0.0, 0.1);
  EXPECT_TRUE(rel_near_result(t.svy_n(0), 4000.0 * 0.1 * 0.1, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Fy0_LoadDependence_ClosedForm) {
  // Fz = 5000 gives dfz = 0.25.
  const coeffs c = s_with({{&coeffs::PDY2, -0.1},
                           {&coeffs::PHY1, 0.002},
                           {&coeffs::PHY2, 0.004},
                           {&coeffs::PVY1, 0.01},
                           {&coeffs::PVY2, 0.04}});
  const point_lat t = fy0_at(c, 5000.0, 0.0);
  EXPECT_TRUE(rel_near_result(t.mu_y(0), 0.975, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.dy_n(0), 4875.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.shy(0), 0.003, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.svy_n(0), 100.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(t.alpha_y(0), 0.003, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Fy0_Lky_ScalesKyLinearly) {
  const double base = fy0_at(s_with({}), FZ_NOMINAL, 0.05).ky0_n(0);
  const double scaled = fy0_at(s_with({{&coeffs::LKY, 0.7}}), FZ_NOMINAL, 0.05).ky0_n(0);
  EXPECT_TRUE(rel_near_result(scaled, 0.7 * base, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Fy0_KyZeroViaPky3_ByZero) {
  const coeffs c = s_with({{&coeffs::PKY3, 1.0}, {&coeffs::PVY1, 0.01}});
  const point_lat t = fy0_at(c, FZ_NOMINAL, 0.05, 1.0);
  // Ky = Ky0*(1 - 1*1) is a product with an exact zero, so it is zero; sin(0) = 0 leaves Fy0 = SVy.
  constexpr double zero_tol = 1e-12;
  EXPECT_NEAR(t.ky_n(0), 0.0, zero_tol);
  EXPECT_NEAR(t.by(0), 0.0, zero_tol);
  EXPECT_TRUE(rel_near_result(t.fy0_n(0), t.svy_n(0), REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Fy0_NegativeCyDy_EpsilonSubtracted) {
  const point_lat t = fy0_at(s_with({{&coeffs::PDY1, -1.0}}), FZ_NOMINAL, 0.05);
  EXPECT_LT(t.cy * t.dy_n(0), 0.0);
  // Equal to Ky/(Cy*Dy - 1e-6); a relative tolerance covers fused multiply-add targets.
  EXPECT_TRUE(rel_near_result(t.by(0), t.ky_n(0) / ((t.cy * t.dy_n(0)) - 1e-6), 1e-15));
}

TEST(mf5x_kernels, EvaluateSlipState_ComposesStages) {
  const coeffs c = s_with({{&coeffs::PHX1, 0.001}, {&coeffs::PVY1, 0.01}, {&coeffs::QDZ6, 0.01}});
  const point_inputs p = lanes_of(make_point(3500.0, 0.07, 0.04, 0.03));
  const point_state s = kernels::evaluate_slip_state(c, p);
  const point_load load = kernels::vertical_load(c, p.fz_n);
  const point_lon lon = kernels::fx0(c, load, p.fz_n, p.kappa, p.gamma_rad);
  const point_lat lat = kernels::fy0(c, load, p.fz_n, p.alpha_rad, p.gamma_rad);
  const point_aln aln = kernels::aligning(c, load, lat, p.fz_n, p.alpha_rad, p.gamma_rad);
  samples actual;
  append_state(actual, s);
  samples expected;
  append_load(expected, load);
  append_lon(expected, lon);
  append_lat(expected, lat);
  append_aln(expected, aln);
  expect_same_samples(actual, expected);
  EXPECT_EQ(bits(s.load.fz0_prime_n), bits(load.fz0_prime_n));
  EXPECT_EQ(bits(s.lon.cx), bits(lon.cx));
  EXPECT_EQ(bits(s.lat.cy), bits(lat.cy));
  EXPECT_EQ(bits(s.aln.ct), bits(aln.ct));
  EXPECT_EQ(bits(s.aln.cr), bits(aln.cr));
}

// ---- aligning moment, equivalent slip angle, combined slip -------------------------------------------------------

TEST(mf5x_kernels, Aligning_Alpha005_MatchesV11) {
  const coeffs c = s_with({});
  const double cos_alpha = std::cos(0.05);
  const point_state s = state_at(c, FZ_NOMINAL, 0.0, 0.05);
  const double trail = t_of(s.aln, s.aln.alpha_t(0), cos_alpha);
  const double mz0 = -(trail * s.lat.fy0_n(0)) + mzr_of(s.aln, s.aln.alpha_r(0), cos_alpha);
  EXPECT_NEAR(trail, 0.0254434, 5e-8);
  EXPECT_NEAR(mz0, 60.46, printed_tol(2));
}

TEST(mf5x_kernels, Aligning_Lfzo12_DtIs003) {
  const point_state s = state_at(s_with({{&coeffs::LFZO, 1.2}}), FZ_NOMINAL, 0.0, 0.05);
  EXPECT_TRUE(rel_near_result(s.aln.dt_m(0), 0.03, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(s.load.dfz(0), -1.0 / 6.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Aligning_Shf_UsesGuardedKy) {
  const point_state s = state_at(s_with({{&coeffs::PVY1, 0.01}}), FZ_NOMINAL, 0.0, 0.05);
  const double expected = s.lat.shy(0) + (s.lat.svy_n(0) / add_epsilon_of(s.lat.ky_n(0)));
  EXPECT_EQ(bits(s.aln.shf(0)), bits(expected));
  EXPECT_TRUE(rel_near_result(s.aln.alpha_r(0), 0.05 + s.aln.shf(0), REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Aligning_EtClamp) {
  const point_state clamped = state_at(s_with({{&coeffs::QEZ1, 1.5}}), FZ_NOMINAL, 0.0, 0.05);
  const point_state at_one = state_at(s_with({{&coeffs::QEZ1, 1.0}}), FZ_NOMINAL, 0.0, 0.05);
  const point_state below = state_at(s_with({{&coeffs::QEZ1, 0.999}}), FZ_NOMINAL, 0.0, 0.05);
  EXPECT_EQ(bits(clamped.aln.et(0)), bits(at_one.aln.et(0)));
  EXPECT_NE(bits(below.aln.et(0)), bits(at_one.aln.et(0)));
}

TEST(mf5x_kernels, Aligning_Et_ClosedForm) {
  const point_state s = state_at(s_with({{&coeffs::QEZ1, 0.5}, {&coeffs::QEZ4, 0.2}}), FZ_NOMINAL, 0.0, 0.05);
  const double expected = 0.5 * (1.0 + (0.2 * (2.0 / std::numbers::pi) * std::atan(10.0 * 1.2 * 0.05)));
  EXPECT_TRUE(rel_near_result(s.aln.et(0), expected, REL_TOL_CLOSED_FORM));
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
  const point_state s = state_at(c, 5000.0, 0.0, 0.05, 0.1);
  EXPECT_TRUE(rel_near_result(s.aln.gamma_z(0), 0.1, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(s.aln.bt(0), 14.3, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(s.aln.dt_m(0), 0.09, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(s.aln.sht(0), 0.04, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(s.aln.alpha_t(0), 0.09, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(s.aln.dr_nm(0), 36.0, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(s.aln.br(0), 2.0 + (0.5 * s.lat.by(0) * s.lat.cy), REL_TOL_CLOSED_FORM));
  EXPECT_DOUBLE_EQ(s.aln.cr, 1.0);
  EXPECT_DOUBLE_EQ(s.aln.ct, 1.2);
}

TEST(mf5x_kernels, T_EvenInSlip) {
  // P5.
  const coeffs c = s_with({{&coeffs::QEZ1, 0.5}, {&coeffs::QEZ4, 0.2}});
  const point_state s = state_at(c, FZ_NOMINAL, 0.0, 0.05);
  for (const double x : {0.01, 0.05, 0.3, 1.0}) {
    EXPECT_TRUE(rel_near_result(t_of(s.aln, -x, 0.9), t_of(s.aln, x, 0.9), REL_TOL_CLOSED_FORM));
  }
}

TEST(mf5x_kernels, Mzr_EvenInSlip) {
  // P5.
  const coeffs c = s_with({{&coeffs::QBZ10, 1.0}, {&coeffs::QDZ6, 0.01}});
  const point_state s = state_at(c, FZ_NOMINAL, 0.0, 0.05);
  EXPECT_GT(std::abs(s.aln.dr_nm(0)), 0.0);
  for (const double x : {0.01, 0.05, 0.3, 1.0}) {
    EXPECT_TRUE(rel_near_result(mzr_of(s.aln, -x, 0.9), mzr_of(s.aln, x, 0.9), REL_TOL_CLOSED_FORM));
  }
}

TEST(mf5x_kernels, AlphaEq_ZeroKappa_ReturnsInput) {
  // The tolerance covers the tan/atan round trip (a few ulp).
  for (const double a : {0.05, -0.05}) {
    EXPECT_TRUE(rel_near_result(alpha_eq_of(a, 0.0, 8e4, -5.5e4), a, 1e-14));
  }
}

TEST(mf5x_kernels, AlphaEq_SignAtZero_UsesSgnPlus) {
  // sgn+(+0.0) = sgn+(-0.0) = +1; the sign flips only for a < 0.
  EXPECT_GT(alpha_eq_of(0.0, 0.1, 8e4, -5.5e4), 0.0);
  EXPECT_GT(alpha_eq_of(-0.0, 0.1, 8e4, -5.5e4), 0.0);
  EXPECT_LT(alpha_eq_of(-1e-9, 0.1, 8e4, -5.5e4), 0.0);
  EXPECT_GT(alpha_eq_of(1e-9, 0.1, 8e4, -5.5e4), 0.0);
}

TEST(mf5x_kernels, AlphaEq_ClosedForm) {
  const double tan_a = std::tan(0.1);
  const double ratio = 8e4 / -5.5e4;
  const double expected = std::atan(std::sqrt((tan_a * tan_a) + (ratio * ratio * 0.05 * 0.05)));
  EXPECT_TRUE(rel_near_result(alpha_eq_of(0.1, 0.05, 8e4, -5.5e4), expected, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Gxa_ZeroB_ReturnsOne) {
  const coeffs c = s_with({{&coeffs::RBX1, 0.0}});
  const double weight = gxa_of(c, load_at(c, FZ_NOMINAL), 0.1, 0.1);
  EXPECT_TRUE(rel_near_result(weight, 1.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Gxa_MatchesV12Factor) {
  const coeffs c = s_with({});
  const double weight = gxa_of(c, load_at(c, FZ_NOMINAL), 0.1, 0.1);
  EXPECT_TRUE(rel_near_result(weight, std::cos(std::numbers::pi / 4.0), REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Gyk_ZeroB_ReturnsOne) {
  const coeffs c = s_with({{&coeffs::RBY1, 0.0}});
  const double weight = gyk_of(c, load_at(c, FZ_NOMINAL), 0.1, 0.05);
  EXPECT_TRUE(rel_near_result(weight, 1.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Gyk_MatchesV13Factor) {
  const coeffs c = s_with({});
  const double weight = gyk_of(c, load_at(c, FZ_NOMINAL), 0.1, 0.05);
  EXPECT_TRUE(rel_near_result(weight, std::cos(std::numbers::pi / 4.0), REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Weighting_ExaEykClamp) {
  const auto weight_x = [](double rex1) {
    const coeffs c = s_with({{&coeffs::REX1, rex1}});
    return gxa_of(c, load_at(c, FZ_NOMINAL), 0.1, 0.1);
  };
  const auto weight_y = [](double rey1) {
    const coeffs c = s_with({{&coeffs::REY1, rey1}});
    return gyk_of(c, load_at(c, FZ_NOMINAL), 0.1, 0.05);
  };
  EXPECT_EQ(bits(weight_x(1.5)), bits(weight_x(1.0)));
  EXPECT_NE(bits(weight_x(0.999)), bits(weight_x(1.0)));
  EXPECT_EQ(bits(weight_y(1.5)), bits(weight_y(1.0)));
  EXPECT_NE(bits(weight_y(0.999)), bits(weight_y(1.0)));
}

TEST(mf5x_kernels, Weighting_NotBoundedByOne) {
  // No G <= 1 assumption: a horizontal shift makes the numerator exceed the denominator.
  const coeffs c = s_with({{&coeffs::RHX1, 0.1}});
  const double weight = gxa_of(c, load_at(c, FZ_NOMINAL), 0.1, -0.1);
  EXPECT_GT(weight, 1.0);
  EXPECT_TRUE(rel_near_result(weight, 1.0 / std::cos(std::numbers::pi / 4.0), REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Svyk_MatchesV14) {
  const coeffs c = s_with({{&coeffs::RBY1, 0.0}, {&coeffs::RVY1, 0.1}, {&coeffs::RVY5, 1.0}, {&coeffs::RVY6, 10.0}});
  const point_load load = load_at(c, FZ_NOMINAL);
  const double mu_zero_camber = mu_y_of(c, load, 0.0);
  EXPECT_NEAR(svyk_of(c, load, mu_zero_camber, FZ_NOMINAL, 0.1, 0.05, 0.0), 282.843, 0.0005);
}

TEST(mf5x_kernels, Svyk_ZeroCoefficients_ReturnsZero) {
  const coeffs c = s_with({});
  const point_load load = load_at(c, FZ_NOMINAL);
  EXPECT_NEAR(svyk_of(c, load, 1.0, FZ_NOMINAL, 0.1, 0.05, 0.1), 0.0, 1e-12);
}

TEST(mf5x_kernels, Svyk_CamberAndLoad_ClosedForm) {
  // Fz = 5000 gives dfz = 0.25; DVyk = mu_y*Fz*(RVY1 + RVY2*dfz + RVY3*gamma)*cos(atan(RVY4*alpha)).
  const coeffs c = s_with({{&coeffs::RVY1, 0.1},
                           {&coeffs::RVY2, 0.2},
                           {&coeffs::RVY3, 0.5},
                           {&coeffs::RVY4, 2.0},
                           {&coeffs::RVY5, 1.0},
                           {&coeffs::RVY6, 10.0},
                           {&coeffs::LVYKA, 0.8}});
  const point_load load = load_at(c, 5000.0);
  const double dvyk = 1.0 * 5000.0 * (0.1 + (0.2 * 0.25) + (0.5 * 0.1)) * std::cos(std::atan(2.0 * 0.05));
  const double expected = dvyk * std::sin(std::atan(10.0 * 0.1)) * 0.8;
  EXPECT_TRUE(rel_near_result(svyk_of(c, load, 1.0, 5000.0, 0.1, 0.05, 0.1), expected, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, CosineCombinedForces_MatchesV12V13) {
  const coeffs c = s_with({});
  const point_forces both = combined_at(c, FZ_NOMINAL, 0.1, 0.1);
  EXPECT_NEAR(both.fx_n(0), 2782.80, printed_tol(2));
  const point_forces lateral_case = combined_at(c, FZ_NOMINAL, 0.1, 0.05);
  EXPECT_NEAR(lateral_case.fy_n(0), -1680.37, printed_tol(2));
  EXPECT_NEAR(lateral_case.svyk_n(0), 0.0, 1e-12);
}

TEST(mf5x_kernels, CosineCombinedForces_AddsSvykToFy) {
  const coeffs c = s_with({{&coeffs::RBY1, 0.0}, {&coeffs::RVY1, 0.1}, {&coeffs::RVY5, 1.0}, {&coeffs::RVY6, 10.0}});
  const point_inputs p = lanes_of(make_point(FZ_NOMINAL, 0.1, 0.05, 0.0));
  const point_state s = kernels::evaluate_slip_state(c, p);
  const point_forces forces = kernels::cosine_combined_forces(c, s, p);
  // V14: SVyk = 400*sin(atan(1)) and Fy = Fy0 + SVyk (Gyk = 1 because RBY1 = 0).
  EXPECT_TRUE(rel_near_result(forces.svyk_n(0), 400.0 * std::sin(std::numbers::pi / 4.0), REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(forces.fy_n(0), s.lat.fy0_n(0) + forces.svyk_n(0), REL_TOL_CLOSED_FORM));
}

// ---- friction ellipse -------------------------------------------------------------------------------------------

TEST(mf5x_kernels, FrictionEllipse_FourQuadrants_SignsAndMagnitudes) {
  // P1.
  const coeffs c = ellipse_coefficients();
  for (const double kappa : {0.1, -0.1}) {
    for (const double alpha : {0.05, -0.05}) {
      const point_state s = state_at(c, FZ_NOMINAL, kappa, alpha);
      const ratio_values ratios = ratios_of(s, kappa, alpha);
      SCOPED_TRACE(std::format("kappa {} alpha {}", kappa, alpha));
      EXPECT_TRUE(is_unit_ratio(ratios.rx));
      EXPECT_TRUE(is_unit_ratio(ratios.ry));
      EXPECT_TRUE(is_reduction_of(ratios.rx * s.lon.fx0_n(0), s.lon.fx0_n(0)));
      EXPECT_TRUE(is_reduction_of(ratios.ry * s.lat.fy0_n(0), s.lat.fy0_n(0)));
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
    const point_state s = state_at(c, fz, kappa, alpha);
    const ratio_values actual = ratios_of(s, kappa, alpha);
    const ratio_values expected = printed_form_ratios(s, fz, kappa, alpha);
    EXPECT_TRUE(rel_near_result(actual.rx, expected.rx, 1e-10)) << "point " << i;
    EXPECT_TRUE(rel_near_result(actual.ry, expected.ry, 1e-10)) << "point " << i;
  }
}

TEST(mf5x_kernels, FrictionEllipse_DegeneratePoints_RatioOne) {
  // P16: kappa_c = 0 and alpha* = 0 give the 0/0 ratio 1.
  const coeffs c = s_with({});
  const ratio_values origin = ratios_of(state_at(c, FZ_NOMINAL, 0.0, 0.0), 0.0, 0.0);
  EXPECT_DOUBLE_EQ(origin.rx, 1.0);
  EXPECT_DOUBLE_EQ(origin.ry, 1.0);
}

TEST(mf5x_kernels, FrictionEllipse_OneZeroSlip_RatioZeroAndOne) {
  // A hand-built state whose pure forces are non-zero at the shifted origin: kappa_c = 0 gives rx = 0 and ry = 1.
  point_state s;
  s.lon.shx = lane(0.0);
  s.lon.svx_n = lane(0.0);
  s.lon.kx_n = lane(80000.0);
  s.lon.fx0_n = lane(1000.0);
  s.lon.dx_n = lane(4000.0);
  s.lat.shy = lane(0.0);
  s.lat.svy_n = lane(0.0);
  s.lat.ky_n = lane(-55000.0);
  s.lat.fy0_n = lane(-2000.0);
  s.lat.dy_n = lane(4000.0);
  const ratio_values no_kappa = ratios_of(s, 0.0, 0.05);
  EXPECT_NEAR(no_kappa.rx, 0.0, 1e-12);
  EXPECT_NEAR(no_kappa.ry, 1.0, 1e-12);
  // alpha* = 0 gives ry = 0 and rx = 1.
  const ratio_values no_alpha = ratios_of(s, 0.1, 0.0);
  EXPECT_NEAR(no_alpha.rx, 1.0, 1e-12);
  EXPECT_NEAR(no_alpha.ry, 0.0, 1e-12);
}

TEST(mf5x_kernels, FrictionEllipse_ZeroKx_GuardedFinite) {
  const coeffs c = s_with({{&coeffs::PKX1, 0.0}, {&coeffs::PVX1, 0.01}});
  const point_state s = state_at(c, FZ_NOMINAL, 0.1, 0.05);
  const ratio_values ratios = ratios_of(s, 0.1, 0.05);
  EXPECT_TRUE(std::isfinite(ratios.rx));
  EXPECT_TRUE(std::isfinite(ratios.ry));
  EXPECT_GE(ratios.rx, 0.0);
  EXPECT_LE(ratios.rx, 1.0 + 1e-12);
}

// ---- moment arm, Mx, My -----------------------------------------------------------------------------------------

TEST(mf5x_kernels, S_ClosedForm) {
  const coeffs c = s_with(
      {{&coeffs::SSZ1, 0.1}, {&coeffs::SSZ2, 0.2}, {&coeffs::SSZ3, 0.3}, {&coeffs::SSZ4, 0.4}, {&coeffs::LS, 0.9}});
  const point_load load = load_at(c, FZ_NOMINAL);
  // R0*(SSZ1 + SSZ2*Fy/FNOMIN + (SSZ3 + SSZ4*dfz)*gamma)*LS with dfz = 0.
  const double expected = 0.3 * (0.1 + (0.2 * -1000.0 / 4000.0) + (0.3 * 0.05)) * 0.9;
  EXPECT_TRUE(rel_near_result(s_of(c, load, -1000.0, 0.05), expected, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(s_of(c, load, -1000.0, 0.05), 0.01755, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, S_Lfzo12_NormalisedByFnomin) {
  const coeffs c = s_with(
      {{&coeffs::LFZO, 1.2}, {&coeffs::SSZ1, 0.1}, {&coeffs::SSZ2, 0.2}, {&coeffs::SSZ3, 0.3}, {&coeffs::SSZ4, 0.4}});
  const point_load load = load_at(c, FZ_NOMINAL);
  const double dfz = -1.0 / 6.0;
  const double expected = 0.3 * (0.1 + (0.2 * -1000.0 / 4000.0) + ((0.3 + (0.4 * dfz)) * 0.05));
  const double with_f0_prime = 0.3 * (0.1 + (0.2 * -1000.0 / 4800.0) + ((0.3 + (0.4 * dfz)) * 0.05));
  const double actual = s_of(c, load, -1000.0, 0.05);
  EXPECT_TRUE(rel_near_result(actual, expected, REL_TOL_CLOSED_FORM));
  EXPECT_FALSE(rel_near_result(actual, with_f0_prime, 1e-6));
}

TEST(mf5x_kernels, Mx_MatchesV9) {
  const coeffs c = s_with({{&coeffs::QSX1, 0.01}, {&coeffs::QSX2, 0.5}, {&coeffs::QSX3, 0.1}});
  EXPECT_TRUE(rel_near_result(mx_of(c, FZ_NOMINAL, 0.0, 0.05), -18.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, Mx_ClosedForm) {
  // P11 with LVMX = 0.8, LMX = 1.1 and Fy = 500.
  const coeffs c = s_with(
      {{&coeffs::QSX1, 0.01}, {&coeffs::QSX2, 0.5}, {&coeffs::QSX3, 0.1}, {&coeffs::LVMX, 0.8}, {&coeffs::LMX, 1.1}});
  const double expected = 0.3 * 4000.0 * ((0.01 * 0.8) - (0.5 * 0.05) + (0.1 * 500.0 / 4000.0)) * 1.1;
  EXPECT_TRUE(rel_near_result(mx_of(c, FZ_NOMINAL, 500.0, 0.05), expected, REL_TOL_CLOSED_FORM));
  EXPECT_TRUE(rel_near_result(expected, -5.94, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, My_MatchesV5) {
  const double rolling = my_of(s_with({}), FZ_NOMINAL, 0.0, SPEED_MPS);
  EXPECT_TRUE(rel_near_result(rolling, -12.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, My_SpeedTerms_MatchesV21) {
  const coeffs c = s_with({{&coeffs::QSY3, 0.1}, {&coeffs::QSY4, 0.01}});
  const double rolling = my_of(c, FZ_NOMINAL, 0.0, SPEED_MPS);
  const double ratio = 10.0 / 16.6;
  const double expected = -1200.0 * (0.01 + (0.1 * ratio) + (0.01 * std::pow(ratio, 4.0)));
  EXPECT_TRUE(rel_near_result(rolling, expected, REL_TOL_CLOSED_FORM));
  EXPECT_NEAR(rolling, -85.87, printed_tol(2));
}

TEST(mf5x_kernels, My_NegativeSpeed_UsesAbsoluteValueForLinearTerm) {
  const coeffs c = s_with({{&coeffs::QSY3, 0.1}});
  const double forward = my_of(c, FZ_NOMINAL, 0.0, SPEED_MPS);
  const double backward = my_of(c, FZ_NOMINAL, 0.0, -SPEED_MPS);
  EXPECT_TRUE(rel_near_result(backward, forward, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, My_LinearInFx) {
  // P12: the slope of My_out versus Fx is -R0*Fz*QSY2*LMY/FNOMIN.
  const coeffs c = s_with({{&coeffs::QSY2, 0.1}, {&coeffs::LMY, 0.9}});
  const double at_zero = my_of(c, FZ_NOMINAL, 0.0, SPEED_MPS);
  const double at_1000 = my_of(c, FZ_NOMINAL, 1000.0, SPEED_MPS);
  const double expected_slope = -0.3 * 4000.0 * 0.1 * 0.9 / 4000.0;
  EXPECT_TRUE(rel_near_result((at_1000 - at_zero) / 1000.0, expected_slope, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, MyFromShifts_MatchesV10) {
  const coeffs c = s_with({{&coeffs::PHX1, 0.001}, {&coeffs::PVX1, 0.01}});
  const point_lon lon = fx0_at(c, FZ_NOMINAL, 0.0);
  EXPECT_TRUE(rel_near_result(kernels::my_from_shifts(c, lon)(0), -36.0, REL_TOL_CLOSED_FORM));
}

TEST(mf5x_kernels, MyFromShifts_NegativeFnomin_NegatesPositiveFnominValue) {
  const coeffs positive = rich_coefficients();
  coeffs negative = positive;
  negative.FNOMIN = -positive.FNOMIN;
  on_each_lane_type(grid_points(true), [&](const auto &in) {
    const auto state = kernels::evaluate_slip_state(positive, in);
    const auto expected = kernels::my_from_shifts(positive, state.lon);
    const auto actual = kernels::my_from_shifts(negative, state.lon);
    ASSERT_EQ(actual.size(), expected.size());
    for (Eigen::Index i = 0; i < actual.size(); ++i) {
      EXPECT_EQ(bits(actual(i)), bits(-expected(i))) << "row " << i;
    }
  });
}

// ---- block lane against point lanes -----------------------------------------------------------------------------

TEST(mf5x_kernels, SineMagicFormula_BlockVsPoint_Agree) {
  expect_block_matches_points(grid_points(false), [](const auto &in) {
    using Lane_T = std::decay_t<decltype(in.fz_n)>;
    const Lane_T b = in.fz_n / 2000.0;
    const Lane_T d = in.fz_n * 0.9;
    const Lane_T e = in.alpha_rad * 2.0;
    samples out;
    append_lane(out, kernels::sine_magic_formula(b, 1.3, d, e, in.kappa), FORCE_SCALE_N);
    return out;
  });
}

TEST(mf5x_kernels, CosineMagicFormula_BlockVsPoint_Agree) {
  expect_block_matches_points(grid_points(false), [](const auto &in) {
    using Lane_T = std::decay_t<decltype(in.fz_n)>;
    const Lane_T b = in.fz_n / 1000.0;
    const Lane_T e = in.alpha_rad * 2.0;
    samples out;
    append_lane(out, kernels::cosine_magic_formula(b, 1.1, e, in.kappa), UNIT_SCALE);
    return out;
  });
}

TEST(mf5x_kernels, VerticalLoad_BlockVsPoint_Agree) {
  const coeffs c = s_with({{&coeffs::LFZO, 1.2}});
  expect_block_matches_points(grid_points(false), [&](const auto &in) {
    samples out;
    append_load(out, kernels::vertical_load(c, in.fz_n));
    return out;
  });
}

TEST(mf5x_kernels, MuY_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    using Lane_T = std::decay_t<decltype(in.fz_n)>;
    const auto load = kernels::vertical_load(c, in.fz_n);
    const Lane_T gamma_y = in.gamma_rad * c.LGAY;
    samples out;
    append_lane(out, kernels::mu_y(c, load, gamma_y), UNIT_SCALE);
    return out;
  });
}

TEST(mf5x_kernels, Fx0_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    const auto load = kernels::vertical_load(c, in.fz_n);
    samples out;
    append_lon(out, kernels::fx0(c, load, in.fz_n, in.kappa, in.gamma_rad));
    return out;
  });
}

TEST(mf5x_kernels, Fy0_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    const auto load = kernels::vertical_load(c, in.fz_n);
    samples out;
    append_lat(out, kernels::fy0(c, load, in.fz_n, in.alpha_rad, in.gamma_rad));
    return out;
  });
}

TEST(mf5x_kernels, Aligning_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    const auto load = kernels::vertical_load(c, in.fz_n);
    const auto lat = kernels::fy0(c, load, in.fz_n, in.alpha_rad, in.gamma_rad);
    samples out;
    append_aln(out, kernels::aligning(c, load, lat, in.fz_n, in.alpha_rad, in.gamma_rad));
    return out;
  });
}

TEST(mf5x_kernels, EvaluateSlipState_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    samples out;
    append_state(out, kernels::evaluate_slip_state(c, in));
    return out;
  });
}

TEST(mf5x_kernels, T_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    using Lane_T = std::decay_t<decltype(in.fz_n)>;
    const auto state = kernels::evaluate_slip_state(c, in);
    const Lane_T cos_alpha = in.alpha_rad.cos();
    samples out;
    append_lane(out, kernels::t(state.aln, state.aln.alpha_t, cos_alpha), UNIT_SCALE);
    return out;
  });
}

TEST(mf5x_kernels, Mzr_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    using Lane_T = std::decay_t<decltype(in.fz_n)>;
    const auto state = kernels::evaluate_slip_state(c, in);
    const Lane_T cos_alpha = in.alpha_rad.cos();
    samples out;
    append_lane(out, kernels::mzr(state.aln, state.aln.alpha_r, cos_alpha), MOMENT_SCALE_NM);
    return out;
  });
}

TEST(mf5x_kernels, AlphaEq_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    using Lane_T = std::decay_t<decltype(in.fz_n)>;
    const auto state = kernels::evaluate_slip_state(c, in);
    const Lane_T ky_guarded_n = kernels::add_epsilon(state.lat.ky_n);
    samples out;
    append_lane(out, kernels::alpha_eq(state.aln.alpha_t, in.kappa, state.lon.kx_n, ky_guarded_n), UNIT_SCALE);
    append_lane(out, kernels::alpha_eq(state.aln.alpha_r, in.kappa, state.lon.kx_n, ky_guarded_n), UNIT_SCALE);
    return out;
  });
}

TEST(mf5x_kernels, Gxa_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    const auto load = kernels::vertical_load(c, in.fz_n);
    samples out;
    append_lane(out, kernels::gxa(c, load, in.kappa, in.alpha_rad), UNIT_SCALE);
    return out;
  });
}

TEST(mf5x_kernels, Gyk_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    const auto load = kernels::vertical_load(c, in.fz_n);
    samples out;
    append_lane(out, kernels::gyk(c, load, in.kappa, in.alpha_rad), UNIT_SCALE);
    return out;
  });
}

TEST(mf5x_kernels, Svyk_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    const auto state = kernels::evaluate_slip_state(c, in);
    using Lane_T = std::decay_t<decltype(in.fz_n)>;
    const Lane_T svyk_n = kernels::svyk(c, state.load, state.lat.mu_y, in.fz_n, in.kappa, in.alpha_rad, in.gamma_rad);
    samples out;
    append_lane(out, svyk_n, FORCE_SCALE_N);
    return out;
  });
}

TEST(mf5x_kernels, CosineCombinedForces_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    const auto state = kernels::evaluate_slip_state(c, in);
    const auto forces = kernels::cosine_combined_forces(c, state, in);
    samples out;
    append_lane(out, forces.fx_n, FORCE_SCALE_N);
    append_lane(out, forces.fy_n, FORCE_SCALE_N);
    append_lane(out, forces.svyk_n, FORCE_SCALE_N);
    return out;
  });
}

TEST(mf5x_kernels, FrictionEllipseRatios_BlockVsPoint_IncludingZeroOverZero_Agree) {
  // Fixture S has no shifts, so the origin row (kappa = 0, alpha = 0) is the 0/0 case next to non-zero slips; the
  // shifted set moves the degenerate point away from the origin.
  for (const coeffs &c : {s_with({}), ellipse_coefficients(), rich_coefficients()}) {
    expect_block_matches_points(grid_points(true), [&](const auto &in) {
      const auto state = kernels::evaluate_slip_state(c, in);
      const auto ratios = kernels::friction_ellipse_ratios(state, in.kappa, in.alpha_rad);
      samples out;
      append_lane(out, ratios.rx, UNIT_SCALE);
      append_lane(out, ratios.ry, UNIT_SCALE);
      return out;
    });
  }
}

TEST(mf5x_kernels, S_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    const auto state = kernels::evaluate_slip_state(c, in);
    samples out;
    append_lane(out, kernels::s(c, state.load, state.lat.fy0_n, in.gamma_rad), UNIT_SCALE);
    return out;
  });
}

TEST(mf5x_kernels, Mx_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    const auto state = kernels::evaluate_slip_state(c, in);
    samples out;
    append_lane(out, kernels::mx(c, in.fz_n, state.lat.fy0_n, in.gamma_rad), MOMENT_SCALE_NM);
    return out;
  });
}

TEST(mf5x_kernels, My_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    const auto state = kernels::evaluate_slip_state(c, in);
    samples out;
    append_lane(out, kernels::my(c, in.fz_n, state.lon.fx0_n, in.vx_mps), MOMENT_SCALE_NM);
    return out;
  });
}

TEST(mf5x_kernels, MyFromShifts_BlockVsPoint_Agree) {
  const coeffs c = rich_coefficients();
  expect_block_matches_points(grid_points(true), [&](const auto &in) {
    const auto state = kernels::evaluate_slip_state(c, in);
    samples out;
    append_lane(out, kernels::my_from_shifts(c, state.lon), MOMENT_SCALE_NM);
    return out;
  });
}

// ---- form dispatch on both lane types ---------------------------------------------------------------------------

TEST(mf5x_kernels, TrailCosine_CosAlpha_PointAndBlock_EqualsCos) {
  on_each_lane_type(grid_points(false), [](const auto &in) {
    using Lane_T = std::decay_t<decltype(in.fz_n)>;
    const Lane_T actual = kernels::trail_cosine(mf::trail_cosine_form::COS_ALPHA, in);
    Lane_T expected = Lane_T::Zero(in.alpha_rad.size());
    for (Eigen::Index i = 0; i < expected.size(); ++i) {
      expected(i) = std::cos(in.alpha_rad(i));
    }
    expect_lanes_near(actual, expected, UNIT_SCALE);
  });
}

TEST(mf5x_kernels, TrailCosine_CosPrime_PointAndBlock_EqualsVxOverHypotPlusEps) {
  on_each_lane_type(grid_points(false), [](const auto &in) {
    using Lane_T = std::decay_t<decltype(in.fz_n)>;
    const Lane_T actual = kernels::trail_cosine(mf::trail_cosine_form::COS_PRIME, in);
    Lane_T expected = Lane_T::Zero(in.alpha_rad.size());
    for (Eigen::Index i = 0; i < expected.size(); ++i) {
      const double vx = in.vx_mps(i);
      expected(i) = vx / (std::hypot(vx, std::tan(in.alpha_rad(i)) * vx) + kernels::EPSILON);
    }
    if constexpr (Lane_T::RowsAtCompileTime == 1) {
      expect_lanes_bits(actual, expected);
    } else {
      expect_lanes_near(actual, expected, UNIT_SCALE);
    }
  });
}

TEST(mf5x_kernels, Combine_Cosine_PointAndBlock_EqualsCosineCombinedForces) {
  const coeffs c = rich_coefficients();
  on_each_lane_type(grid_points(true), [&](const auto &in) {
    const auto state = kernels::evaluate_slip_state(c, in);
    const auto actual = kernels::combine(c, mf::combined_method::COSINE, state, in);
    const auto expected = kernels::cosine_combined_forces(c, state, in);
    expect_lanes_near(actual.fx_n, expected.fx_n, FORCE_SCALE_N);
    expect_lanes_near(actual.fy_n, expected.fy_n, FORCE_SCALE_N);
    expect_lanes_near(actual.svyk_n, expected.svyk_n, FORCE_SCALE_N);
  });
}

TEST(mf5x_kernels, Combine_FrictionEllipse_PointAndBlock_RatiosAndZeroSvyk) {
  const coeffs c = ellipse_coefficients();
  on_each_lane_type(grid_points(true), [&](const auto &in) {
    using Lane_T = std::decay_t<decltype(in.fz_n)>;
    const auto state = kernels::evaluate_slip_state(c, in);
    const auto actual = kernels::combine(c, mf::combined_method::FRICTION_ELLIPSE, state, in);
    const auto ratios = kernels::friction_ellipse_ratios(state, in.kappa, in.alpha_rad);
    const Lane_T expected_fx = state.lon.fx0_n * ratios.rx;
    const Lane_T expected_fy = state.lat.fy0_n * ratios.ry;
    expect_lanes_near(actual.fx_n, expected_fx, FORCE_SCALE_N);
    expect_lanes_near(actual.fy_n, expected_fy, FORCE_SCALE_N);
    ASSERT_EQ(actual.svyk_n.size(), in.fz_n.size());
    for (Eigen::Index i = 0; i < actual.svyk_n.size(); ++i) {
      EXPECT_EQ(bits(actual.svyk_n(i)), bits(0.0));
    }
  });
}

TEST(mf5x_kernels, FyPrime_Svyk_PointAndBlock_FyMinusSvyk) {
  const coeffs c = rich_coefficients();
  on_each_lane_type(grid_points(true), [&](const auto &in) {
    using Lane_T = std::decay_t<decltype(in.fz_n)>;
    const auto state = kernels::evaluate_slip_state(c, in);
    const auto forces = kernels::cosine_combined_forces(c, state, in);
    const Lane_T actual = kernels::fy_prime(c, mf::fy_prime_form::SVYK, state, forces, in);
    const Lane_T expected = forces.fy_n - forces.svyk_n;
    expect_lanes_near(actual, expected, FORCE_SCALE_N);
  });
}

TEST(mf5x_kernels, FyPrime_Svyk0_PointAndBlock_UsesZeroCamberMuY) {
  const coeffs c = rich_coefficients();
  bool differs_from_svyk_form = false;
  on_each_lane_type(grid_points(true), [&](const auto &in) {
    using Lane_T = std::decay_t<decltype(in.fz_n)>;
    const auto state = kernels::evaluate_slip_state(c, in);
    const auto forces = kernels::cosine_combined_forces(c, state, in);
    const Lane_T actual = kernels::fy_prime(c, mf::fy_prime_form::SVYK0, state, forces, in);
    const Lane_T zero_camber = Lane_T::Zero(in.fz_n.size());
    const Lane_T mu_zero_camber = kernels::mu_y(c, state.load, zero_camber);
    const Lane_T svyk0_n = kernels::svyk(c, state.load, mu_zero_camber, in.fz_n, in.kappa, in.alpha_rad, in.gamma_rad);
    const Lane_T expected = forces.fy_n - svyk0_n;
    expect_lanes_near(actual, expected, FORCE_SCALE_N);
    const Lane_T with_svyk = kernels::fy_prime(c, mf::fy_prime_form::SVYK, state, forces, in);
    for (Eigen::Index i = 0; i < actual.size(); ++i) {
      differs_from_svyk_form = differs_from_svyk_form || (bits(actual(i)) != bits(with_svyk(i)));
    }
  });
  // The two forms must differ somewhere on the grid, otherwise the test would not tell them apart.
  EXPECT_TRUE(differs_from_svyk_form);
}

TEST(mf5x_kernels, RollingResistance_Qsy_PointAndBlock_EqualsMy) {
  const coeffs c = rich_coefficients();
  on_each_lane_type(grid_points(true), [&](const auto &in) {
    using Lane_T = std::decay_t<decltype(in.fz_n)>;
    const auto state = kernels::evaluate_slip_state(c, in);
    constexpr mf::rolling_resistance_form form = mf::rolling_resistance_form::QSY;
    const Lane_T actual = kernels::rolling_resistance(c, form, state, state.lon.fx0_n, in);
    const Lane_T expected = kernels::my(c, in.fz_n, state.lon.fx0_n, in.vx_mps);
    expect_lanes_near(actual, expected, MOMENT_SCALE_NM);
  });
}

TEST(mf5x_kernels, RollingResistance_Shifts_PointAndBlock_EqualsMyFromShifts) {
  const coeffs c = rich_coefficients();
  on_each_lane_type(grid_points(true), [&](const auto &in) {
    using Lane_T = std::decay_t<decltype(in.fz_n)>;
    const auto state = kernels::evaluate_slip_state(c, in);
    constexpr mf::rolling_resistance_form form = mf::rolling_resistance_form::SHIFTS;
    const Lane_T actual = kernels::rolling_resistance(c, form, state, state.lon.fx0_n, in);
    const Lane_T expected = kernels::my_from_shifts(c, state.lon);
    expect_lanes_near(actual, expected, MOMENT_SCALE_NM);
  });
}

TEST(mf5x_kernels, Mf5xEvaluate_Combined_PointBitEqualHandComposition_BlockNear) {
  check_evaluate_against_hand_composition(mf::slip_mode::COMBINED);
}

TEST(mf5x_kernels, Mf5xEvaluate_Uncombined_PointBitEqualHandComposition_BlockNear) {
  check_evaluate_against_hand_composition(mf::slip_mode::UNCOMBINED);
}
