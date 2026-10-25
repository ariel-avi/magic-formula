#pragma once

#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <gtest/gtest.h>
#include <iterator>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>

/// Shared test helpers (header-only). Reference values come from docs/research/test-data.md.
namespace mf::test {
  /// Relative tolerance for closed-form identities.
  inline constexpr double REL_TOL_CLOSED_FORM = 1e-12;
  /// Relative tolerance for MF 5.2 vectors, where eps perturbs at the 1e-7 level.
  inline constexpr double REL_TOL_MF52 = 1e-6;
  /// Relative tolerance for continuity checks.
  inline constexpr double REL_TOL_CONTINUITY = 1e-9;

  /// Half a unit of the last printed digit: 0.5*10^-decimals.
  [[nodiscard]] inline double printed_tol(int decimals) {
    return 0.5 * std::pow(10.0, -decimals);
  }

  /// |actual - expected| <= rel_tol*|expected|, with an absolute bound of 1e-300 when expected is 0. False for NaN.
  [[nodiscard]] inline bool rel_near(double actual, double expected, double rel_tol) {
    const double scale = std::abs(expected);
    const double bound = scale > 0.0 ? rel_tol * scale : 1e-300;
    return std::abs(actual - expected) <= bound;
  }

  /// `rel_near` as a gtest assertion result with a diagnostic: `EXPECT_TRUE(rel_near_result(a, e, tol))`.
  [[nodiscard]] inline ::testing::AssertionResult rel_near_result(double actual, double expected, double rel_tol) {
    if (rel_near(actual, expected, rel_tol)) {
      return ::testing::AssertionSuccess();
    }
    const std::string message = std::format("actual {:.17g}, expected {:.17g}, tol {:g}", actual, expected, rel_tol);
    return ::testing::AssertionFailure() << message;
  }

  /// The IEEE-754 bit pattern of `value`, for the documented bit-equal exception (`EXPECT_EQ(bits(a), bits(b))`).
  [[nodiscard]] inline std::uint64_t bits(double value) {
    return std::bit_cast<std::uint64_t>(value);
  }

  /// Path of a file in tests/data (MF_TEST_DATA_DIR is a usage requirement of the CMake target `mf::test_support`).
  [[nodiscard]] inline std::filesystem::path data_file(std::string_view name) {
    return std::filesystem::path{MF_TEST_DATA_DIR} / std::filesystem::path{name};
  }

  /// One operating point in SI units.
  struct point {
    double fz_n = 0.0;
    double kappa = 0.0;
    double alpha_rad = 0.0;
    double gamma_rad = 0.0;
    double vx_mps = 0.0;
  };

  /// A one-point lane holding `value`.
  [[nodiscard]] inline mf::detail::point_lane lane(double value) {
    return mf::detail::point_lane::Constant(value);
  }

  /// The point lanes of any point-like value (a type with the members of `mf::test::point`).
  template <typename Point_T> [[nodiscard]] mf::detail::input_lanes<mf::detail::point_lane> lanes_of(const Point_T &p) {
    return mf::detail::input_lanes<mf::detail::point_lane>{.fz_n = lane(p.fz_n),
                                                           .alpha_rad = lane(p.alpha_rad),
                                                           .kappa = lane(p.kappa),
                                                           .gamma_rad = lane(p.gamma_rad),
                                                           .vx_mps = lane(p.vx_mps)};
  }

  /// Pure-slip state of one point on point lanes.
  template <typename Point_T>
  [[nodiscard]] mf::detail::slip_state<mf::detail::point_lane> slip_state_at(const mf::mf5x_coefficients &c,
                                                                             const Point_T &p) {
    return mf::detail::evaluate_slip_state(c, lanes_of(p));
  }

