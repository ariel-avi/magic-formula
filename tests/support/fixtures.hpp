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
#include <magic_formula/io/mf5x_bindings.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/mf5x_signs.hpp>
#include <magic_formula/sign_transform.hpp>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

  /// Fixture S with PVY3, PKY1, PKX1 and QDZ1 replaced (camber thrust and sign variants for coordinate
  /// identification).
  [[nodiscard]] inline mf::mf5x_parameters fixture_s_with(double pvy3, double pky1 = -15.0, double pkx1 = 20.0,
                                                          double qdz1 = 0.1) {
    mf::mf5x_parameters parameters = fixture_s();
    parameters.coefficients.PVY3 = pvy3;
    parameters.coefficients.PKY1 = pky1;
    parameters.coefficients.PKX1 = pkx1;
    parameters.coefficients.QDZ1 = qdz1;
    return parameters;
  }

  /// Fixture S-iso: a consistent ISO set (the contents of pac2002_S_iso.tir): fixture S with PKY1 = 15 and PVY3 = -0.1.
  [[nodiscard]] inline mf::mf5x_parameters fixture_s_iso() {
    return fixture_s_with(-0.1, 15.0);
  }

  /// Fixture S-sae: a consistent SAE set (the contents of pac2002_S_sae.tir): fixture S-iso with the VERTICAL sign
  /// table applied (FNOMIN = -4000, PDX1 = -1, PKX1 = -20); default limits.
  [[nodiscard]] inline mf::mf5x_parameters fixture_s_sae() {
    const mf::mf5x_parameters iso = fixture_s_iso();
    return mf::mf5x_parameters{
        .coefficients = mf::mf5x::transform_coefficients(iso.coefficients, mf::sign_transforms{.vertical = true}),
        .limits = mf::default_limits(mf::mf5x::LIMITS),
        .metadata = iso.metadata};
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

  /// The coefficient distributions of `random_mf5x_parameters`.
  enum class random_profile : std::uint8_t {
    WILD,          ///< every coefficient non-zero with a random sign; scaling factors in [0.7, 1.3]
    PLAUSIBLE_ISO, ///< a plausible ISO-side set: positive stiffnesses and small shifts
  };

  /// A uniform double in [0, 1) from 53 bits of `rng` (portable: no std::uniform_real_distribution, whose output
  /// differs between standard libraries).
  [[nodiscard]] inline double unit_uniform(std::mt19937_64 &rng) {
    return static_cast<double>(rng() >> 11U) * 0x1.0p-53;
  }

  /// A uniform double in [lo, hi).
  [[nodiscard]] inline double uniform(std::mt19937_64 &rng, double lo, double hi) {
    return lo + ((hi - lo) * unit_uniform(rng));
  }

  /// A random sign times a uniform magnitude in [lo, hi).
  [[nodiscard]] inline double signed_magnitude(std::mt19937_64 &rng, double lo, double hi) {
    const double sign = unit_uniform(rng) < 0.5 ? -1.0 : 1.0;
    const double magnitude = uniform(rng, lo, hi);
    return sign * magnitude;
  }

  /// Draws the coefficients of the WILD profile: every binding, then the camber factors, then the overrides that keep
  /// the set well-posed (Ky != 0, R0 > 0, ...).
  inline void draw_wild_coefficients(std::mt19937_64 &rng, mf::mf5x_coefficients &c) {
    for (const mf::io::coefficient_binding &binding : mf::io::mf5x_common_bindings()) {
      const bool scaling = binding.role == mf::io::binding_role::SCALING;
      c.*(binding.member) = scaling ? uniform(rng, 0.7, 1.3) : signed_magnitude(rng, 0.02, 0.6);
    }
    for (const mf::io::coefficient_binding &binding : mf::io::pac2002_camber_bindings()) {
      c.*(binding.member) = uniform(rng, 0.7, 1.3);
    }
    c.FNOMIN = uniform(rng, 2000.0, 6000.0);
    c.UNLOADED_RADIUS = uniform(rng, 0.25, 0.4);
    c.LONGVL = 16.7;
    c.PKX1 = signed_magnitude(rng, 15.0, 30.0);
    c.PKY1 = signed_magnitude(rng, 10.0, 25.0);
    c.PKY2 = signed_magnitude(rng, 1.2, 2.5);
  }

  /// Draws the coefficients of the PLAUSIBLE_ISO profile: a plausible passenger-tire set on the ISO side.
  inline void draw_plausible_iso_coefficients(std::mt19937_64 &rng, mf::mf5x_coefficients &c) {
    for (const mf::io::coefficient_binding &binding : mf::io::mf5x_common_bindings()) {
      c.*(binding.member) = binding.role == mf::io::binding_role::SCALING ? 1.0 : 0.0;
    }
    for (const mf::io::coefficient_binding &binding : mf::io::pac2002_camber_bindings()) {
      c.*(binding.member) = 1.0;
    }
    c.LONGVL = 16.7;
    c.FNOMIN = uniform(rng, 3000.0, 6000.0);
    c.UNLOADED_RADIUS = uniform(rng, 0.28, 0.36);
    c.PCX1 = uniform(rng, 1.4, 1.7);
    c.PDX1 = uniform(rng, 0.9, 1.2);
    c.PDX2 = uniform(rng, -0.1, -0.01);
    c.PEX1 = uniform(rng, -0.5, 0.3);
    c.PKX1 = uniform(rng, 15.0, 30.0);
    c.PKX2 = uniform(rng, -5.0, 5.0);
    c.PHX1 = uniform(rng, -0.002, 0.002);
    c.PVX1 = uniform(rng, -0.02, 0.02);
    c.PCY1 = uniform(rng, 1.1, 1.6);
    c.PDY1 = uniform(rng, 0.8, 1.2);
    c.PDY2 = uniform(rng, -0.1, -0.01);
    c.PEY1 = uniform(rng, -1.0, 0.0);
    c.PKY1 = uniform(rng, 10.0, 25.0);
    c.PKY2 = uniform(rng, 1.2, 2.5);
    c.PHY1 = uniform(rng, -0.005, 0.005);
    c.PHY2 = uniform(rng, -0.005, 0.005);
    c.PVY1 = uniform(rng, -0.03, 0.03);
    c.PVY2 = uniform(rng, -0.03, 0.03);
    c.PVY3 = uniform(rng, -0.2, -0.05);
    c.QSY1 = uniform(rng, 0.005, 0.02);
    c.QBZ1 = uniform(rng, 5.0, 12.0);
    c.QCZ1 = uniform(rng, 1.05, 1.3);
    c.QDZ1 = uniform(rng, 0.05, 0.15);
    c.QEZ1 = uniform(rng, -2.0, 0.0);
    c.RBX1 = uniform(rng, 8.0, 13.0);
    c.RCX1 = uniform(rng, 0.9, 1.1);
    c.RBY1 = uniform(rng, 6.0, 10.0);
    c.RCY1 = uniform(rng, 0.9, 1.1);
  }

  /// Random MF 5.x parameters of `profile` from `seed` (mt19937_64 and the portable draws above, so the sets are the
  /// same on every platform). No ranges; metadata {no FITTYP, USE_MODE 4, LEFT}.
  [[nodiscard]] inline mf::mf5x_parameters random_mf5x_parameters(std::uint64_t seed, random_profile profile) {
    std::mt19937_64 rng(seed);
    mf::mf5x_parameters parameters;
    parameters.metadata = mf::model_metadata{.fittyp = std::nullopt, .use_mode = 4, .side = mf::tyre_side::LEFT};
    if (profile == random_profile::WILD) {
      draw_wild_coefficients(rng, parameters.coefficients);
    } else {
      draw_plausible_iso_coefficients(rng, parameters.coefficients);
    }
    return parameters;
  }

  /// Random finite ranges in `mf5x::limit_index` order, scaled to the nominal load magnitude `fz0_abs` [N]: FZ on
  /// the ISO side, symmetric slip, angle and camber ranges, VXLOW in [1, 10] m/s.
  [[nodiscard]] inline Eigen::VectorXd random_limits(std::uint64_t seed, double fz0_abs) {
    std::mt19937_64 rng(seed);
    Eigen::VectorXd limits(mf::mf5x::LIMITS_LENGTH);
    limits(mf::mf5x::FZMIN) = uniform(rng, 0.4, 0.8) * fz0_abs;
    limits(mf::mf5x::FZMAX) = uniform(rng, 1.2, 1.6) * fz0_abs;
    limits(mf::mf5x::KPUMIN) = -uniform(rng, 0.05, 0.25);
    limits(mf::mf5x::KPUMAX) = uniform(rng, 0.05, 0.25);
    limits(mf::mf5x::ALPMIN) = -uniform(rng, 0.05, 0.25);
    limits(mf::mf5x::ALPMAX) = uniform(rng, 0.05, 0.25);
    limits(mf::mf5x::CAMMIN) = -uniform(rng, 0.02, 0.08);
    limits(mf::mf5x::CAMMAX) = uniform(rng, 0.02, 0.08);
    limits(mf::mf5x::VXLOW) = uniform(rng, 1.0, 10.0);
    return limits;
  }

  /// `rows` random operating points (rows x INPUT_LENGTH, `mf5x::input_index` columns) of a tire loaded on the side
  /// `load_sign` (+1 or -1). Exact zeros in SA, SR and IA on every 17th, 19th and 23rd row; every 29th row has Fz on
  /// the wrong side, every 31st Fz = 0 and every 13th Vx = 0.5 m/s.
  [[nodiscard]] inline Eigen::MatrixXd random_operating_points(std::uint64_t seed, Eigen::Index rows,
                                                               double load_sign) {
    std::mt19937_64 rng(seed);
    Eigen::MatrixXd points(rows, mf::mf5x::INPUT_LENGTH);
    for (Eigen::Index r = 0; r < rows; ++r) {
      double fz = load_sign * uniform(rng, 1000.0, 9000.0);
      double sa = signed_magnitude(rng, 0.003, 0.3);
      double sr = signed_magnitude(rng, 0.003, 0.3);
      double ia = signed_magnitude(rng, 0.002, 0.1);
      double vx = uniform(rng, 5.0, 40.0);
      if (r % 17 == 16) {
        sa = 0.0;
      }
      if (r % 19 == 18) {
        sr = 0.0;
      }
      if (r % 23 == 22) {
        ia = 0.0;
      }
      if (r % 29 == 28) {
        fz = -load_sign * 3000.0;
      }
      if (r % 31 == 30) {
        fz = 0.0;
      }
      if (r % 13 == 12) {
        vx = 0.5;
      }
      points(r, mf::mf5x::FZ) = fz;
      points(r, mf::mf5x::SA) = sa;
      points(r, mf::mf5x::SR) = sr;
      points(r, mf::mf5x::IA) = ia;
      points(r, mf::mf5x::VX) = vx;
    }
    return points;
  }

  /// Every coefficient member with its key: the 113 common bindings, LONGVL, then LGAX, LGAY and LGAZ (117 entries).
  [[nodiscard]] inline std::vector<std::pair<std::string_view, double mf::mf5x_coefficients::*>>
  all_coefficient_members() {
    std::vector<std::pair<std::string_view, double mf::mf5x_coefficients::*>> members;
    for (const mf::io::coefficient_binding &binding : mf::io::mf5x_common_bindings()) {
      members.emplace_back(binding.key, binding.member);
    }
    members.emplace_back("LONGVL", &mf::mf5x_coefficients::LONGVL);
    for (const mf::io::coefficient_binding &binding : mf::io::pac2002_camber_bindings()) {
      members.emplace_back(binding.key, binding.member);
    }
    return members;
  }

  /// Success iff every one of the 117 members has the same bit pattern in `a` and `b`; the failure names the first
  /// differing key with both values.
  [[nodiscard]] inline ::testing::AssertionResult coefficients_bitwise_equal(const mf::mf5x_coefficients &a,
                                                                             const mf::mf5x_coefficients &b) {
    for (const auto &[key, member] : all_coefficient_members()) {
      if (bits(a.*member) != bits(b.*member)) {
        const std::string message = std::format("{}: {:.17g} vs {:.17g}", key, a.*member, b.*member);
        return ::testing::AssertionFailure() << message;
      }
    }
    return ::testing::AssertionSuccess();
  }

  /// Like `coefficients_bitwise_equal` with operator==, so +0.0 equals -0.0.
  [[nodiscard]] inline ::testing::AssertionResult coefficients_value_equal(const mf::mf5x_coefficients &a,
                                                                           const mf::mf5x_coefficients &b) {
    for (const auto &[key, member] : all_coefficient_members()) {
      if (!(a.*member == b.*member)) {
        const std::string message = std::format("{}: {:.17g} vs {:.17g}", key, a.*member, b.*member);
        return ::testing::AssertionFailure() << message;
      }
    }
    return ::testing::AssertionSuccess();
  }

  /// Success iff `a` and `b` have the same size and the same bit pattern at every position.
  [[nodiscard]] inline ::testing::AssertionResult limits_bitwise_equal(const Eigen::VectorXd &a,
                                                                       const Eigen::VectorXd &b) {
    if (a.size() != b.size()) {
      return ::testing::AssertionFailure() << std::format("sizes {} vs {}", a.size(), b.size());
    }
    for (Eigen::Index i = 0; i < a.size(); ++i) {
      if (bits(a(i)) != bits(b(i))) {
        const std::string message = std::format("limit {}: {:.17g} vs {:.17g}", i, a(i), b(i));
        return ::testing::AssertionFailure() << message;
      }
    }
    return ::testing::AssertionSuccess();
  }

  /// Like `limits_bitwise_equal` with operator==, so +0.0 equals -0.0.
  [[nodiscard]] inline ::testing::AssertionResult limits_value_equal(const Eigen::VectorXd &a,
                                                                     const Eigen::VectorXd &b) {
    if (a.size() != b.size()) {
      return ::testing::AssertionFailure() << std::format("sizes {} vs {}", a.size(), b.size());
    }
    for (Eigen::Index i = 0; i < a.size(); ++i) {
      if (!(a(i) == b(i))) {
        const std::string message = std::format("limit {}: {:.17g} vs {:.17g}", i, a(i), b(i));
        return ::testing::AssertionFailure() << message;
      }
    }
    return ::testing::AssertionSuccess();
  }
} // namespace mf::test
