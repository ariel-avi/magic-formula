#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <magic_formula/detail/dual_lane.hpp>
#include <magic_formula/detail/lanes.hpp>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_forms.hpp>
#include <string_view>
#include <utility>
#include <vector>

namespace {
  namespace kernels = mf::detail;
  using kernels::block_lane;
  using kernels::dual_lane;
  using kernels::make_dual;
  using kernels::mask_of;
  using kernels::point_lane;
  using mf::test::bits;
  using mf::test::near_scaled;
  using mf::test::rel_near_result;
  using mf::test::REL_TOL_CLOSED_FORM;

  using values_t = std::vector<double>;

  void expect_bits(double actual, double expected) {
    EXPECT_EQ(bits(actual), bits(expected));
  }

  void expect_finite(double value) {
    EXPECT_TRUE(std::isfinite(value));
  }

  void expect_scaled(double actual, double expected, double tol, double scale, std::string_view label) {
    EXPECT_TRUE(near_scaled(actual, expected, tol, scale)) << label;
  }

  // Calls fn(first index, count) once per value for a point lane, and once for all values for a block lane.
  template <class Lane_T, class Fn_T> void for_each_chunk(std::size_t total, Fn_T fn) {
    constexpr bool single = Lane_T::RowsAtCompileTime == 1;
    const std::size_t step = single ? 1 : total;
    for (std::size_t begin = 0; begin < total; begin += step) {
      fn(begin, step);
    }
  }

  template <class Lane_T> Lane_T lane_slice(const values_t &values, std::size_t begin, std::size_t count) {
    Lane_T lane = Lane_T::Zero(static_cast<Eigen::Index>(count));
    for (std::size_t i = 0; i < count; ++i) {
      lane(static_cast<Eigen::Index>(i)) = values[begin + i];
    }
    return lane;
  }

  template <class Lane_T>
  dual_lane<Lane_T> dual_slice(const values_t &values, const values_t &derivatives, std::size_t begin,
                               std::size_t count) {
    return make_dual(lane_slice<Lane_T>(values, begin, count), lane_slice<Lane_T>(derivatives, begin, count));
  }

  template <class Lane_T>
  void expect_dual(const dual_lane<Lane_T> &actual, const values_t &values, const values_t &derivatives,
                   std::size_t begin) {
    for (Eigen::Index i = 0; i < actual.size(); ++i) {
      const std::size_t index = begin + static_cast<std::size_t>(i);
      EXPECT_TRUE(rel_near_result(actual.value(i), values[index], REL_TOL_CLOSED_FORM)) << "value " << index;
      EXPECT_TRUE(rel_near_result(actual.derivative(i), derivatives[index], REL_TOL_CLOSED_FORM))
          << "derivative " << index;
    }
  }

  // Applies op to the dual (values, derivatives) and compares with expected_value(v) and expected_derivative(v, d).
  template <class Lane_T, class Op_T, class Fv_T, class Fd_T>
  void check_unary(const values_t &values, const values_t &derivatives, Op_T op, Fv_T expected_value,
                   Fd_T expected_derivative) {
    for_each_chunk<Lane_T>(values.size(), [&](std::size_t begin, std::size_t count) {
      const dual_lane<Lane_T> x = dual_slice<Lane_T>(values, derivatives, begin, count);
      const dual_lane<Lane_T> result = op(x);
      ASSERT_EQ(result.size(), static_cast<Eigen::Index>(count));
      values_t ev;
      values_t ed;
      for (std::size_t i = 0; i < values.size(); ++i) {
        ev.push_back(expected_value(values[i]));
        ed.push_back(expected_derivative(values[i], derivatives[i]));
      }
      expect_dual(result, ev, ed, begin);
    });
  }

  struct pair_data {
    values_t a_value;
    values_t a_derivative;
    values_t b_value;
    values_t b_derivative;
  };

  template <class Lane_T, class Op_T, class Fv_T, class Fd_T>
  void check_binary(const pair_data &data, Op_T op, Fv_T expected_value, Fd_T expected_derivative) {
    for_each_chunk<Lane_T>(data.a_value.size(), [&](std::size_t begin, std::size_t count) {
      const dual_lane<Lane_T> a = dual_slice<Lane_T>(data.a_value, data.a_derivative, begin, count);
      const dual_lane<Lane_T> b = dual_slice<Lane_T>(data.b_value, data.b_derivative, begin, count);
      const dual_lane<Lane_T> result = op(a, b);
      ASSERT_EQ(result.size(), static_cast<Eigen::Index>(count));
      values_t ev;
      values_t ed;
      for (std::size_t i = 0; i < data.a_value.size(); ++i) {
        const double av = data.a_value[i];
        const double ad = data.a_derivative[i];
        const double bv = data.b_value[i];
        const double bd = data.b_derivative[i];
        ev.push_back(expected_value(av, bv));
        ed.push_back(expected_derivative(av, ad, bv, bd));
      }
      expect_dual(result, ev, ed, begin);
    });
  }

  // Pure-slip state (load, Fx0, Fy0) of the lanes `p`; the aligning terms are not needed by the force combination.
  template <class Lane_T>
  kernels::slip_state<Lane_T> pure_state(const mf::mf5x_coefficients &c, const kernels::input_lanes<Lane_T> &p) {
    kernels::slip_state<Lane_T> state{};
    state.load = kernels::vertical_load(c, p.fz_n);
    state.lon = kernels::fx0(c, state.load, p.fz_n, p.kappa, p.gamma_rad);
    state.lat = kernels::fy0(c, state.load, p.fz_n, p.alpha_rad, p.gamma_rad);
    return state;
  }

  constexpr double FZ_N = 4000.0;
  constexpr double SPEED_MPS = 10.0;

  template <class Lane_T> kernels::input_lanes<Lane_T> plain_inputs(const Lane_T &alpha, double kappa) {
    const Eigen::Index n = alpha.size();
    kernels::input_lanes<Lane_T> in{};
    in.fz_n = Lane_T::Constant(n, FZ_N);
    in.alpha_rad = alpha;
    in.kappa = Lane_T::Constant(n, kappa);
    in.gamma_rad = Lane_T::Zero(n);
    in.vx_mps = Lane_T::Constant(n, SPEED_MPS);
    return in;
  }

  // The seeds of the two slips of a dual point.
  struct slip_seeds {
    double alpha = 1.0;
    double kappa = 0.0;
  };

  // The same point with alpha and kappa seeded (default: alpha 1, kappa 0) and every other input constant.
  template <class Lane_T>
  kernels::input_lanes<dual_lane<Lane_T>> dual_inputs(const Lane_T &alpha, double kappa,
                                                      const slip_seeds &seeds = slip_seeds{}) {
    using dual_t = dual_lane<Lane_T>;
    const Eigen::Index n = alpha.size();
    kernels::input_lanes<dual_t> in{};
    in.fz_n = dual_t::Constant(n, FZ_N);
    const Lane_T alpha_seed = Lane_T::Constant(n, seeds.alpha);
    const Lane_T kappa_value = Lane_T::Constant(n, kappa);
    const Lane_T kappa_seed = Lane_T::Constant(n, seeds.kappa);
    in.alpha_rad = make_dual(alpha, alpha_seed);
    in.kappa = make_dual(kappa_value, kappa_seed);
    in.gamma_rad = dual_t::Zero(n);
    in.vx_mps = dual_t::Constant(n, SPEED_MPS);
    return in;
  }

