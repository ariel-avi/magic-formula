#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/detail/lanes.hpp>
#include <span>
#include <vector>

namespace {
  using mf::detail::block_lane;
  using mf::detail::mask_of;
  using mf::detail::point_lane;
  using mf::test::bits;

  constexpr double INF = std::numeric_limits<double>::infinity();
  constexpr double NAN_VALUE = std::numeric_limits<double>::quiet_NaN();
  // Absolute tolerance for results computed as d + 1e-6*sgn(d) and compared with a decimal literal.
  constexpr double EPSILON_TOL = 1e-12;

  // A lane holding `values` (one value for a point lane, any number up to BLOCK_ROWS for a block lane).
  template <class Lane_T> Lane_T lane_of(std::span<const double> values) {
    Lane_T lane = Lane_T::Zero(static_cast<Eigen::Index>(values.size()));
    for (Eigen::Index i = 0; i < lane.size(); ++i) {
      lane(i) = values[static_cast<std::size_t>(i)];
    }
    return lane;
  }

  // Calls fn(first index, lane) once per value for a point lane, and once for all values for a block lane.
  template <class Lane_T, class Fn_T> void for_each_lane(const std::vector<double> &values, Fn_T fn) {
    constexpr bool single = Lane_T::RowsAtCompileTime == 1;
    const std::size_t step = single ? 1 : values.size();
    for (std::size_t begin = 0; begin < values.size(); begin += step) {
      const auto lane = lane_of<Lane_T>(std::span<const double>(values).subspan(begin, step));
      fn(begin, lane);
    }
  }

  void expect_same_bits(double actual, double expected) {
    EXPECT_EQ(bits(actual), bits(expected));
  }

  void expect_near_epsilon(double actual, double expected) {
    EXPECT_NEAR(actual, expected, EPSILON_TOL);
  }

  // Applies op to every value (point lane) or to all values at once (block lane) and compares each element.
  template <class Lane_T, class Op_T, class Cmp_T>
  void check_unary(const std::vector<double> &inputs, const std::vector<double> &expected, Op_T op, Cmp_T compare) {
    ASSERT_EQ(inputs.size(), expected.size());
    for_each_lane<Lane_T>(inputs, [&](std::size_t begin, const Lane_T &lane) {
      const Lane_T result = op(lane);
      ASSERT_EQ(result.size(), lane.size());
      for (Eigen::Index i = 0; i < result.size(); ++i) {
        compare(result(i), expected[begin + static_cast<std::size_t>(i)]);
      }
    });
  }

  template <class Lane_T> void check_sgn_plus() {
    const std::vector<double> inputs{2.0, -2.0, +0.0, -0.0};
    const std::vector<double> expected{1.0, -1.0, 1.0, 1.0};
    check_unary<Lane_T>(inputs, expected, [](const Lane_T &x) { return mf::detail::sgn_plus(x); }, expect_same_bits);
  }

  template <class Lane_T> void check_add_epsilon() {
    const std::vector<double> inputs{0.0, -1.0, 2.0};
    const std::vector<double> expected{1e-6, -1.000001, 2.000001};
    check_unary<Lane_T>(
        inputs, expected, [](const Lane_T &d) { return mf::detail::add_epsilon(d); }, expect_near_epsilon);
  }

  template <class Lane_T> void check_limit_curvature() {
    const std::vector<double> inputs{0.5, 1.0, 3.0, -4.0};
    const std::vector<double> expected{0.5, 1.0, 1.0, -4.0};
    check_unary<Lane_T>(
        inputs, expected, [](const Lane_T &e) { return mf::detail::limit_curvature(e); }, expect_same_bits);
  }

  template <class Lane_T> void check_hypot() {
    const std::vector<double> xs{3.0, 1e300, 0.0, -5.0, INF};
    const std::vector<double> ys{4.0, 1e300, -0.0, 12.0, NAN_VALUE};
    for_each_lane<Lane_T>(xs, [&](std::size_t begin, const Lane_T &x) {
      const auto y = lane_of<Lane_T>(std::span<const double>(ys).subspan(begin, static_cast<std::size_t>(x.size())));
      const Lane_T result = mf::detail::elementwise_hypot(x, y);
      ASSERT_EQ(result.size(), x.size());
      for (Eigen::Index i = 0; i < result.size(); ++i) {
        const std::size_t index = begin + static_cast<std::size_t>(i);
        expect_same_bits(result(i), std::hypot(xs[index], ys[index]));
      }
    });
  }

