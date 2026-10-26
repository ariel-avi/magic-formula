#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <array>
#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/detail/lanes.hpp>
#include <magic_formula/detail/stationary_point.hpp>
#include <numbers>

namespace {
  using mf::detail::block_lane;
  using mf::detail::first_stationary_point;
  using mf::detail::point_lane;
  using mf::detail::STATIONARY_BISECTIONS;
  using mf::detail::STATIONARY_SCAN_CELLS;
  using mf::test::bits;

  constexpr double NAN_VALUE = std::numeric_limits<double>::quiet_NaN();

  // Root accuracy: the final bracket is at most one scan cell wide over 2^52 (bisection), plus a few ulp of the root
  // for the rounding of the midpoint arithmetic.
  double root_tol(double end, double root) {
    return (std::abs(end) / (static_cast<double>(STATIONARY_SCAN_CELLS) * 0x1p52)) +
           (4.0 * std::numeric_limits<double>::epsilon() * std::abs(root));
  }

  point_lane point_of(double value) {
    return point_lane::Constant(1, value);
  }

  double solve_point(double end, const auto &slope) {
    return first_stationary_point<point_lane>(slope, point_of(end))(0);
  }

  TEST(first_stationary_point, LinearSlope_PositiveEnd_ReturnsRoot) {
    const auto slope = [](const point_lane &s) noexcept -> point_lane { return 0.3 - s; };
    EXPECT_NEAR(solve_point(1.0, slope), 0.3, root_tol(1.0, 0.3));
  }

  TEST(first_stationary_point, LinearSlope_NegativeEnd_ReturnsNegativeRoot) {
    const auto slope = [](const point_lane &s) noexcept -> point_lane { return s + 0.3; };
    EXPECT_NEAR(solve_point(-1.0, slope), -0.3, root_tol(1.0, 0.3));
  }

  TEST(first_stationary_point, NoSignChange_ReturnsNaN) {
    const auto slope = [](const point_lane &s) noexcept -> point_lane { return 1.0 + (s * s); };
    EXPECT_TRUE(std::isnan(solve_point(1.0, slope)));
  }

  TEST(first_stationary_point, ZeroEnd_ReturnsNaN) {
    const auto slope = [](const point_lane &s) noexcept -> point_lane { return 0.3 - s; };
    EXPECT_TRUE(std::isnan(solve_point(0.0, slope)));
  }

  TEST(first_stationary_point, NaNSlope_ReturnsNaN) {
    const auto slope = [](const point_lane &s) noexcept -> point_lane {
      return point_lane::Constant(s.size(), NAN_VALUE);
    };
    EXPECT_TRUE(std::isnan(solve_point(1.0, slope)));
  }

  TEST(first_stationary_point, RootOnGridPoint_ReturnsGridPoint) {
    const auto slope = [](const point_lane &s) noexcept -> point_lane { return 0.25 - s; };
    EXPECT_NEAR(solve_point(1.0, slope), 0.25, root_tol(1.0, 0.25));
  }

  TEST(first_stationary_point, ZeroAtOriginThenConstantSign_ReturnsNaN) {
    const auto slope = [](const point_lane &s) noexcept -> point_lane { return -s; };
    EXPECT_TRUE(std::isnan(solve_point(1.0, slope)));
  }

  TEST(first_stationary_point, ZeroAtOriginThenLaterChange_FindsLaterRoot) {
    const auto slope = [](const point_lane &s) noexcept -> point_lane { return s * (0.5 - s); };
    EXPECT_NEAR(solve_point(1.0, slope), 0.5, root_tol(1.0, 0.5));
  }

  TEST(first_stationary_point, TwoRoots_ReturnsFirst) {
    const auto slope = [](const point_lane &s) noexcept -> point_lane { return (0.2 - s) * (0.7 - s); };
    EXPECT_NEAR(solve_point(1.0, slope), 0.2, root_tol(1.0, 0.2));
  }