  template <class Plain_T, class Dual_T>
  void expect_combine_equal(const Plain_T &plain, const Dual_T &dual, Eigen::Index i) {
    expect_bits(dual.fx_n.value(i), plain.fx_n(i));
    expect_bits(dual.fy_n.value(i), plain.fy_n(i));
    expect_bits(dual.svyk_n.value(i), plain.svyk_n(i));
    expect_finite(dual.fy_n.derivative(i));
    expect_finite(dual.fx_n.derivative(i));
  }

  template <class Lane_T>
  void check_combine_value_bit_equal(mf::combined_method method, double kappa, const values_t &alphas) {
    const mf::mf5x_coefficients c = mf::test::fixture_s().coefficients;
    for_each_chunk<Lane_T>(alphas.size(), [&](std::size_t begin, std::size_t count) {
      const auto alpha = lane_slice<Lane_T>(alphas, begin, count);
      const auto plain_in = plain_inputs(alpha, kappa);
      const auto dual_in = dual_inputs<Lane_T>(alpha, kappa);
      const auto plain_state = pure_state(c, plain_in);
      const auto dual_state = pure_state(c, dual_in);
      const auto plain = kernels::combine(c, method, plain_state, plain_in);
      const auto dual = kernels::combine(c, method, dual_state, dual_in);
      const auto plain_ratios = kernels::friction_ellipse_ratios(plain_state, plain_in.kappa, plain_in.alpha_rad);
      const auto dual_ratios = kernels::friction_ellipse_ratios(dual_state, dual_in.kappa, dual_in.alpha_rad);
      for (Eigen::Index i = 0; i < alpha.size(); ++i) {
        expect_combine_equal(plain, dual, i);
        if (method == mf::combined_method::FRICTION_ELLIPSE) {
          expect_bits(dual_ratios.rx.value(i), plain_ratios.rx(i));
          expect_bits(dual_ratios.ry.value(i), plain_ratios.ry(i));
          expect_bits(dual_ratios.fx_n.value(i), plain_ratios.fx_n(i));
          expect_bits(dual_ratios.fy_n.value(i), plain_ratios.fy_n(i));
        }
      }
    });
  }

  // A friction-ellipse evaluation of one chunk: plain and dual ratios and forces at the same points.
  template <class Lane_T> struct ellipse_case {
    Lane_T alpha;
    kernels::slip_state<Lane_T> plain_state;
    kernels::ellipse_ratios<Lane_T> plain;
    kernels::ellipse_ratios<dual_lane<Lane_T>> dual;
  };

  // Calls fn(ellipse_case) for every chunk of `alphas` at a common kappa with the given seeds.
  template <class Lane_T, class Fn_T>
  void for_each_ellipse_case(const mf::mf5x_coefficients &c, const values_t &alphas, double kappa,
                             const slip_seeds &seeds, Fn_T fn) {
    for_each_chunk<Lane_T>(alphas.size(), [&](std::size_t begin, std::size_t count) {
      const auto alpha = lane_slice<Lane_T>(alphas, begin, count);
      const auto plain_in = plain_inputs(alpha, kappa);
      const auto dual_in = dual_inputs<Lane_T>(alpha, kappa, seeds);
      const auto plain_state = pure_state(c, plain_in);
      const auto dual_state = pure_state(c, dual_in);
      const ellipse_case<Lane_T> ec{
          .alpha = alpha,
          .plain_state = plain_state,
          .plain = kernels::friction_ellipse_ratios(plain_state, plain_in.kappa, plain_in.alpha_rad),
          .dual = kernels::friction_ellipse_ratios(dual_state, dual_in.kappa, dual_in.alpha_rad)};
      fn(ec);
    });
  }

  // Closed-form dy/dx of D*sin(C*atan(phi)), phi = u - E*(u - atan(u)), u = B*x.
  double sine_slope(double b, double c, double d, double e, double x) {
    const double u = b * x;
    const double phi = u - (e * (u - std::atan(u)));
    const double theta = std::atan(phi);
    return d * c * std::cos(c * theta) * b * ((1.0 - e) + (e / (1.0 + (u * u)))) / (1.0 + (phi * phi));
  }

  // Closed-form dh/dB of h = cos(C*atan(phi)).
  double cosine_partial_b(double b, double c, double e, double x) {
    const double u = b * x;
    const double phi = u - (e * (u - std::atan(u)));
    const double theta = std::atan(phi);
    return -c * std::sin(c * theta) * x * ((1.0 - e) + (e / (1.0 + (u * u)))) / (1.0 + (phi * phi));
  }

  // Closed-form dh/dx of h = cos(C*atan(phi)), phi = u - E*(u - atan(u)), u = B*x.
  double cosine_partial_x(double b, double c, double e, double x) {
    const double u = b * x;
    const double phi = u - (e * (u - std::atan(u)));
    const double theta = std::atan(phi);
    return -c * std::sin(c * theta) * b * ((1.0 - e) + (e / (1.0 + (u * u)))) / (1.0 + (phi * phi));
  }

  double cosine_shape(double b, double c, double e, double x) {
    const double u = b * x;
    const double phi = u - (e * (u - std::atan(u)));
    return std::cos(c * std::atan(phi));
  }

  double sgn_of(double x) {
    return x >= 0.0 ? 1.0 : -1.0;
  }

  // Partial derivatives with respect to kappa and alpha.
  struct slopes {
    double d_kappa = 0.0;
    double d_alpha = 0.0;
  };

  // The slopes of an Fx-like and an Fy-like quantity.
  struct slope_pair {
    slopes x;
    slopes y;
  };

  // Parameters of a combined-slip weight h(B, x)/h(B, shift) and the derivatives of B and x.
  struct weight_inputs {
    double b = 0.0;
    double db_dkappa = 0.0;
    double db_dalpha = 0.0;
    double c = 0.0;
    double e = 0.0;
    double x = 0.0;
    double dx_dkappa = 0.0;
    double dx_dalpha = 0.0;
    double shift = 0.0;
  };

  double weight_value(const weight_inputs &w) {
    return cosine_shape(w.b, w.c, w.e, w.x) / cosine_shape(w.b, w.c, w.e, w.shift);
  }

  // Quotient rule on h(B(s), x(s))/h(B(s), shift) with the closed-form partials of h.
  slopes weight_slopes(const weight_inputs &w) {
    const double den = cosine_shape(w.b, w.c, w.e, w.shift);
    const double num = cosine_shape(w.b, w.c, w.e, w.x);
    const double num_db = cosine_partial_b(w.b, w.c, w.e, w.x);
    const double den_db = cosine_partial_b(w.b, w.c, w.e, w.shift);
    const double num_dx = cosine_partial_x(w.b, w.c, w.e, w.x);
    const auto slope = [&](double db, double dx) {
      return ((((num_db * db) + (num_dx * dx)) * den) - (num * den_db * db)) / (den * den);
    };
    return slopes{.d_kappa = slope(w.db_dkappa, w.dx_dkappa), .d_alpha = slope(w.db_dalpha, w.dx_dalpha)};
  }