  // Clamps `inputs` to [lower, upper]; every result and every moved flag is compared bit for bit.
  template <class Lane_T>
  void check_clamp(const std::vector<double> &inputs, double lower, double upper, const std::vector<double> &expected,
                   const std::vector<double> &expected_moved) {
    ASSERT_EQ(inputs.size(), expected.size());
    ASSERT_EQ(inputs.size(), expected_moved.size());
    for_each_lane<Lane_T>(inputs, [&](std::size_t begin, const Lane_T &lane) {
      Lane_T moved = Lane_T::Zero(lane.size());
      const Lane_T result = mf::detail::clamp_to_limits(lane, lower, upper, moved);
      ASSERT_EQ(result.size(), lane.size());
      ASSERT_EQ(moved.size(), lane.size());
      for (Eigen::Index i = 0; i < result.size(); ++i) {
        const std::size_t index = begin + static_cast<std::size_t>(i);
        expect_same_bits(result(i), expected[index]);
        expect_same_bits(moved(i), expected_moved[index]);
      }
    });
  }

  template <class Lane_T> void check_to_flag() {
    const std::vector<double> truth{1.0, 0.0, 0.0, 1.0, 1.0};
    for_each_lane<Lane_T>(truth, [&](std::size_t begin, const Lane_T &lane) {
      const mask_of<Lane_T> mask = lane > 0.5;
      const auto flags = mf::detail::to_flag<Lane_T>(mask);
      ASSERT_EQ(flags.size(), lane.size());
      for (Eigen::Index i = 0; i < flags.size(); ++i) {
        expect_same_bits(flags(i), truth[begin + static_cast<std::size_t>(i)]);
      }
    });
  }
} // namespace

TEST(lanes, BlockRows_Is32AndBlockLaneMaxRowsMatches) {
  EXPECT_EQ(mf::detail::BLOCK_ROWS, 32);
  EXPECT_EQ(static_cast<Eigen::Index>(block_lane::MaxRowsAtCompileTime), mf::detail::BLOCK_ROWS);
  EXPECT_EQ(static_cast<Eigen::Index>(block_lane::RowsAtCompileTime), static_cast<Eigen::Index>(Eigen::Dynamic));
  EXPECT_EQ(static_cast<Eigen::Index>(point_lane::RowsAtCompileTime), 1);
  EXPECT_EQ(static_cast<Eigen::Index>(point_lane::MaxRowsAtCompileTime), 1);
}

TEST(lanes, SgnPlus_PointLane_PositiveNegativeAndBothZeros) {
  check_sgn_plus<point_lane>();
}

TEST(lanes, SgnPlus_BlockLane_PositiveNegativeAndBothZeros) {
  check_sgn_plus<block_lane>();
}

TEST(lanes, SgnPlus_SpecialValues_BitIdenticalToReference) {
  const std::vector<double> inputs{-1.0, -0.0, +0.0, 1.0, NAN_VALUE};
  const std::vector<double> expected{-1.0, 1.0, 1.0, 1.0, -1.0};
  check_unary<block_lane>(
      inputs, expected, [](const block_lane &x) { return mf::detail::sgn_plus(x); }, expect_same_bits);
  check_unary<point_lane>(
      inputs, expected, [](const point_lane &x) { return mf::detail::sgn_plus(x); }, expect_same_bits);
}

TEST(lanes, SelectLanes_PointLaneMaskTrue_ReturnsIfTrue) {
  const point_lane if_true = point_lane::Constant(3.0);
  const point_lane if_false = point_lane::Constant(-7.0);
  const point_lane result = mf::detail::select_lanes(if_true > 0.0, if_true, if_false);
  expect_same_bits(result(0), 3.0);
}