  /// Fixture S: synthetic PAC2002 set, all L* = 1, no ranges, VXLOW 1, USE_MODE 4, LEFT, no
  /// FITTYP. Every other coefficient is 0.
  [[nodiscard]] inline mf::mf5x_parameters fixture_s() {
    mf::mf5x_parameters parameters;
    mf::mf5x_coefficients &c = parameters.coefficients;
    c.FNOMIN = 4000.0;
    c.UNLOADED_RADIUS = 0.3;
    c.LONGVL = 16.6;
    c.PCX1 = 1.5;
    c.PDX1 = 1.0;
    c.PKX1 = 20.0;
    c.RBX1 = 10.0;
    c.RCX1 = 1.0;
    c.PCY1 = 1.3;
    c.PDY1 = 1.0;
    c.PKY1 = -15.0;
    c.PKY2 = 1.5;
    c.RBY1 = 10.0;
    c.RCY1 = 1.0;
    c.QSY1 = 0.01;
    c.QBZ1 = 10.0;
    c.QCZ1 = 1.2;
    c.QDZ1 = 0.1;
    parameters.limits(mf::mf5x::VXLOW) = 1.0;
    parameters.metadata = mf::model_metadata{.fittyp = std::nullopt, .use_mode = 4, .side = mf::tyre_side::LEFT};
    return parameters;
  }

  /// Fixture S52: fixture S with FITTYP 6 (MF 5.2 forces LGA* = 1 in `create`).
  [[nodiscard]] inline mf::mf5x_parameters fixture_s52() {
    mf::mf5x_parameters parameters = fixture_s();
    parameters.metadata.fittyp = 6;
    return parameters;
  }

  /// Fixture S-rng: fixture S with synthetic ranges: Fz [200, 10000] N, kappa [-1, 1], alpha [-1.5, 1.5] rad,
  /// gamma [-0.2, 0.2] rad.
  [[nodiscard]] inline mf::mf5x_parameters fixture_s_rng() {
    mf::mf5x_parameters parameters = fixture_s();
    Eigen::VectorXd &limits = parameters.limits;
    limits(mf::mf5x::FZMIN) = 200.0;
    limits(mf::mf5x::FZMAX) = 10000.0;
    limits(mf::mf5x::KPUMIN) = -1.0;
    limits(mf::mf5x::KPUMAX) = 1.0;
    limits(mf::mf5x::ALPMIN) = -1.5;
    limits(mf::mf5x::ALPMAX) = 1.5;
    limits(mf::mf5x::CAMMIN) = -0.2;
    limits(mf::mf5x::CAMMAX) = 0.2;
    return parameters;
  }

  /// Relative tolerance of batch results against per-point results (scaled, see `near_scaled`): the vectorised libm
  /// differs from the scalar one by a few ULP.
  inline constexpr double BATCH_REL_TOL = 1e-12;

  /// The input vector of `p` in `mf5x::input_index` order (FZ, SA, SR, IA, VX).
  [[nodiscard]] inline Eigen::VectorXd input_of(const point &p) {
    Eigen::VectorXd input(mf::mf5x::INPUT_LENGTH);
    input(mf::mf5x::FZ) = p.fz_n;
    input(mf::mf5x::SA) = p.alpha_rad;
    input(mf::mf5x::SR) = p.kappa;
    input(mf::mf5x::IA) = p.gamma_rad;
    input(mf::mf5x::VX) = p.vx_mps;
    return input;
  }

  /// The input matrix of `points`: one row per point, one column per `mf5x::input_index`.
  [[nodiscard]] inline Eigen::MatrixXd inputs_of(std::span<const point> points) {
    Eigen::MatrixXd inputs(static_cast<Eigen::Index>(points.size()), mf::mf5x::INPUT_LENGTH);
    for (std::size_t i = 0; i < points.size(); ++i) {
      inputs.row(static_cast<Eigen::Index>(i)) = input_of(points[i]).transpose();
    }
    return inputs;
  }

  /// Success iff |actual - expected| <= tol*(scale + max(|actual|, |expected|)). A NaN on either side fails. The
  /// failure message prints both values with 17 significant digits.
  [[nodiscard]] inline ::testing::AssertionResult near_scaled(double actual, double expected, double tol,
                                                              double scale) {
    const double bound = tol * (scale + std::max(std::abs(actual), std::abs(expected)));
    if (std::abs(actual - expected) <= bound) {
      return ::testing::AssertionSuccess();
    }
    const std::string message =
        std::format("actual {:.17g}, expected {:.17g}, tol {:g}, scale {:g}", actual, expected, tol, scale);
    return ::testing::AssertionFailure() << message;
  }