  // The pure-slip scalars of one point that the combined-slip closed forms need.
  struct pure_scalars {
    double bx = 0.0;
    double cx = 0.0;
    double dx = 0.0;
    double ex = 0.0;
    double shx = 0.0;
    double fx0 = 0.0;
    double by = 0.0;
    double cy = 0.0;
    double dy = 0.0;
    double ey = 0.0;
    double shy = 0.0;
    double fy0 = 0.0;
    double mu_y = 0.0;
  };

  template <class Lane_T> pure_scalars scalars_at(const kernels::slip_state<Lane_T> &state, Eigen::Index i) {
    return pure_scalars{.bx = state.lon.bx(i),
                        .cx = state.lon.cx,
                        .dx = state.lon.dx_n(i),
                        .ex = state.lon.ex(i),
                        .shx = state.lon.shx(i),
                        .fx0 = state.lon.fx0_n(i),
                        .by = state.lat.by(i),
                        .cy = state.lat.cy,
                        .dy = state.lat.dy_n(i),
                        .ey = state.lat.ey(i),
                        .shy = state.lat.shy(i),
                        .fy0 = state.lat.fy0_n(i),
                        .mu_y = state.lat.mu_y(i)};
  }

  // A fixture-S variant whose combined-slip weights depend on the swept slips (RBX2, RBY2, RBY3, RVY*) and whose
  // curvature and shift terms are nonzero. Fz = FNOMIN in the tests, so every dfz term vanishes.
  mf::mf5x_coefficients combined_coefficients() {
    mf::mf5x_coefficients c = mf::test::fixture_s().coefficients;
    c.PEX1 = -0.5;
    c.PEY1 = -0.4;
    c.PHX1 = 0.001;
    c.PHY1 = 0.002;
    c.RBX2 = 6.0;
    c.REX1 = -0.3;
    c.RHX1 = 0.002;
    c.RBY2 = 5.0;
    c.RBY3 = 0.02;
    c.REY1 = -0.2;
    c.RHY1 = 0.01;
    c.RVY1 = 0.05;
    c.RVY4 = 2.0;
    c.RVY5 = 1.0;
    c.RVY6 = 10.0;
    return c;
  }

  // A slip point and which slip carries the unit seed.
  struct slip_point {
    double alpha = 0.0;
    double kappa = 0.0;
    bool seed_alpha = true;
  };

  double select_slope(const slopes &s, bool seed_alpha) {
    return seed_alpha ? s.d_alpha : s.d_kappa;
  }

  // Closed-form dFx/ds and dFy/ds of the cosine method: Fx = Fx0*Gxa, Fy = Fy0*Gyk + SVyk.
  slope_pair cosine_slopes(const mf::mf5x_coefficients &c, const pure_scalars &p, double alpha, double kappa) {
    const double fx0_dkappa = sine_slope(p.bx, p.cx, p.dx, p.ex, kappa + p.shx);
    const double fy0_dalpha = sine_slope(p.by, p.cy, p.dy, p.ey, alpha + p.shy);

    const double ux = c.RBX2 * kappa;
    const weight_inputs wx{.b = c.RBX1 * std::cos(std::atan(ux)) * c.LXAL,
                           .db_dkappa = c.RBX1 * c.LXAL * (-ux * c.RBX2) * std::pow(1.0 + (ux * ux), -1.5),
                           .db_dalpha = 0.0,
                           .c = c.RCX1,
                           .e = c.REX1,
                           .x = alpha + c.RHX1,
                           .dx_dkappa = 0.0,
                           .dx_dalpha = 1.0,
                           .shift = c.RHX1};
    const double uy = c.RBY2 * (alpha - c.RBY3);
    const weight_inputs wy{.b = c.RBY1 * std::cos(std::atan(uy)) * c.LYKA,
                           .db_dkappa = 0.0,
                           .db_dalpha = c.RBY1 * c.LYKA * (-uy * c.RBY2) * std::pow(1.0 + (uy * uy), -1.5),
                           .c = c.RCY1,
                           .e = c.REY1,
                           .x = kappa + c.RHY1,
                           .dx_dkappa = 1.0,
                           .dx_dalpha = 0.0,
                           .shift = c.RHY1};
    const double gxa = weight_value(wx);
    const double gyk = weight_value(wy);
    const slopes gxa_slopes = weight_slopes(wx);
    const slopes gyk_slopes = weight_slopes(wy);

    const double w = c.RVY4 * alpha;
    const double v = c.RVY6 * kappa;
    const double dvyk = p.mu_y * FZ_N * c.RVY1 * std::cos(std::atan(w));
    const double dvyk_dalpha = p.mu_y * FZ_N * c.RVY1 * (-w * c.RVY4) * std::pow(1.0 + (w * w), -1.5);
    const double shape = std::sin(c.RVY5 * std::atan(v));
    const double shape_dkappa = std::cos(c.RVY5 * std::atan(v)) * c.RVY5 * c.RVY6 / (1.0 + (v * v));
    const double svyk_dalpha = dvyk_dalpha * shape * c.LVYKA;
    const double svyk_dkappa = dvyk * shape_dkappa * c.LVYKA;

    const slopes fx{.d_kappa = (fx0_dkappa * gxa) + (p.fx0 * gxa_slopes.d_kappa),
                    .d_alpha = p.fx0 * gxa_slopes.d_alpha};
    const slopes fy{.d_kappa = (p.fy0 * gyk_slopes.d_kappa) + svyk_dkappa,
                    .d_alpha = (fy0_dalpha * gyk) + (p.fy0 * gyk_slopes.d_alpha) + svyk_dalpha};
    return slope_pair{.x = fx, .y = fy};
  }

  // d(N/sqrt(Q)) from the slopes of N and Q.
  double root_ratio_slope(double n, double dn, double q, double dq) {
    const double root = std::sqrt(q);
    return ((dn * root) - (n * dq / (2.0 * root))) / q;
  }

  // Closed-form slopes of the friction-ellipse ratios rx (x) and ry (y); SVx = SVy = 0 in the fixtures.
  slope_pair ellipse_slopes(const pure_scalars &p, double alpha, double kappa) {
    const double fx0_dkappa = sine_slope(p.bx, p.cx, p.dx, p.ex, kappa + p.shx);
    const double fy0_dalpha = sine_slope(p.by, p.cy, p.dy, p.ey, alpha + p.shy);
    const double kc = kappa + p.shx;
    const double alpha_c = alpha + p.shy;
    const double astar = std::sin(alpha_c);
    const double astar_dalpha = std::cos(alpha_c);
    const double mxa = std::abs(p.fx0);
    const double mxa_dkappa = sgn_of(p.fx0) * fx0_dkappa;
    const double mya = std::abs(p.fy0);
    const double mya_dalpha = sgn_of(p.fy0) * fy0_dalpha;
    const double mxm = std::abs(p.dx);
    const double mym = std::abs(p.dy);

    const double nx = std::abs(kc) * mym;
    const double nx_dkappa = sgn_of(kc) * mym;
    const double qx = (kc * kc * mym * mym) + (mxa * mxa * astar * astar);
    const double qx_dkappa = (2.0 * kc * mym * mym) + (2.0 * mxa * mxa_dkappa * astar * astar);
    const double qx_dalpha = 2.0 * mxa * mxa * astar * astar_dalpha;

    const double ny = std::abs(astar) * mxm;
    const double ny_dalpha = sgn_of(astar) * astar_dalpha * mxm;
    const double qy = (kc * kc * mya * mya) + (astar * astar * mxm * mxm);
    const double qy_dkappa = 2.0 * kc * mya * mya;
    const double qy_dalpha = (2.0 * kc * kc * mya * mya_dalpha) + (2.0 * astar * astar_dalpha * mxm * mxm);

    const slopes rx{.d_kappa = root_ratio_slope(nx, nx_dkappa, qx, qx_dkappa),
                    .d_alpha = root_ratio_slope(nx, 0.0, qx, qx_dalpha)};
    const slopes ry{.d_kappa = root_ratio_slope(ny, 0.0, qy, qy_dkappa),
                    .d_alpha = root_ratio_slope(ny, ny_dalpha, qy, qy_dalpha)};
    return slope_pair{.x = rx, .y = ry};
  }