  TEST(first_stationary_point, RisingSlope_FindsRoot) {
    const auto slope = [](const point_lane &s) noexcept -> point_lane { return s - 0.4; };
    EXPECT_NEAR(solve_point(1.0, slope), 0.4, root_tol(1.0, 0.4));
  }

  TEST(first_stationary_point, TrigonometricSlope_FindsRoot) {
    const auto slope = [](const point_lane &s) noexcept -> point_lane { return (3.0 * s).cos(); };
    const double expected = std::numbers::pi / 6.0;
    EXPECT_NEAR(solve_point(1.0, slope), expected, root_tol(1.0, expected));
  }

  TEST(first_stationary_point, MirroredSlopeNegatedEnd_ExactlyNegatedResult) {
    const auto positive = [](const point_lane &s) noexcept -> point_lane { return (3.0 * s).cos(); };
    const auto mirrored = [](const point_lane &s) noexcept -> point_lane { return (-3.0 * s).cos(); };
    const double forward = solve_point(1.0, positive);
    const double backward = solve_point(-1.0, mirrored);
    EXPECT_EQ(bits(backward), bits(-forward));
  }

  TEST(first_stationary_point, SlopeCallCount_EqualsScanPlusBisections) {
    int calls = 0;
    const auto slope = [&calls](const point_lane &s) noexcept -> point_lane {
      ++calls;
      return 0.3 - s;
    };
    (void)solve_point(1.0, slope);
    EXPECT_EQ(calls, STATIONARY_SCAN_CELLS + 1 + STATIONARY_BISECTIONS);
  }

  constexpr std::array<double, 5> BLOCK_ROOTS{0.3, -0.3, 0.3, 2.0, 0.6};
  constexpr std::array<double, 5> BLOCK_ENDS{1.0, -1.0, 0.0, 1.0, 1.0};
  constexpr std::array<double, 5> BLOCK_EXPECTED{0.3, -0.3, NAN_VALUE, NAN_VALUE, 0.6};

  block_lane block_of(const std::array<double, 5> &values) {
    block_lane lane = block_lane::Zero(static_cast<Eigen::Index>(values.size()));
    for (Eigen::Index i = 0; i < lane.size(); ++i) {
      lane(i) = values.at(static_cast<std::size_t>(i));
    }
    return lane;
  }

  TEST(first_stationary_point, BlockLane_MixedLanes_PerLaneResults) {
    const block_lane roots = block_of(BLOCK_ROOTS);
    const auto slope = [&roots](const block_lane &s) noexcept -> block_lane { return roots - s; };
    const auto result = first_stationary_point<block_lane>(slope, block_of(BLOCK_ENDS));
    ASSERT_EQ(result.size(), 5);
    for (std::size_t i = 0; i < BLOCK_EXPECTED.size(); ++i) {
      const double got = result(static_cast<Eigen::Index>(i));
      const double want = BLOCK_EXPECTED.at(i);
      if (std::isnan(want)) {
        EXPECT_TRUE(std::isnan(got)) << "lane " << i;
      } else {
        EXPECT_NEAR(got, want, root_tol(BLOCK_ENDS.at(i), want)) << "lane " << i;
      }
    }
  }

  TEST(first_stationary_point, BlockLane_EqualsPointLanePerElement) {
    const block_lane roots = block_of(BLOCK_ROOTS);
    const auto block_slope = [&roots](const block_lane &s) noexcept -> block_lane { return roots - s; };
    const auto result = first_stationary_point<block_lane>(block_slope, block_of(BLOCK_ENDS));
    for (std::size_t i = 0; i < BLOCK_ROOTS.size(); ++i) {
      const double root = BLOCK_ROOTS.at(i);
      const double end = BLOCK_ENDS.at(i);
      const auto point_slope = [root](const point_lane &s) noexcept -> point_lane { return root - s; };
      const double expected = solve_point(end, point_slope);
      const double got = result(static_cast<Eigen::Index>(i));
      if (std::isnan(expected)) {
        EXPECT_TRUE(std::isnan(got)) << "lane " << i;
      } else {
        EXPECT_NEAR(got, expected, root_tol(end, expected)) << "lane " << i;
      }
    }
  }
} // namespace