TEST(lanes, SelectLanes_PointLaneMaskFalse_ReturnsIfFalse) {
  const point_lane if_true = point_lane::Constant(3.0);
  const point_lane if_false = point_lane::Constant(-7.0);
  const point_lane result = mf::detail::select_lanes(if_true < 0.0, if_true, if_false);
  expect_same_bits(result(0), -7.0);
}

TEST(lanes, SelectLanes_BlockLaneMixedMask_SelectsPerElement) {
  const auto if_true = lane_of<block_lane>(std::vector<double>{1.0, 2.0, 3.0, 4.0});
  const auto if_false = lane_of<block_lane>(std::vector<double>{-1.0, -2.0, -3.0, -4.0});
  const mask_of<block_lane> mask = lane_of<block_lane>(std::vector<double>{1.0, 0.0, 1.0, 0.0}) > 0.5;
  const block_lane result = mf::detail::select_lanes(mask, if_true, if_false);
  ASSERT_EQ(result.size(), 4);
  expect_same_bits(result(0), 1.0);
  expect_same_bits(result(1), -2.0);
  expect_same_bits(result(2), 3.0);
  expect_same_bits(result(3), -4.0);
}

TEST(lanes, AddEpsilon_PointAndBlock_MovesAwayFromZeroKeepingSign) {
  check_add_epsilon<point_lane>();
  check_add_epsilon<block_lane>();
}

TEST(lanes, LimitCurvature_PointAndBlock_CapsAtOne) {
  check_limit_curvature<point_lane>();
  check_limit_curvature<block_lane>();
}

TEST(lanes, ElementwiseHypot_PointAndBlock_BitEqualStdHypot) {
  check_hypot<point_lane>();
  check_hypot<block_lane>();
}

TEST(lanes, ClampToLimits_PointAndBlock_BelowAboveInsideAtLimits) {
  const std::vector<double> inputs{-3.0, 5.0, 0.5, -1.0, 2.0};
  const std::vector<double> expected{-1.0, 2.0, 0.5, -1.0, 2.0};
  const std::vector<double> moved{1.0, 1.0, 0.0, 0.0, 0.0};
  check_clamp<point_lane>(inputs, -1.0, 2.0, expected, moved);
  check_clamp<block_lane>(inputs, -1.0, 2.0, expected, moved);
}

TEST(lanes, ClampToLimits_NegativeZeroAtZeroLower_NotMovedSignKept) {
  const std::vector<double> inputs{-0.0};
  const std::vector<double> expected{-0.0};
  const std::vector<double> moved{0.0};
  check_clamp<point_lane>(inputs, 0.0, 1.0, expected, moved);
  check_clamp<block_lane>(inputs, 0.0, 1.0, expected, moved);
}

TEST(lanes, ClampToLimits_NaN_PassesThroughNotMoved) {
  const std::vector<double> inputs{NAN_VALUE, 0.5};
  const auto check = [&]<class Lane_T>(const Lane_T & /*tag*/) {
    for_each_lane<Lane_T>(inputs, [&](std::size_t begin, const Lane_T &lane) {
      Lane_T moved = Lane_T::Zero(lane.size());
      const Lane_T result = mf::detail::clamp_to_limits(lane, -1.0, 1.0, moved);
      for (Eigen::Index i = 0; i < result.size(); ++i) {
        const std::size_t index = begin + static_cast<std::size_t>(i);
        if (index == 0) {
          EXPECT_TRUE(std::isnan(result(i)));
        } else {
          expect_same_bits(result(i), 0.5);
        }
        expect_same_bits(moved(i), 0.0);
      }
    });
  };
  check(point_lane{});
  check(block_lane{});
}

TEST(lanes, ClampToLimits_InfiniteBounds_NeverMoved) {
  const std::vector<double> inputs{1e308, -1e308};
  const std::vector<double> moved{0.0, 0.0};
  check_clamp<point_lane>(inputs, -INF, INF, inputs, moved);
  check_clamp<block_lane>(inputs, -INF, INF, inputs, moved);
}

TEST(lanes, ToFlag_PointAndBlock_TrueOneFalseZero) {
  check_to_flag<point_lane>();
  check_to_flag<block_lane>();
}

TEST(lanes, Epsilon_Is1e6) {
  EXPECT_EQ(bits(mf::detail::EPSILON), bits(1e-6));
}