  // Every slope is compared relative to its own magnitude at the closed-form tolerance: the dual path and the closed
  // forms are the same chain rule, so they differ by a few ULP of the terms (about 1e-15 relative); 1e-12 keeps
  // three orders of margin for the libm differences without hiding an unstable rewrite of a small cross term.
  constexpr double SLOPE_TOL = mf::test::REL_TOL_CLOSED_FORM;

  // Compares the dual forces and ratios of lane element i with the closed forms.
  template <class Combined_T, class Ratios_T>
  void expect_slopes_at(const mf::mf5x_coefficients &c, const pure_scalars &p, const slip_point &pt,
                        const Combined_T &cosine, const Ratios_T &ratios, Eigen::Index i) {
    const slope_pair cs = cosine_slopes(c, p, pt.alpha, pt.kappa);
    const slope_pair es = ellipse_slopes(p, pt.alpha, pt.kappa);
    expect_scaled(cosine.fx_n.derivative(i), select_slope(cs.x, pt.seed_alpha), SLOPE_TOL, 0.0, "fx");
    expect_scaled(cosine.fy_n.derivative(i), select_slope(cs.y, pt.seed_alpha), SLOPE_TOL, 0.0, "fy");
    expect_scaled(ratios.rx.derivative(i), select_slope(es.x, pt.seed_alpha), SLOPE_TOL, 0.0, "rx");
    expect_scaled(ratios.ry.derivative(i), select_slope(es.y, pt.seed_alpha), SLOPE_TOL, 0.0, "ry");
  }

  // Seeds alpha (or kappa) and compares the dual cosine-method forces and ellipse ratios with the closed forms.
  template <class Lane_T> void check_combined_slopes(double kappa, bool seed_alpha) {
    const mf::mf5x_coefficients c = combined_coefficients();
    const values_t alphas{0.05, -0.1};
    const slip_seeds seeds{.alpha = seed_alpha ? 1.0 : 0.0, .kappa = seed_alpha ? 0.0 : 1.0};
    for_each_chunk<Lane_T>(alphas.size(), [&](std::size_t begin, std::size_t count) {
      const auto alpha = lane_slice<Lane_T>(alphas, begin, count);
      const auto plain_in = plain_inputs(alpha, kappa);
      const auto dual_in = dual_inputs<Lane_T>(alpha, kappa, seeds);
      const auto plain_state = pure_state(c, plain_in);
      const auto dual_state = pure_state(c, dual_in);
      const auto cosine = kernels::combine(c, mf::combined_method::COSINE, dual_state, dual_in);
      const auto ratios = kernels::friction_ellipse_ratios(dual_state, dual_in.kappa, dual_in.alpha_rad);
      for (Eigen::Index i = 0; i < alpha.size(); ++i) {
        const slip_point pt{.alpha = alpha(i), .kappa = kappa, .seed_alpha = seed_alpha};
        expect_slopes_at(c, scalars_at(plain_state, i), pt, cosine, ratios, i);
      }
    });
  }

  constexpr double TOL_CLOSED = 1e-14;

  values_t x_grid() {
    return {-0.2, 0.0, 0.05, 0.3};
  }

  values_t e_grid() {
    return {0.0, 0.3, -0.5};
  }

  template <class Lane_T> void check_select(const values_t &flags) {
    const values_t tv{1.0, 2.0, 3.0, 4.0};
    const values_t td{0.1, 0.2, 0.3, 0.4};
    const values_t fv{-1.0, -2.0, -3.0, -4.0};
    const values_t fd{-0.1, -0.2, -0.3, -0.4};
    for_each_chunk<Lane_T>(flags.size(), [&](std::size_t begin, std::size_t count) {
      const dual_lane<Lane_T> if_true = dual_slice<Lane_T>(tv, td, begin, count);
      const dual_lane<Lane_T> if_false = dual_slice<Lane_T>(fv, fd, begin, count);
      const mask_of<Lane_T> mask = lane_slice<Lane_T>(flags, begin, count) > 0.5;
      const dual_lane<Lane_T> result = select_lanes(mask, if_true, if_false);
      ASSERT_EQ(result.size(), static_cast<Eigen::Index>(count));
      for (Eigen::Index i = 0; i < result.size(); ++i) {
        const std::size_t index = begin + static_cast<std::size_t>(i);
        const bool chosen = flags[index] > 0.5;
        expect_bits(result.value(i), chosen ? tv[index] : fv[index]);
        expect_bits(result.derivative(i), chosen ? td[index] : fd[index]);
      }
    });
  }

  double sgn_plus_value(double v) {
    return sgn_of(v);
  }

  double add_epsilon_value(double v) {
    return v + (kernels::EPSILON * sgn_of(v));
  }

  double limit_value(double v) {
    return v < 1.0 ? v : 1.0;
  }

  double limit_slope(double v, double d) {
    return v < 1.0 ? d : 0.0;
  }

} // namespace

template <class Lane_T> class dual_lane_ops : public ::testing::Test {};
using lane_types = ::testing::Types<mf::detail::point_lane, mf::detail::block_lane>;
TYPED_TEST_SUITE(dual_lane_ops, lane_types);

TYPED_TEST(dual_lane_ops, Constant_AnySize_ZeroDerivative) {
  using lane_t = TypeParam;
  const Eigen::Index n = lane_t::RowsAtCompileTime == 1 ? 1 : 4;
  const dual_lane<lane_t> x = dual_lane<lane_t>::Constant(n, 2.5);
  ASSERT_EQ(x.size(), n);
  ASSERT_EQ(x.derivative.size(), n);
  for (Eigen::Index i = 0; i < n; ++i) {
    expect_bits(x.value(i), 2.5);
    expect_bits(x.derivative(i), 0.0);
  }
}

TYPED_TEST(dual_lane_ops, Zero_ReturnsZeroValueAndDerivative) {
  using lane_t = TypeParam;
  const Eigen::Index n = lane_t::RowsAtCompileTime == 1 ? 1 : 3;
  const dual_lane<lane_t> x = dual_lane<lane_t>::Zero(n);
  ASSERT_EQ(x.size(), n);
  for (Eigen::Index i = 0; i < n; ++i) {
    expect_bits(x.value(i), 0.0);
    expect_bits(x.derivative(i), 0.0);
  }
}

