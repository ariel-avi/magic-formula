#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/detail/lanes.hpp>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_forms.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/model_layout.hpp>
#include <vector>

namespace {
  namespace kernels = mf::detail;
  using kernels::block_lane;
  using kernels::point_lane;
  using mf::test::bits;
  using mf::test::lanes_of;
  using mf::test::near_scaled;

  // Scales of the block-vs-point comparison: forces [N] and moments [N*m] (FNOMIN*R0 of fixture S).
  constexpr double FORCE_SCALE_N = 4000.0;
  constexpr double MOMENT_SCALE_NM = 1200.0;
  constexpr double ONE = 1.0;
  constexpr double ZERO = 0.0;
  constexpr double EXTREME = 1e300;

  // The conditioned inputs, contact mask and flags of one row.
  struct condition_row {
    double fz_n = 0.0;
    double kappa = 0.0;
    double alpha_rad = 0.0;
    double gamma_rad = 0.0;
    double vx_mps = 0.0;
    bool contact = false;
    double fz_clamped = 0.0;
    double kappa_clamped = 0.0;
    double alpha_clamped = 0.0;
    double gamma_clamped = 0.0;
    double below_vxlow = 0.0;
  };

  // The forces, moments and flags of one row.
  struct output_row {
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

  mf::test::point pt(double fz, double kappa, double alpha, double gamma, double vx = 10.0) {
    return mf::test::point{.fz_n = fz, .kappa = kappa, .alpha_rad = alpha, .gamma_rad = gamma, .vx_mps = vx};
  }

  // Fixture S-rng ranges on the limits vector of the layout: Fz [200, 10000] N, kappa [-1, 1], alpha [-1.5, 1.5] rad,
  // gamma [-0.2, 0.2] rad, VXLOW 1 m/s.
  Eigen::VectorXd ranged_limits() {
    Eigen::VectorXd limits = mf::default_limits(mf::mf5x::LIMITS);
    limits(mf::mf5x::FZMIN) = 200.0;
    limits(mf::mf5x::FZMAX) = 10000.0;
    limits(mf::mf5x::KPUMIN) = -1.0;
    limits(mf::mf5x::KPUMAX) = 1.0;
    limits(mf::mf5x::ALPMIN) = -1.5;
    limits(mf::mf5x::ALPMAX) = 1.5;
    limits(mf::mf5x::CAMMIN) = -0.2;
    limits(mf::mf5x::CAMMAX) = 0.2;
    return limits;
  }

  double clamp_to(double x, double lower, double upper) {
    if (x < lower) {
      return lower;
    }
    return x > upper ? upper : x;
  }