  /// The operating regimes of `regime_grid`: nominal, pure and combined slip of both signs, Fz = 0, -0.0, < 0 and
  /// 1e-300, beyond each fixture-S-rng range, alpha near +-pi/2, kappa = -1, Vx = 0 and below VXLOW, and alpha_y
  /// around 0.
  inline constexpr std::array<point, 24> REGIME_POINTS{{
      {.fz_n = 4000.0, .kappa = 0.0, .alpha_rad = 0.0, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = 4000.0, .kappa = 0.0, .alpha_rad = 0.1, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = 4000.0, .kappa = 0.0, .alpha_rad = -0.1, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = 4000.0, .kappa = 0.1, .alpha_rad = 0.0, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = 4000.0, .kappa = -0.1, .alpha_rad = 0.0, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = 4000.0, .kappa = 0.1, .alpha_rad = 0.1, .gamma_rad = 0.05, .vx_mps = 16.6},
      {.fz_n = 3000.0, .kappa = 0.05, .alpha_rad = -0.2, .gamma_rad = -0.05, .vx_mps = 10.0},
      {.fz_n = 5000.0, .kappa = -0.3, .alpha_rad = 0.2, .gamma_rad = 0.1, .vx_mps = 30.0},
      {.fz_n = 0.0, .kappa = 0.1, .alpha_rad = 0.1, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = -0.0, .kappa = 0.1, .alpha_rad = 0.1, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = -500.0, .kappa = 0.1, .alpha_rad = 0.1, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = 1e-300, .kappa = 0.1, .alpha_rad = 0.1, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = 20000.0, .kappa = 0.1, .alpha_rad = 0.1, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = 100.0, .kappa = 0.1, .alpha_rad = 0.1, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = 4000.0, .kappa = 0.0, .alpha_rad = 1.5707, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = 4000.0, .kappa = 0.0, .alpha_rad = -1.5707, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = 4000.0, .kappa = -1.0, .alpha_rad = 0.0, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = 4000.0, .kappa = 1.5, .alpha_rad = 0.0, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = 4000.0, .kappa = 0.0, .alpha_rad = 0.05, .gamma_rad = 0.3, .vx_mps = 16.6},
      {.fz_n = 4000.0, .kappa = 0.05, .alpha_rad = 0.05, .gamma_rad = -0.3, .vx_mps = 16.6},
      {.fz_n = 4000.0, .kappa = 0.1, .alpha_rad = 0.1, .gamma_rad = 0.0, .vx_mps = 0.0},
      {.fz_n = 4000.0, .kappa = 0.1, .alpha_rad = 0.1, .gamma_rad = 0.0, .vx_mps = 0.5},
      {.fz_n = 4000.0, .kappa = 0.0, .alpha_rad = 1e-12, .gamma_rad = 0.0, .vx_mps = 16.6},
      {.fz_n = 4000.0, .kappa = 0.02, .alpha_rad = -1e-12, .gamma_rad = 0.0, .vx_mps = 16.6},
  }};

  /// A `rows` x INPUT_LENGTH input matrix whose row r is `REGIME_POINTS[r % 24]` (columns in `mf5x::input_index`
  /// order).
  [[nodiscard]] inline Eigen::MatrixXd regime_grid(Eigen::Index rows) {
    Eigen::MatrixXd grid(rows, mf::mf5x::INPUT_LENGTH);
    for (Eigen::Index r = 0; r < rows; ++r) {
      const auto position = static_cast<std::ptrdiff_t>(static_cast<std::size_t>(r) % REGIME_POINTS.size());
      grid.row(r) = input_of(*std::next(REGIME_POINTS.begin(), position)).transpose();
    }
    return grid;
  }
} // namespace mf::test