TYPED_TEST(dual_lane_ops, Size_EqualsValueSize) {
  using lane_t = TypeParam;
  const Eigen::Index n = lane_t::RowsAtCompileTime == 1 ? 1 : 5;
  const lane_t value = lane_t::Constant(n, 1.0);
  const lane_t derivative = lane_t::Constant(n, 2.0);
  EXPECT_EQ(make_dual(value, derivative).size(), value.size());
}

TYPED_TEST(dual_lane_ops, MakeDual_StoresValueAndDerivative) {
  using lane_t = TypeParam;
  const Eigen::Index n = lane_t::RowsAtCompileTime == 1 ? 1 : 2;
  const lane_t value = lane_t::Constant(n, 1.25);
  const lane_t derivative = lane_t::Constant(n, -3.5);
  const dual_lane<lane_t> x = make_dual(value, derivative);
  for (Eigen::Index i = 0; i < n; ++i) {
    expect_bits(x.value(i), 1.25);
    expect_bits(x.derivative(i), -3.5);
  }
}

TYPED_TEST(dual_lane_ops, ArithmeticDualDual_AddSubMulDiv_ChainRule) {
  using lane_t = TypeParam;
  const pair_data data{.a_value = {1.5, -0.5, 2.0},
                       .a_derivative = {0.3, -1.1, 0.2},
                       .b_value = {-2.0, 0.4, 3.0},
                       .b_derivative = {0.7, 0.9, -0.6}};
  check_binary<lane_t>(
      data, [](const auto &a, const auto &b) { return a + b; }, [](double av, double bv) { return av + bv; },
      [](double, double ad, double, double bd) { return ad + bd; });
  check_binary<lane_t>(
      data, [](const auto &a, const auto &b) { return a - b; }, [](double av, double bv) { return av - bv; },
      [](double, double ad, double, double bd) { return ad - bd; });
  check_binary<lane_t>(
      data, [](const auto &a, const auto &b) { return a * b; }, [](double av, double bv) { return av * bv; },
      [](double av, double ad, double bv, double bd) { return (ad * bv) + (av * bd); });
  check_binary<lane_t>(
      data, [](const auto &a, const auto &b) { return a / b; }, [](double av, double bv) { return av / bv; },
      [](double av, double ad, double bv, double bd) { return ((ad * bv) - (av * bd)) / (bv * bv); });
}

TYPED_TEST(dual_lane_ops, ArithmeticDualScalar_AllSevenForms_ChainRule) {
  using lane_t = TypeParam;
  constexpr double x = 2.5;
  const values_t values{1.5, -0.5, 2.0};
  const values_t derivatives{0.3, -1.1, 0.2};
  check_unary<lane_t>(
      values, derivatives, [](const auto &a) { return a + x; }, [](double v) { return v + x; },
      [](double, double d) { return d; });
  check_unary<lane_t>(
      values, derivatives, [](const auto &a) { return x + a; }, [](double v) { return x + v; },
      [](double, double d) { return d; });
  check_unary<lane_t>(
      values, derivatives, [](const auto &a) { return a - x; }, [](double v) { return v - x; },
      [](double, double d) { return d; });
  check_unary<lane_t>(
      values, derivatives, [](const auto &a) { return x - a; }, [](double v) { return x - v; },
      [](double, double d) { return -d; });
  check_unary<lane_t>(
      values, derivatives, [](const auto &a) { return a * x; }, [](double v) { return v * x; },
      [](double, double d) { return d * x; });
  check_unary<lane_t>(
      values, derivatives, [](const auto &a) { return x * a; }, [](double v) { return x * v; },
      [](double, double d) { return x * d; });
  check_unary<lane_t>(
      values, derivatives, [](const auto &a) { return a / x; }, [](double v) { return v / x; },
      [](double, double d) { return d / x; });
}

TYPED_TEST(dual_lane_ops, Atan_Sin_Cos_Exp_Sqrt_ChainRule) {
  using lane_t = TypeParam;
  const values_t values{0.7, -0.4, 1.9};
  const values_t derivatives{-1.3, 0.8, 0.5};
  check_unary<lane_t>(
      values, derivatives, [](const auto &a) { return a.atan(); }, [](double v) { return std::atan(v); },
      [](double v, double d) { return d / (1.0 + (v * v)); });
  check_unary<lane_t>(
      values, derivatives, [](const auto &a) { return a.sin(); }, [](double v) { return std::sin(v); },
      [](double v, double d) { return d * std::cos(v); });
  check_unary<lane_t>(
      values, derivatives, [](const auto &a) { return a.cos(); }, [](double v) { return std::cos(v); },
      [](double v, double d) { return -d * std::sin(v); });
  check_unary<lane_t>(
      values, derivatives, [](const auto &a) { return a.exp(); }, [](double v) { return std::exp(v); },
      [](double v, double d) { return d * std::exp(v); });
  const values_t positive{0.7, 0.4, 1.9};
  check_unary<lane_t>(
      positive, derivatives, [](const auto &a) { return a.sqrt(); }, [](double v) { return std::sqrt(v); },
      [](double v, double d) { return d / (2.0 * std::sqrt(v)); });
}

TYPED_TEST(dual_lane_ops, Abs_NegativeValue_NegatedDerivative) {
  using lane_t = TypeParam;
  check_unary<lane_t>(
      {-0.5, -2.0, -0.1}, {0.8, -0.3, 1.5}, [](const auto &a) { return a.abs(); }, [](double v) { return -v; },
      [](double, double d) { return -d; });
}

TYPED_TEST(dual_lane_ops, Abs_PositiveAndNegativeZero_RightDerivative) {
  using lane_t = TypeParam;
  check_unary<lane_t>(
      {+0.0, -0.0}, {0.8, 0.8}, [](const auto &a) { return a.abs(); }, [](double) { return 0.0; },
      [](double, double d) { return d; });
}

TYPED_TEST(dual_lane_ops, Abs_PositiveValue_Derivative) {
  using lane_t = TypeParam;
  check_unary<lane_t>(
      {0.5, 2.0, 0.1}, {0.8, -0.3, 1.5}, [](const auto &a) { return a.abs(); }, [](double v) { return v; },
      [](double, double d) { return d; });
}

TYPED_TEST(dual_lane_ops, Min_BelowBound_PassesDerivative) {
  using lane_t = TypeParam;
  check_unary<lane_t>(
      {0.5, 0.2}, {0.8, -0.3}, [](const auto &a) { return a.min(1.0); }, [](double v) { return v; },
      [](double, double d) { return d; });
}

TYPED_TEST(dual_lane_ops, Min_AboveBound_ZeroDerivative) {
  using lane_t = TypeParam;
  check_unary<lane_t>(
      {1.5, 3.0}, {0.8, -0.3}, [](const auto &a) { return a.min(1.0); }, [](double) { return 1.0; },
      [](double, double) { return 0.0; });
}

TYPED_TEST(dual_lane_ops, Min_AtBound_ZeroDerivative) {
  using lane_t = TypeParam;
  check_unary<lane_t>(
      {1.0, 1.0}, {0.8, -0.3}, [](const auto &a) { return a.min(1.0); }, [](double) { return 1.0; },
      [](double, double) { return 0.0; });
}

