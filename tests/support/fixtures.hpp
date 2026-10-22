#pragma once

#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <gtest/gtest.h>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <optional>
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

  /// Path of a file in tests/data (MF_TEST_DATA_DIR is defined by `mf_add_test`).
  [[nodiscard]] inline std::filesystem::path data_file(std::string_view name) {
    return std::filesystem::path{MF_TEST_DATA_DIR} / std::filesystem::path{name};
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
    parameters.limits.vxlow_mps = 1.0;
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
    parameters.limits.fz_n = mf::interval{.lower = 200.0, .upper = 10000.0};
    parameters.limits.kappa = mf::interval{.lower = -1.0, .upper = 1.0};
    parameters.limits.alpha_rad = mf::interval{.lower = -1.5, .upper = 1.5};
    parameters.limits.gamma_rad = mf::interval{.lower = -0.2, .upper = 0.2};
    return parameters;
  }
} // namespace mf::test