  kernels::input_lanes<block_lane> block_inputs_of(const std::vector<mf::test::point> &points) {
    const auto rows = static_cast<Eigen::Index>(points.size());
    kernels::input_lanes<block_lane> in;
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

  template <class Lane_T>
  condition_row condition_row_of(const kernels::conditioned_lanes<Lane_T> &cond, Eigen::Index i) {
    condition_row row;
    row.fz_n = cond.point.fz_n(i);
    row.kappa = cond.point.kappa(i);
    row.alpha_rad = cond.point.alpha_rad(i);
    row.gamma_rad = cond.point.gamma_rad(i);
    row.vx_mps = cond.point.vx_mps(i);
    row.contact = cond.contact(i);
    row.fz_clamped = cond.fz_clamped(i);
    row.kappa_clamped = cond.kappa_clamped(i);
    row.alpha_clamped = cond.alpha_clamped(i);
    row.gamma_clamped = cond.gamma_clamped(i);
    row.below_vxlow = cond.below_vxlow(i);
    return row;
  }

  template <class Lane_T> output_row output_row_of(const kernels::output_lanes<Lane_T> &out, Eigen::Index i) {
    output_row row;
    row.fx_n = out.fx_n(i);
    row.fy_n = out.fy_n(i);
    row.mz_nm = out.mz_nm(i);
    row.mx_nm = out.mx_nm(i);
    row.my_nm = out.my_nm(i);
    row.fz_clamped = out.fz_clamped(i);
    row.kappa_clamped = out.kappa_clamped(i);
    row.alpha_clamped = out.alpha_clamped(i);
    row.gamma_clamped = out.gamma_clamped(i);
    row.below_vxlow = out.below_vxlow(i);
    return row;
  }

  std::vector<condition_row> condition_on_points(const Eigen::VectorXd &limits,
                                                 const std::vector<mf::test::point> &points, double load_sign = ONE) {
    std::vector<condition_row> rows;
    for (const mf::test::point &p : points) {
      rows.push_back(condition_row_of(kernels::mf5x_condition<point_lane>(limits, lanes_of(p), load_sign), 0));
    }
    return rows;
  }

  std::vector<condition_row> condition_on_block(const Eigen::VectorXd &limits,
                                                const std::vector<mf::test::point> &points, double load_sign = ONE) {
    const kernels::conditioned_lanes<block_lane> cond =
        kernels::mf5x_condition<block_lane>(limits, block_inputs_of(points), load_sign);
    std::vector<condition_row> rows;
    for (Eigen::Index i = 0; i < static_cast<Eigen::Index>(points.size()); ++i) {
      rows.push_back(condition_row_of(cond, i));
    }
    return rows;
  }

  std::vector<output_row> pipeline_on_points(const mf::mf5x_coefficients &c, mf::slip_mode mode,
                                             const Eigen::VectorXd &limits,
                                             const std::vector<mf::test::point> &points) {
    std::vector<output_row> rows;
    for (const mf::test::point &p : points) {
      rows.push_back(output_row_of(kernels::mf5x_pipeline(c, mf::mf5x_forms{}, mode, limits, lanes_of(p)), 0));
    }
    return rows;
  }

  std::vector<output_row> pipeline_on_block(const mf::mf5x_coefficients &c, mf::slip_mode mode,
                                            const Eigen::VectorXd &limits, const std::vector<mf::test::point> &points) {
    const kernels::output_lanes<block_lane> out =
        kernels::mf5x_pipeline(c, mf::mf5x_forms{}, mode, limits, block_inputs_of(points));
    std::vector<output_row> rows;
    for (Eigen::Index i = 0; i < static_cast<Eigen::Index>(points.size()); ++i) {
      rows.push_back(output_row_of(out, i));
    }
    return rows;
  }

  // The conditioning of `points` on point lanes (index 0) and on one block lane (index 1).
  std::array<std::vector<condition_row>, 2>
  condition_on_both(const Eigen::VectorXd &limits, const std::vector<mf::test::point> &points, double load_sign = ONE) {
    return {condition_on_points(limits, points, load_sign), condition_on_block(limits, points, load_sign)};
  }

  // The pipeline of `points` on point lanes (index 0) and on one block lane (index 1).
  std::array<std::vector<output_row>, 2> pipeline_on_both(const mf::mf5x_coefficients &c, mf::slip_mode mode,
                                                          const Eigen::VectorXd &limits,
                                                          const std::vector<mf::test::point> &points) {
    return {pipeline_on_points(c, mode, limits, points), pipeline_on_block(c, mode, limits, points)};
  }

  const char *lane_name(std::size_t index) {
    return index == 0 ? "point lanes" : "block lane";
  }

  void expect_same_bits(double actual, double expected) {
    EXPECT_EQ(bits(actual), bits(expected));
  }

  // The conditioned values and the four clamp flags of a row.
  void expect_condition(const condition_row &row, double fz, double kappa, double alpha, double gamma, double fz_flag,
                        double kappa_flag, double alpha_flag, double gamma_flag) {
    expect_same_bits(row.fz_n, fz);
    expect_same_bits(row.kappa, kappa);
    expect_same_bits(row.alpha_rad, alpha);
    expect_same_bits(row.gamma_rad, gamma);
    expect_same_bits(row.fz_clamped, fz_flag);
    expect_same_bits(row.kappa_clamped, kappa_flag);
    expect_same_bits(row.alpha_clamped, alpha_flag);
    expect_same_bits(row.gamma_clamped, gamma_flag);
  }

  void expect_zero_forces(const output_row &row) {
    expect_same_bits(row.fx_n, ZERO);
    expect_same_bits(row.fy_n, ZERO);
    expect_same_bits(row.mz_nm, ZERO);
    expect_same_bits(row.mx_nm, ZERO);
    expect_same_bits(row.my_nm, ZERO);
  }

  void expect_clamp_flags(const output_row &row, double fz_flag, double kappa_flag, double alpha_flag,
                          double gamma_flag) {
    expect_same_bits(row.fz_clamped, fz_flag);
    expect_same_bits(row.kappa_clamped, kappa_flag);
    expect_same_bits(row.alpha_clamped, alpha_flag);
    expect_same_bits(row.gamma_clamped, gamma_flag);
  }

  void expect_forces_near(const output_row &actual, const output_row &expected) {
    constexpr double tol = mf::test::BATCH_REL_TOL;
    EXPECT_TRUE(near_scaled(actual.fx_n, expected.fx_n, tol, FORCE_SCALE_N));
    EXPECT_TRUE(near_scaled(actual.fy_n, expected.fy_n, tol, FORCE_SCALE_N));
    EXPECT_TRUE(near_scaled(actual.mz_nm, expected.mz_nm, tol, MOMENT_SCALE_NM));
    EXPECT_TRUE(near_scaled(actual.mx_nm, expected.mx_nm, tol, MOMENT_SCALE_NM));
    EXPECT_TRUE(near_scaled(actual.my_nm, expected.my_nm, tol, MOMENT_SCALE_NM));
  }

  // The evaluation of a clamped point on point lanes.
  output_row evaluate_point(const mf::mf5x_coefficients &c, mf::slip_mode mode, const mf::test::point &p) {
    const kernels::force_lanes<point_lane> f = kernels::mf5x_evaluate(c, mf::mf5x_forms{}, mode, lanes_of(p));
    output_row row;
    row.fx_n = f.fx_n(0);
    row.fy_n = f.fy_n(0);
    row.mz_nm = f.mz_nm(0);
    row.mx_nm = f.mx_nm(0);
    row.my_nm = f.my_nm(0);
    return row;
  }

  mf::mf5x_coefficients coefficients() {
    return mf::test::fixture_s().coefficients;
  }
} // namespace

// ---- mf5x_condition ---------------------------------------------------------------------------------------------

TEST(mf5x_pipeline, Condition_EachVariableBelowLower_ClampedAndFlagged) {
  std::vector<mf::test::point> points;
  points.push_back(pt(100.0, 0.1, 0.05, 0.02));
  points.push_back(pt(4000.0, -2.0, 0.05, 0.02));
  points.push_back(pt(4000.0, 0.1, -2.0, 0.02));
  points.push_back(pt(4000.0, 0.1, 0.05, -0.5));
  const auto runs = condition_on_both(ranged_limits(), points);
  for (std::size_t k = 0; k < runs.size(); ++k) {
    SCOPED_TRACE(lane_name(k));
    const std::vector<condition_row> &rows = runs[k];
    ASSERT_EQ(rows.size(), 4U);
    expect_condition(rows[0], 200.0, 0.1, 0.05, 0.02, ONE, ZERO, ZERO, ZERO);
    expect_condition(rows[1], 4000.0, -1.0, 0.05, 0.02, ZERO, ONE, ZERO, ZERO);
    expect_condition(rows[2], 4000.0, 0.1, -1.5, 0.02, ZERO, ZERO, ONE, ZERO);
    expect_condition(rows[3], 4000.0, 0.1, 0.05, -0.2, ZERO, ZERO, ZERO, ONE);
    for (const condition_row &row : rows) {
      EXPECT_TRUE(row.contact);
    }
  }
}

TEST(mf5x_pipeline, Condition_EachVariableAboveUpper_ClampedAndFlagged) {
  std::vector<mf::test::point> points;
  points.push_back(pt(20000.0, 0.1, 0.05, 0.02));
  points.push_back(pt(4000.0, 2.0, 0.05, 0.02));
  points.push_back(pt(4000.0, 0.1, 2.0, 0.02));
  points.push_back(pt(4000.0, 0.1, 0.05, 0.5));
  const auto runs = condition_on_both(ranged_limits(), points);
  for (std::size_t k = 0; k < runs.size(); ++k) {
    SCOPED_TRACE(lane_name(k));
    const std::vector<condition_row> &rows = runs[k];
    ASSERT_EQ(rows.size(), 4U);
    expect_condition(rows[0], 10000.0, 0.1, 0.05, 0.02, ONE, ZERO, ZERO, ZERO);
    expect_condition(rows[1], 4000.0, 1.0, 0.05, 0.02, ZERO, ONE, ZERO, ZERO);
    expect_condition(rows[2], 4000.0, 0.1, 1.5, 0.02, ZERO, ZERO, ONE, ZERO);
    expect_condition(rows[3], 4000.0, 0.1, 0.05, 0.2, ZERO, ZERO, ZERO, ONE);
    for (const condition_row &row : rows) {
      EXPECT_TRUE(row.contact);
    }
  }
}

TEST(mf5x_pipeline, Condition_EachVariableAtLimit_NotFlagged) {
  std::vector<mf::test::point> points;
  points.push_back(pt(200.0, 0.1, 0.05, 0.02));
  points.push_back(pt(10000.0, 0.1, 0.05, 0.02));
  points.push_back(pt(4000.0, -1.0, 0.05, 0.02));
  points.push_back(pt(4000.0, 1.0, 0.05, 0.02));
  points.push_back(pt(4000.0, 0.1, -1.5, 0.02));
  points.push_back(pt(4000.0, 0.1, 1.5, 0.02));
  points.push_back(pt(4000.0, 0.1, 0.05, -0.2));
  points.push_back(pt(4000.0, 0.1, 0.05, 0.2));
  const auto runs = condition_on_both(ranged_limits(), points);
  for (std::size_t k = 0; k < runs.size(); ++k) {
    SCOPED_TRACE(lane_name(k));
    const std::vector<condition_row> &rows = runs[k];
    ASSERT_EQ(rows.size(), points.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
      SCOPED_TRACE(i);
      expect_condition(rows[i], points[i].fz_n, points[i].kappa, points[i].alpha_rad, points[i].gamma_rad, ZERO, ZERO,
                       ZERO, ZERO);
      EXPECT_TRUE(rows[i].contact);
    }
  }
}

TEST(mf5x_pipeline, Condition_InsideRange_Unchanged) {
  std::vector<mf::test::point> points;
  points.push_back(pt(4000.0, 0.1, 0.05, 0.02));
  points.push_back(pt(3000.0, -0.3, -0.2, -0.05));
  points.push_back(pt(5000.0, 0.0, 0.0, 0.0));
  const auto runs = condition_on_both(ranged_limits(), points);
  for (std::size_t k = 0; k < runs.size(); ++k) {
    SCOPED_TRACE(lane_name(k));
    const std::vector<condition_row> &rows = runs[k];
    ASSERT_EQ(rows.size(), points.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
      SCOPED_TRACE(i);
      expect_condition(rows[i], points[i].fz_n, points[i].kappa, points[i].alpha_rad, points[i].gamma_rad, ZERO, ZERO,
                       ZERO, ZERO);
      EXPECT_TRUE(rows[i].contact);
    }
  }
}

TEST(mf5x_pipeline, Condition_AbsentLimits_NeverClamped) {
  const Eigen::VectorXd absent = mf::default_limits(mf::mf5x::LIMITS);
  std::vector<mf::test::point> points;
  points.push_back(pt(EXTREME, EXTREME, EXTREME, EXTREME));
  points.push_back(pt(1e-300, -EXTREME, -EXTREME, -EXTREME));
  points.push_back(pt(EXTREME, 1e3, 100.0, 50.0));
  const auto runs = condition_on_both(absent, points);
  for (std::size_t k = 0; k < runs.size(); ++k) {
    SCOPED_TRACE(lane_name(k));
    const std::vector<condition_row> &rows = runs[k];
    ASSERT_EQ(rows.size(), points.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
      SCOPED_TRACE(i);
      expect_condition(rows[i], points[i].fz_n, points[i].kappa, points[i].alpha_rad, points[i].gamma_rad, ZERO, ZERO,
                       ZERO, ZERO);
      EXPECT_TRUE(rows[i].contact);
    }
  }
}

TEST(mf5x_pipeline, Condition_VxNeverModified) {
  const std::vector<double> speeds{0.0, -0.0, 0.5, 1.0, 16.6, 1e6, -3.0};
  std::vector<mf::test::point> points;
  for (const double vx : speeds) {
    points.push_back(pt(4000.0, 3.0, 3.0, 3.0, vx));
  }
  const auto runs = condition_on_both(ranged_limits(), points);
  for (std::size_t k = 0; k < runs.size(); ++k) {
    SCOPED_TRACE(lane_name(k));
    const std::vector<condition_row> &rows = runs[k];
    ASSERT_EQ(rows.size(), speeds.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
      SCOPED_TRACE(i);
      expect_same_bits(rows[i].vx_mps, speeds[i]);
    }
  }
}

TEST(mf5x_pipeline, Condition_NegativeLoadSign_NegativeFzInContact) {
  std::vector<mf::test::point> points;
  points.push_back(pt(-4000.0, 0.1, 0.05, 0.02));
  points.push_back(pt(-1e-300, 0.1, 0.05, 0.02));
  points.push_back(pt(-EXTREME, 0.1, 0.05, 0.02));
  const auto runs = condition_on_both(mf::default_limits(mf::mf5x::LIMITS), points, -ONE);
  for (std::size_t k = 0; k < runs.size(); ++k) {
    SCOPED_TRACE(lane_name(k));
    ASSERT_EQ(runs[k].size(), points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
      SCOPED_TRACE(i);
      expect_condition(runs[k][i], points[i].fz_n, 0.1, 0.05, 0.02, ZERO, ZERO, ZERO, ZERO);
      EXPECT_TRUE(runs[k][i].contact);
    }
  }
}

TEST(mf5x_pipeline, Condition_NegativeLoadSign_PositiveZeroNegZeroNanFzNoContactNoFlags) {
  Eigen::VectorXd limits = mf::default_limits(mf::mf5x::LIMITS);
  limits(mf::mf5x::FZMIN) = -6000.0;
  limits(mf::mf5x::FZMAX) = -1000.0;
  const double quiet_nan = std::numeric_limits<double>::quiet_NaN();
  std::vector<mf::test::point> points;
  points.push_back(pt(500.0, 0.1, 0.05, 0.02));
  points.push_back(pt(0.0, 0.1, 0.05, 0.02));
  points.push_back(pt(-0.0, 0.1, 0.05, 0.02));
  points.push_back(pt(quiet_nan, 0.1, 0.05, 0.02));
  points.push_back(pt(8000.0, 0.1, 0.05, 0.02));
  const auto runs = condition_on_both(limits, points, -ONE);
  for (std::size_t k = 0; k < runs.size(); ++k) {
    SCOPED_TRACE(lane_name(k));
    ASSERT_EQ(runs[k].size(), points.size());
    for (const condition_row &row : runs[k]) {
      EXPECT_FALSE(row.contact);
      expect_same_bits(row.fz_clamped, ZERO);
      expect_same_bits(row.kappa_clamped, ZERO);
      expect_same_bits(row.alpha_clamped, ZERO);
      expect_same_bits(row.gamma_clamped, ZERO);
    }
  }
}

TEST(mf5x_pipeline, Condition_NegativeLoadSign_NegativeRangeClampsAndFlags) {
  Eigen::VectorXd limits = mf::default_limits(mf::mf5x::LIMITS);
  limits(mf::mf5x::FZMIN) = -6000.0;
  limits(mf::mf5x::FZMAX) = -1000.0;
  std::vector<mf::test::point> points;
  points.push_back(pt(-8000.0, 0.1, 0.05, 0.02));
  points.push_back(pt(-500.0, 0.1, 0.05, 0.02));
  points.push_back(pt(500.0, 0.1, 0.05, 0.02));
  points.push_back(pt(-3000.0, 0.1, 0.05, 0.02));
  const auto runs = condition_on_both(limits, points, -ONE);
  for (std::size_t k = 0; k < runs.size(); ++k) {
    SCOPED_TRACE(lane_name(k));
    ASSERT_EQ(runs[k].size(), points.size());
    expect_condition(runs[k][0], -6000.0, 0.1, 0.05, 0.02, ONE, ZERO, ZERO, ZERO);
    EXPECT_TRUE(runs[k][0].contact);
    expect_condition(runs[k][1], -1000.0, 0.1, 0.05, 0.02, ONE, ZERO, ZERO, ZERO);
    EXPECT_TRUE(runs[k][1].contact);
    EXPECT_FALSE(runs[k][2].contact);
    expect_same_bits(runs[k][2].fz_clamped, ZERO);
    expect_condition(runs[k][3], -3000.0, 0.1, 0.05, 0.02, ZERO, ZERO, ZERO, ZERO);
    EXPECT_TRUE(runs[k][3].contact);
  }
}

// ---- mf5x_pipeline ----------------------------------------------------------------------------------------------

TEST(mf5x_pipeline, Pipeline_NegativeFnomin_PositiveFzGivesExactZeros) {
  mf::mf5x_coefficients c = coefficients();
  c.FNOMIN = -4000.0;
  const std::vector<mf::test::point> points{pt(4000.0, 0.1, 0.05, 0.02), pt(100.0, 0.1, 0.05, 0.02),
                                            pt(0.0, 0.1, 0.05, 0.02)};
  for (const mf::slip_mode mode : {mf::slip_mode::COMBINED, mf::slip_mode::UNCOMBINED}) {
    const auto runs = pipeline_on_both(c, mode, ranged_limits(), points);
    for (std::size_t k = 0; k < runs.size(); ++k) {
      SCOPED_TRACE(lane_name(k));
      ASSERT_EQ(runs[k].size(), points.size());
      for (const output_row &row : runs[k]) {
        expect_zero_forces(row);
        expect_clamp_flags(row, ZERO, ZERO, ZERO, ZERO);
      }
    }
  }
}

TEST(mf5x_pipeline, Pipeline_RawFzZero_ExactZeroForcesNoClampFlags) {
  const std::vector<mf::test::point> points{pt(0.0, 5.0, 3.0, 1.0), pt(0.0, 0.1, 0.05, 0.02)};
  for (const mf::slip_mode mode : {mf::slip_mode::COMBINED, mf::slip_mode::UNCOMBINED}) {
    const auto runs = pipeline_on_both(coefficients(), mode, ranged_limits(), points);
    for (std::size_t k = 0; k < runs.size(); ++k) {
      SCOPED_TRACE(lane_name(k));
      ASSERT_EQ(runs[k].size(), points.size());
      for (const output_row &row : runs[k]) {
        expect_zero_forces(row);
        expect_clamp_flags(row, ZERO, ZERO, ZERO, ZERO);
      }
    }
  }
}

TEST(mf5x_pipeline, Pipeline_RawFzNegativeZero_ExactZeroForcesNoClampFlags) {
  const std::vector<mf::test::point> points{pt(-0.0, 5.0, 3.0, 1.0), pt(-0.0, 0.1, 0.05, 0.02)};
  for (const mf::slip_mode mode : {mf::slip_mode::COMBINED, mf::slip_mode::UNCOMBINED}) {
    const auto runs = pipeline_on_both(coefficients(), mode, ranged_limits(), points);
    for (std::size_t k = 0; k < runs.size(); ++k) {
      SCOPED_TRACE(lane_name(k));
      ASSERT_EQ(runs[k].size(), points.size());
      for (const output_row &row : runs[k]) {
        expect_zero_forces(row);
        expect_clamp_flags(row, ZERO, ZERO, ZERO, ZERO);
      }
    }
  }
}

TEST(mf5x_pipeline, Pipeline_RawFzNegative_ClampFlagsMasked) {
  std::vector<mf::test::point> points;
  points.push_back(pt(-500.0, 5.0, 3.0, 1.0));
  points.push_back(pt(-EXTREME, -5.0, -3.0, -1.0));
  points.push_back(pt(-1e-300, 0.1, 0.05, 0.02));
  for (const mf::slip_mode mode : {mf::slip_mode::COMBINED, mf::slip_mode::UNCOMBINED}) {
    const auto runs = pipeline_on_both(coefficients(), mode, ranged_limits(), points);
    for (std::size_t k = 0; k < runs.size(); ++k) {
      SCOPED_TRACE(lane_name(k));
      ASSERT_EQ(runs[k].size(), points.size());
      for (const output_row &row : runs[k]) {
        expect_zero_forces(row);
        expect_clamp_flags(row, ZERO, ZERO, ZERO, ZERO);
      }
    }
  }
}

TEST(mf5x_pipeline, Pipeline_ClampedFzNonPositive_ZeroForcesFzFlagSet) {
  Eigen::VectorXd limits = ranged_limits();
  limits(mf::mf5x::FZMIN) = -100.0;
  limits(mf::mf5x::FZMAX) = 0.0;
  const std::vector<mf::test::point> points{pt(50.0, 0.1, 0.05, 0.02), pt(5000.0, 0.1, 0.05, 0.02)};
  for (const mf::slip_mode mode : {mf::slip_mode::COMBINED, mf::slip_mode::UNCOMBINED}) {
    const auto runs = pipeline_on_both(coefficients(), mode, limits, points);
    for (std::size_t k = 0; k < runs.size(); ++k) {
      SCOPED_TRACE(lane_name(k));
      ASSERT_EQ(runs[k].size(), points.size());
      for (const output_row &row : runs[k]) {
        expect_zero_forces(row);
        expect_clamp_flags(row, ONE, ZERO, ZERO, ZERO);
      }
    }
  }
}

TEST(mf5x_pipeline, Pipeline_BelowVxlow_FlagOne) {
  // The flag is independent of contact: the raw Fz = 0 row is flagged too.
  std::vector<mf::test::point> points;
  points.push_back(pt(4000.0, 0.1, 0.05, 0.02, 0.5));
  points.push_back(pt(4000.0, 0.1, 0.05, 0.02, 0.0));
  points.push_back(pt(4000.0, 0.1, 0.05, 0.02, -1.0));
  points.push_back(pt(4000.0, 0.1, 0.05, 0.02, 0.999999));
  points.push_back(pt(0.0, 0.1, 0.05, 0.02, 0.5));
  const auto runs = pipeline_on_both(coefficients(), mf::slip_mode::COMBINED, ranged_limits(), points);
  for (std::size_t k = 0; k < runs.size(); ++k) {
    SCOPED_TRACE(lane_name(k));
    ASSERT_EQ(runs[k].size(), points.size());
    for (const output_row &row : runs[k]) {
      expect_same_bits(row.below_vxlow, ONE);
    }
  }
}

TEST(mf5x_pipeline, Pipeline_AtVxlow_FlagZero) {
  std::vector<mf::test::point> points;
  points.push_back(pt(4000.0, 0.1, 0.05, 0.02, 1.0));
  points.push_back(pt(4000.0, 0.1, 0.05, 0.02, 1.000001));
  points.push_back(pt(4000.0, 0.1, 0.05, 0.02, 10.0));
  points.push_back(pt(0.0, 0.1, 0.05, 0.02, 1.0));
  const auto runs = pipeline_on_both(coefficients(), mf::slip_mode::COMBINED, ranged_limits(), points);
  for (std::size_t k = 0; k < runs.size(); ++k) {
    SCOPED_TRACE(lane_name(k));
    ASSERT_EQ(runs[k].size(), points.size());
    for (const output_row &row : runs[k]) {
      expect_same_bits(row.below_vxlow, ZERO);
    }
  }
}

TEST(mf5x_pipeline, Pipeline_Contact_EqualsEvaluateOnClampedPoint) {
  const mf::mf5x_coefficients c = coefficients();
  const Eigen::VectorXd limits = ranged_limits();
  std::vector<mf::test::point> points;
  points.push_back(pt(4000.0, 0.1, 0.05, 0.02));
  points.push_back(pt(100.0, 0.1, 0.05, 0.02));
  points.push_back(pt(20000.0, 0.1, 0.05, 0.02));
  points.push_back(pt(4000.0, 2.0, 0.05, 0.02));
  points.push_back(pt(4000.0, -2.0, 0.05, 0.02));
  points.push_back(pt(4000.0, 0.1, 2.0, 0.02));
  points.push_back(pt(4000.0, 0.1, -2.0, 0.02));
  points.push_back(pt(4000.0, 0.1, 0.05, 0.5));
  points.push_back(pt(4000.0, 0.1, 0.05, -0.5));
  points.push_back(pt(50.0, 3.0, -3.0, 0.5, 5.0));
  for (const mf::slip_mode mode : {mf::slip_mode::COMBINED, mf::slip_mode::UNCOMBINED}) {
    const auto runs = pipeline_on_both(c, mode, limits, points);
    for (std::size_t k = 0; k < runs.size(); ++k) {
      SCOPED_TRACE(lane_name(k));
      ASSERT_EQ(runs[k].size(), points.size());
      for (std::size_t i = 0; i < points.size(); ++i) {
        SCOPED_TRACE(i);
        const mf::test::point &raw = points[i];
        const double fz = clamp_to(raw.fz_n, limits(mf::mf5x::FZMIN), limits(mf::mf5x::FZMAX));
        const double kappa = clamp_to(raw.kappa, limits(mf::mf5x::KPUMIN), limits(mf::mf5x::KPUMAX));
        const double alpha = clamp_to(raw.alpha_rad, limits(mf::mf5x::ALPMIN), limits(mf::mf5x::ALPMAX));
        const double gamma = clamp_to(raw.gamma_rad, limits(mf::mf5x::CAMMIN), limits(mf::mf5x::CAMMAX));
        const mf::test::point clamped = pt(fz, kappa, alpha, gamma, raw.vx_mps);
        const output_row expected = evaluate_point(c, mode, clamped);
        if (k == 0) {
          expect_same_bits(runs[k][i].fx_n, expected.fx_n);
          expect_same_bits(runs[k][i].fy_n, expected.fy_n);
          expect_same_bits(runs[k][i].mz_nm, expected.mz_nm);
          expect_same_bits(runs[k][i].mx_nm, expected.mx_nm);
          expect_same_bits(runs[k][i].my_nm, expected.my_nm);
        } else {
          expect_forces_near(runs[k][i], expected);
        }
      }
    }
  }
}

TEST(mf5x_pipeline, Pipeline_BlockVsPoint_ZerosAndFlagsBitEqualForcesNear) {
  const mf::mf5x_coefficients c = coefficients();
  const Eigen::VectorXd limits = ranged_limits();
  const Eigen::MatrixXd grid = mf::test::regime_grid(kernels::BLOCK_ROWS);
  std::vector<mf::test::point> points;
  for (Eigen::Index r = 0; r < grid.rows(); ++r) {
    points.push_back(pt(grid(r, mf::mf5x::FZ), grid(r, mf::mf5x::SR), grid(r, mf::mf5x::SA), grid(r, mf::mf5x::IA),
                        grid(r, mf::mf5x::VX)));
  }
  const std::vector<condition_row> conditioned = condition_on_points(limits, points);
  ASSERT_EQ(conditioned.size(), points.size());
  bool saw_contact = false;
  bool saw_no_contact = false;
  for (const mf::slip_mode mode : {mf::slip_mode::COMBINED, mf::slip_mode::UNCOMBINED}) {
    const std::vector<output_row> single = pipeline_on_points(c, mode, limits, points);
    const std::vector<output_row> block = pipeline_on_block(c, mode, limits, points);
    ASSERT_EQ(single.size(), points.size());
    ASSERT_EQ(block.size(), points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
      SCOPED_TRACE(i);
      expect_clamp_flags(block[i], single[i].fz_clamped, single[i].kappa_clamped, single[i].alpha_clamped,
                         single[i].gamma_clamped);
      expect_same_bits(block[i].below_vxlow, single[i].below_vxlow);
      if (conditioned[i].contact) {
        saw_contact = true;
        expect_forces_near(block[i], single[i]);
      } else {
        saw_no_contact = true;
        expect_zero_forces(single[i]);
        expect_zero_forces(block[i]);
      }
    }
  }
  // The grid must hold both kinds of rows, otherwise the comparison would not cover the zero rule.
  EXPECT_TRUE(saw_contact);
  EXPECT_TRUE(saw_no_contact);
}