TYPED_TEST(dual_lane_ops, Comparisons_GreaterEqualAndGreater_BothOutcomes) {
  using lane_t = TypeParam;
  const values_t values{-1.0, 0.0, 1.0};
  const values_t derivatives{1.0, 1.0, 1.0};
  const std::vector<bool> expected_ge{false, true, true};
  const std::vector<bool> expected_gt{false, false, true};
  for_each_chunk<lane_t>(values.size(), [&](std::size_t begin, std::size_t count) {
    const dual_lane<lane_t> x = dual_slice<lane_t>(values, derivatives, begin, count);
    const mask_of<lane_t> ge = x >= 0.0;
    const mask_of<lane_t> gt = x > 0.0;
    for (std::size_t i = 0; i < count; ++i) {
      EXPECT_EQ(ge(static_cast<Eigen::Index>(i)), expected_ge[begin + i]);
      EXPECT_EQ(gt(static_cast<Eigen::Index>(i)), expected_gt[begin + i]);
    }
  });
}

TYPED_TEST(dual_lane_ops, SelectLanes_Dual_TrueFalseAndMixed_SelectsValueAndDerivative) {
  using lane_t = TypeParam;
  check_select<lane_t>({1.0, 1.0, 1.0, 1.0});
  check_select<lane_t>({0.0, 0.0, 0.0, 0.0});
  check_select<lane_t>({1.0, 0.0, 1.0, 0.0});
}

TYPED_TEST(dual_lane_ops, SgnPlus_Dual_UnitValueZeroDerivative) {
  using lane_t = TypeParam;
  check_unary<lane_t>(
      {-2.0, 0.0, 3.0}, {0.5, 0.5, 0.5}, [](const auto &a) { return kernels::sgn_plus(a); }, sgn_plus_value,
      [](double, double) { return 0.0; });
}

TYPED_TEST(dual_lane_ops, AddEpsilon_Dual_DerivativePassesThrough) {
  using lane_t = TypeParam;
  check_unary<lane_t>(
      {0.0, -1.0, 2.0}, {0.4, 0.4, 0.4}, [](const auto &a) { return kernels::add_epsilon(a); }, add_epsilon_value,
      [](double, double d) { return d; });
}

TYPED_TEST(dual_lane_ops, LimitCurvature_Dual_BelowOnePasses_AboveOneZeroDerivative) {
  using lane_t = TypeParam;
  check_unary<lane_t>(
      {0.4, 1.5}, {0.9, 0.9}, [](const auto &a) { return kernels::limit_curvature(a); }, limit_value, limit_slope);
}

TYPED_TEST(dual_lane_ops, SineMagicFormula_DualSlip_EqualsClosedFormDerivative) {
  using lane_t = TypeParam;
  constexpr double b = 8.0;
  constexpr double c = 1.5;
  constexpr double d = 4000.0;
  const values_t xs = x_grid();
  for (const double e : e_grid()) {
    for_each_chunk<lane_t>(xs.size(), [&](std::size_t begin, std::size_t count) {
      const auto x_lane = lane_slice<lane_t>(xs, begin, count);
      const Eigen::Index n = x_lane.size();
      const lane_t seed = lane_t::Constant(n, 1.0);
      const dual_lane<lane_t> x = make_dual(x_lane, seed);
      const dual_lane<lane_t> b_dual = dual_lane<lane_t>::Constant(n, b);
      const dual_lane<lane_t> d_dual = dual_lane<lane_t>::Constant(n, d);
      const dual_lane<lane_t> e_dual = dual_lane<lane_t>::Constant(n, e);
      const dual_lane<lane_t> y = kernels::sine_magic_formula(b_dual, c, d_dual, e_dual, x);
      const lane_t b_lane = lane_t::Constant(n, b);
      const lane_t d_lane = lane_t::Constant(n, d);
      const lane_t e_lane = lane_t::Constant(n, e);
      const lane_t plain = kernels::sine_magic_formula(b_lane, c, d_lane, e_lane, x_lane);
      for (Eigen::Index i = 0; i < n; ++i) {
        expect_bits(y.value(i), plain(i));
        expect_scaled(y.derivative(i), sine_slope(b, c, d, e, x_lane(i)), TOL_CLOSED, d * b, "sine slope");
      }
    });
  }
}

TYPED_TEST(dual_lane_ops, CosineMagicFormula_DualB_EqualsClosedFormPartialB) {
  using lane_t = TypeParam;
  constexpr double b = 8.0;
  constexpr double c = 1.5;
  const values_t xs = x_grid();
  for (const double e : e_grid()) {
    for_each_chunk<lane_t>(xs.size(), [&](std::size_t begin, std::size_t count) {
      const auto x_lane = lane_slice<lane_t>(xs, begin, count);
      const Eigen::Index n = x_lane.size();
      const lane_t b_lane = lane_t::Constant(n, b);
      const lane_t seed = lane_t::Constant(n, 1.0);
      const lane_t no_seed = lane_t::Zero(n);
      const dual_lane<lane_t> b_dual = make_dual(b_lane, seed);
      const dual_lane<lane_t> e_dual = dual_lane<lane_t>::Constant(n, e);
      const dual_lane<lane_t> x_const = make_dual(x_lane, no_seed);
      const dual_lane<lane_t> h = kernels::cosine_magic_formula(b_dual, c, e_dual, x_const);
      const lane_t e_lane = lane_t::Constant(n, e);
      const lane_t plain = kernels::cosine_magic_formula(b_lane, c, e_lane, x_lane);
      for (Eigen::Index i = 0; i < n; ++i) {
        expect_bits(h.value(i), plain(i));
        expect_scaled(h.derivative(i), cosine_partial_b(b, c, e, x_lane(i)), TOL_CLOSED, std::abs(x_lane(i)) * c,
                      "cosine partial b");
      }
    });
  }
}

TYPED_TEST(dual_lane_ops, Combine_BothMethodsBothLanes_ValueBitEqualsPlainKernel) {
  using lane_t = TypeParam;
  for (const mf::combined_method method : {mf::combined_method::COSINE, mf::combined_method::FRICTION_ELLIPSE}) {
    check_combine_value_bit_equal<lane_t>(method, 0.1, {0.05, 0.1});
    check_combine_value_bit_equal<lane_t>(method, 0.0, {0.05});
    check_combine_value_bit_equal<lane_t>(method, 0.1, {0.0});
    check_combine_value_bit_equal<lane_t>(method, 0.0, {0.0});
  }
}

TYPED_TEST(dual_lane_ops, CombinedSlip_AlphaSeed_EqualsClosedFormSlopes) {
  using lane_t = TypeParam;
  check_combined_slopes<lane_t>(0.1, true);
  check_combined_slopes<lane_t>(-0.07, true);
}

TYPED_TEST(dual_lane_ops, CombinedSlip_KappaSeed_EqualsClosedFormSlopes) {
  using lane_t = TypeParam;
  check_combined_slopes<lane_t>(0.1, false);
  check_combined_slopes<lane_t>(-0.07, false);
}

TYPED_TEST(dual_lane_ops, FrictionEllipse_DualZeroDenominator_RatioOneDerivativeZero) {
  using lane_t = TypeParam;
  const mf::mf5x_coefficients c = mf::test::fixture_s().coefficients;
  const values_t alphas{0.0, 0.0};
  for_each_chunk<lane_t>(alphas.size(), [&](std::size_t begin, std::size_t count) {
    const auto alpha = lane_slice<lane_t>(alphas, begin, count);
    const auto in = dual_inputs<lane_t>(alpha, 0.0);
    const auto ratios = kernels::friction_ellipse_ratios(pure_state(c, in), in.kappa, in.alpha_rad);
    for (Eigen::Index i = 0; i < alpha.size(); ++i) {
      expect_bits(ratios.rx.value(i), 1.0);
      expect_bits(ratios.rx.derivative(i), 0.0);
      expect_bits(ratios.ry.value(i), 1.0);
      expect_bits(ratios.ry.derivative(i), 0.0);
    }
  });
}

namespace {

  // Fx0'(0) and Fy0'(0) are the pure-slip slopes at the shifted origins.
  double fx0_slope_at(const pure_scalars &p, double kappa) {
    return sine_slope(p.bx, p.cx, p.dx, p.ex, kappa + p.shx);
  }

  double fy0_slope_at(const pure_scalars &p, double alpha) {
    return sine_slope(p.by, p.cy, p.dy, p.ey, alpha + p.shy);
  }

  // Force slopes of the friction ellipse for the points (alphas, kappa), in point order.
  struct force_slope_list {
    values_t fx;
    values_t fy;
  };

  template <class Lane_T>
  force_slope_list ellipse_force_slopes(const values_t &alphas, double kappa, const slip_seeds &seeds) {
    const mf::mf5x_coefficients c = mf::test::fixture_s().coefficients;
    force_slope_list slopes_out;
    for_each_ellipse_case<Lane_T>(c, alphas, kappa, seeds, [&](const ellipse_case<Lane_T> &ec) {
      for (Eigen::Index i = 0; i < ec.alpha.size(); ++i) {
        slopes_out.fx.push_back(ec.dual.fx_n.derivative(i));
        slopes_out.fy.push_back(ec.dual.fy_n.derivative(i));
      }
    });
    return slopes_out;
  }

  constexpr double SEED_TOL = 1e-14;

  // The force slope list of one channel at twice the seed equals twice the slope list at the unit seed.
  template <class Lane_T>
  void expect_seed_doubles(const values_t &alphas, double kappa, const slip_seeds &unit_seed,
                           const slip_seeds &twice_seed, values_t force_slope_list::*channel) {
    const force_slope_list unit = ellipse_force_slopes<Lane_T>(alphas, kappa, unit_seed);
    const force_slope_list twice = ellipse_force_slopes<Lane_T>(alphas, kappa, twice_seed);
    const values_t &unit_values = unit.*channel;
    const values_t &twice_values = twice.*channel;
    ASSERT_EQ(unit_values.size(), alphas.size());
    ASSERT_EQ(twice_values.size(), alphas.size());
    for (std::size_t i = 0; i < unit_values.size(); ++i) {
      EXPECT_NE(unit_values[i], 0.0);
      EXPECT_TRUE(rel_near_result(twice_values[i], 2.0 * unit_values[i], SEED_TOL));
    }
  }

  // Force slopes at a point where both friction-ellipse ratios are defined follow the product rule.
  template <class Lane_T>
  void check_defined_ratio_slope(const mf::mf5x_coefficients &c, double kappa, double alpha_value, bool seed_alpha) {
    const slip_seeds seeds{.alpha = seed_alpha ? 1.0 : 0.0, .kappa = seed_alpha ? 0.0 : 1.0};
    const values_t alphas{alpha_value, alpha_value};
    for_each_ellipse_case<Lane_T>(c, alphas, kappa, seeds, [&](const ellipse_case<Lane_T> &ec) {
      for (Eigen::Index i = 0; i < ec.alpha.size(); ++i) {
        const pure_scalars p = scalars_at(ec.plain_state, i);
        const slope_pair es = ellipse_slopes(p, alpha_value, kappa);
        const double fx0_slope = seed_alpha ? 0.0 : fx0_slope_at(p, kappa);
        const double fy0_slope = seed_alpha ? fy0_slope_at(p, alpha_value) : 0.0;
        const double rx = ec.dual.rx.value(i);
        const double ry = ec.dual.ry.value(i);
        const double expected_x = (fx0_slope * rx) + (p.fx0 * select_slope(es.x, seed_alpha));
        const double expected_y = (fy0_slope * ry) + (p.fy0 * select_slope(es.y, seed_alpha));
        expect_scaled(ec.dual.fx_n.derivative(i), expected_x, SLOPE_TOL, 0.0, "fx");
        expect_scaled(ec.dual.fy_n.derivative(i), expected_y, SLOPE_TOL, 0.0, "fy");
      }
    });
  }

} // namespace

TYPED_TEST(dual_lane_ops, FrictionEllipse_KappaZeroPoint_ForceSlopeIsDirectionalLimit) {
  using lane_t = TypeParam;
  const mf::mf5x_coefficients c = mf::test::fixture_s().coefficients;
  const values_t alphas{0.05, -0.08};
  for_each_ellipse_case<lane_t>(
      c, alphas, 0.0, slip_seeds{.alpha = 0.0, .kappa = 1.0}, [&](const ellipse_case<lane_t> &ec) {
        for (Eigen::Index i = 0; i < ec.alpha.size(); ++i) {
          const pure_scalars p = scalars_at(ec.plain_state, i);
          const double fx0_slope = fx0_slope_at(p, 0.0);
          const double astar = std::sin(ec.alpha(i) + p.shy);
          const double mym = std::abs(p.dy);
          const double expected = fx0_slope * mym / std::sqrt((mym * mym) + (fx0_slope * astar * fx0_slope * astar));
          expect_bits(ec.dual.rx.value(i), 1.0);
          expect_bits(ec.dual.rx.derivative(i), 0.0);
          expect_bits(ec.dual.fx_n.value(i), ec.plain.fx_n(i));
          EXPECT_TRUE(rel_near_result(ec.dual.fx_n.derivative(i), expected, REL_TOL_CLOSED_FORM));
        }
      });
}

TYPED_TEST(dual_lane_ops, FrictionEllipse_AlphaZeroPoint_ForceSlopeIsDirectionalLimit) {
  using lane_t = TypeParam;
  const mf::mf5x_coefficients c = mf::test::fixture_s().coefficients;
  const values_t alphas{0.0, 0.0};
  for (const double kappa : {0.1, -0.06}) {
    for_each_ellipse_case<lane_t>(c, alphas, kappa, slip_seeds{}, [&](const ellipse_case<lane_t> &ec) {
      for (Eigen::Index i = 0; i < ec.alpha.size(); ++i) {
        const pure_scalars p = scalars_at(ec.plain_state, i);
        const double fy0_slope = fy0_slope_at(p, 0.0);
        const double kc = kappa + p.shx;
        const double mxm = std::abs(p.dx);
        const double expected = fy0_slope * mxm / std::sqrt((mxm * mxm) + (kc * fy0_slope * kc * fy0_slope));
        expect_bits(ec.dual.ry.value(i), 1.0);
        expect_bits(ec.dual.ry.derivative(i), 0.0);
        expect_bits(ec.dual.fy_n.value(i), ec.plain.fy_n(i));
        EXPECT_TRUE(rel_near_result(ec.dual.fy_n.derivative(i), expected, REL_TOL_CLOSED_FORM));
      }
    });
  }
}

TYPED_TEST(dual_lane_ops, FrictionEllipse_DoubleOrigin_ForceSlopeIsPureSlope) {
  using lane_t = TypeParam;
  const mf::mf5x_coefficients c = mf::test::fixture_s().coefficients;
  const values_t alphas{0.0, 0.0};
  for_each_ellipse_case<lane_t>(
      c, alphas, 0.0, slip_seeds{.alpha = 0.0, .kappa = 1.0}, [&](const ellipse_case<lane_t> &ec) {
        for (Eigen::Index i = 0; i < ec.alpha.size(); ++i) {
          const pure_scalars p = scalars_at(ec.plain_state, i);
          expect_bits(ec.dual.fx_n.value(i), ec.plain.fx_n(i));
          EXPECT_TRUE(rel_near_result(ec.dual.fx_n.derivative(i), fx0_slope_at(p, 0.0), REL_TOL_CLOSED_FORM));
        }
      });
  for_each_ellipse_case<lane_t>(c, alphas, 0.0, slip_seeds{}, [&](const ellipse_case<lane_t> &ec) {
    for (Eigen::Index i = 0; i < ec.alpha.size(); ++i) {
      const pure_scalars p = scalars_at(ec.plain_state, i);
      expect_bits(ec.dual.fy_n.value(i), ec.plain.fy_n(i));
      EXPECT_TRUE(rel_near_result(ec.dual.fy_n.derivative(i), fy0_slope_at(p, 0.0), REL_TOL_CLOSED_FORM));
    }
  });
}

TYPED_TEST(dual_lane_ops, FrictionEllipse_SeedScaledByTwo_ForceSlopeDoubles) {
  using lane_t = TypeParam;
  const values_t kappa_zero_alphas{0.05, -0.08};
  const slip_seeds kappa_unit{.alpha = 0.0, .kappa = 1.0};
  const slip_seeds kappa_twice{.alpha = 0.0, .kappa = 2.0};
  expect_seed_doubles<lane_t>(kappa_zero_alphas, 0.0, kappa_unit, kappa_twice, &force_slope_list::fx);
  const values_t alpha_zero{0.0, 0.0};
  const slip_seeds alpha_unit{.alpha = 1.0, .kappa = 0.0};
  const slip_seeds alpha_twice{.alpha = 2.0, .kappa = 0.0};
  for (const double kappa : {0.1, -0.06}) {
    expect_seed_doubles<lane_t>(alpha_zero, kappa, alpha_unit, alpha_twice, &force_slope_list::fy);
  }
}

TYPED_TEST(dual_lane_ops, FrictionEllipse_NoFirstOrderLimit_UsesDefinedRatio) {
  using lane_t = TypeParam;
  mf::mf5x_coefficients c = mf::test::fixture_s().coefficients;
  c.PDY1 = 0.0;
  // No lateral friction: along the kappa seed at (0.1, 0) the limit has no first-order term and the defined ratio 1
  // keeps the pure slope.
  const slip_seeds kappa_seed{.alpha = 0.0, .kappa = 1.0};
  const values_t alphas_zero{0.0, 0.0};
  const auto expect_pure_slope = [&](const ellipse_case<lane_t> &ec) {
    for (Eigen::Index i = 0; i < ec.alpha.size(); ++i) {
      const pure_scalars p = scalars_at(ec.plain_state, i);
      expect_bits(ec.dual.fx_n.value(i), ec.plain.fx_n(i));
      EXPECT_TRUE(rel_near_result(ec.dual.fx_n.derivative(i), fx0_slope_at(p, 0.1), REL_TOL_CLOSED_FORM));
    }
  };
  for_each_ellipse_case<lane_t>(c, alphas_zero, 0.1, kappa_seed, expect_pure_slope);
  // At (0, 0.05) the limit exists and is 0.
  const values_t alphas_off{0.05, 0.05};
  const auto expect_zero_slope = [&](const ellipse_case<lane_t> &ec) {
    for (Eigen::Index i = 0; i < ec.alpha.size(); ++i) {
      expect_bits(ec.dual.fx_n.derivative(i), 0.0);
    }
  };
  for_each_ellipse_case<lane_t>(c, alphas_off, 0.0, kappa_seed, expect_zero_slope);
}

TYPED_TEST(dual_lane_ops, FrictionEllipse_DefinedRatio_ForceSlopeIsProductRule) {
  using lane_t = TypeParam;
  const mf::mf5x_coefficients c = mf::test::fixture_s().coefficients;
  const std::vector<std::pair<double, double>> points{{0.1, 0.05}, {0.05, 0.1}}; // (kappa, alpha)
  for (const bool seed_alpha : {true, false}) {
    for (const auto &[kappa, alpha_value] : points) {
      check_defined_ratio_slope<lane_t>(c, kappa, alpha_value, seed_alpha);
    }
  }
}

TYPED_TEST(dual_lane_ops, FrictionEllipse_PlainLanes_OriginRatioStaysOne) {
  using lane_t = TypeParam;
  const mf::mf5x_coefficients c = mf::test::fixture_s().coefficients;
  const values_t alphas{0.05, 0.05};
  for_each_chunk<lane_t>(alphas.size(), [&](std::size_t begin, std::size_t count) {
    const auto alpha = lane_slice<lane_t>(alphas, begin, count);
    const auto kappa_zero = plain_inputs(alpha, 0.0);
    const auto at_kappa_origin =
        kernels::friction_ellipse_ratios(pure_state(c, kappa_zero), kappa_zero.kappa, kappa_zero.alpha_rad);
    const lane_t alpha_zero = lane_t::Zero(alpha.size());
    const auto kappa_on = plain_inputs(alpha_zero, 0.1);
    const auto at_alpha_origin =
        kernels::friction_ellipse_ratios(pure_state(c, kappa_on), kappa_on.kappa, kappa_on.alpha_rad);
    for (Eigen::Index i = 0; i < alpha.size(); ++i) {
      expect_bits(at_kappa_origin.rx(i), 1.0);
      expect_bits(at_alpha_origin.ry(i), 1.0);
    }
  });
}

TYPED_TEST(dual_lane_ops, FrictionEllipse_DualPositiveDenominator_SelectsRawRatio) {
  using lane_t = TypeParam;
  const mf::mf5x_coefficients c = mf::test::fixture_s().coefficients;
  const values_t alphas{0.05, 0.1};
  for_each_chunk<lane_t>(alphas.size(), [&](std::size_t begin, std::size_t count) {
    const auto alpha = lane_slice<lane_t>(alphas, begin, count);
    const auto plain_in = plain_inputs(alpha, 0.1);
    const auto dual_in = dual_inputs<lane_t>(alpha, 0.1);
    const auto plain = kernels::friction_ellipse_ratios(pure_state(c, plain_in), plain_in.kappa, plain_in.alpha_rad);
    const auto dual = kernels::friction_ellipse_ratios(pure_state(c, dual_in), dual_in.kappa, dual_in.alpha_rad);
    for (Eigen::Index i = 0; i < alpha.size(); ++i) {
      expect_bits(dual.rx.value(i), plain.rx(i));
      expect_bits(dual.ry.value(i), plain.ry(i));
      expect_finite(dual.rx.derivative(i));
      expect_finite(dual.ry.derivative(i));
      EXPECT_NE(dual.ry.derivative(i), 0.0);
    }
  });
}
