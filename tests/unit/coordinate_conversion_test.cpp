#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf52_model.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_forms.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <magic_formula/mf5x_signs.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/pac2002_model.hpp>
#include <magic_formula/sign_transform.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The coordinate transforms are exact coefficient sign flips (coordinate-systems.md). This property test builds a
// model, builds the model with transformed coefficients and limits, and checks that evaluating the transformed model
// on the transformed inputs gives the transformed outputs of the original, on random coefficient sets.
namespace {
  using mf::sign_transform;
  using mf::sign_transforms;
  using mf::test::bits;
  using mf::test::random_profile;

  /// The map is exact up to libm and SIMD differences between the two evaluations.
  constexpr double transform_rel_tol = 1e-12;

  constexpr std::uint64_t FIRST_SEED = 1000;
  constexpr std::uint64_t SET_COUNT = 50;
  constexpr Eigen::Index POINT_COUNT = 200;

  enum class variant : std::uint8_t { PAC_COSINE, PAC_ELLIPSE, PAC_COSINE_SHIFT_MY, PAC_ELLIPSE_SHIFT_MY, MF52 };
  enum class start_state : std::uint8_t { ISO_SIDE, VERTICAL_FLIPPED };

  // clang-format off
  constexpr std::array<variant, 5> ALL_VARIANTS{
      variant::PAC_COSINE,
      variant::PAC_ELLIPSE,
      variant::PAC_COSINE_SHIFT_MY,
      variant::PAC_ELLIPSE_SHIFT_MY,
      variant::MF52,
  };
  constexpr std::array<std::string_view, 5> VARIANT_NAMES{
      "PAC_COSINE",
      "PAC_ELLIPSE",
      "PAC_COSINE_SHIFT_MY",
      "PAC_ELLIPSE_SHIFT_MY",
      "MF52",
  };
  // clang-format on
  constexpr std::array<mf::slip_mode, 2> ALL_MODES{mf::slip_mode::COMBINED, mf::slip_mode::UNCOMBINED};
  constexpr std::array<std::string_view, 2> MODE_NAMES{"COMBINED", "UNCOMBINED"};
  constexpr std::array<start_state, 2> ALL_STARTS{start_state::ISO_SIDE, start_state::VERTICAL_FLIPPED};
  constexpr std::array<std::string_view, 2> START_NAMES{"ISO_SIDE", "VERTICAL_FLIPPED"};
  constexpr sign_transforms PLAN_VERTICAL{.vertical = true, .lateral = false};
  constexpr sign_transforms PLAN_LATERAL{.vertical = false, .lateral = true};
  constexpr sign_transforms PLAN_BOTH{.vertical = true, .lateral = true};
  constexpr std::array<sign_transforms, 3> ALL_PLANS{PLAN_VERTICAL, PLAN_LATERAL, PLAN_BOTH};
  constexpr std::array<std::string_view, 3> PLAN_NAMES{"V", "L", "VL"};

  /// One instance of the parameter grid.
  struct configuration {
    std::size_t variant_index = 0;
    std::size_t mode_index = 0;
    bool ranges = false;
    std::size_t start_index = 0;
    std::size_t plan_index = 0;
  };

  variant variant_of(const configuration &cfg) {
    return std::span<const variant>{ALL_VARIANTS}[cfg.variant_index];
  }

  mf::slip_mode mode_of(const configuration &cfg) {
    return std::span<const mf::slip_mode>{ALL_MODES}[cfg.mode_index];
  }

  start_state start_of(const configuration &cfg) {
    return std::span<const start_state>{ALL_STARTS}[cfg.start_index];
  }

  sign_transforms plan_of(const configuration &cfg) {
    return std::span<const sign_transforms>{ALL_PLANS}[cfg.plan_index];
  }

  bool is_pac(variant v) {
    return v != variant::MF52;
  }

  bool is_ellipse(variant v) {
    return v == variant::PAC_ELLIPSE || v == variant::PAC_ELLIPSE_SHIFT_MY;
  }

  bool is_shift_my(variant v) {
    return v == variant::PAC_COSINE_SHIFT_MY || v == variant::PAC_ELLIPSE_SHIFT_MY;
  }

  std::vector<configuration> make_grid() {
    std::vector<configuration> grid;
    for (std::size_t v = 0; v < ALL_VARIANTS.size(); ++v) {
      for (std::size_t m = 0; m < ALL_MODES.size(); ++m) {
        for (const bool ranges : {false, true}) {
          for (std::size_t s = 0; s < ALL_STARTS.size(); ++s) {
            for (std::size_t p = 0; p < ALL_PLANS.size(); ++p) {
              configuration cfg;
              cfg.variant_index = v;
              cfg.mode_index = m;
              cfg.ranges = ranges;
              cfg.start_index = s;
              cfg.plan_index = p;
              grid.push_back(cfg);
            }
          }
        }
      }
    }
    return grid;
  }

  std::string name_of(const ::testing::TestParamInfo<configuration> &info) {
    const configuration &cfg = info.param;
    const std::string_view variant_name = std::span<const std::string_view>{VARIANT_NAMES}[cfg.variant_index];
    const std::string_view mode_name = std::span<const std::string_view>{MODE_NAMES}[cfg.mode_index];
    const std::string_view ranges_name = cfg.ranges ? "RANGES" : "NO_RANGES";
    const std::string_view start_name = std::span<const std::string_view>{START_NAMES}[cfg.start_index];
    const std::string_view plan_name = std::span<const std::string_view>{PLAN_NAMES}[cfg.plan_index];
    return std::format("{}_{}_{}_{}_{}", variant_name, mode_name, ranges_name, start_name, plan_name);
  }

  /// The WILD set of `seed` for `v`: shift-form My variants set FITTYP 5 with QSY1 = QSY2 = 0, MF 5.2 sets FITTYP 6;
  /// random ranges when `ranges`.
  mf::mf5x_parameters drawn_parameters(variant v, bool ranges, std::uint64_t seed) {
    mf::mf5x_parameters parameters = mf::test::random_mf5x_parameters(seed, random_profile::WILD);
    if (is_shift_my(v)) {
      parameters.metadata.fittyp = 5;
      parameters.coefficients.QSY1 = 0.0;
      parameters.coefficients.QSY2 = 0.0;
    }
    if (v == variant::MF52) {
      parameters.metadata.fittyp = 6;
    }
    if (ranges) {
      const double fz0_abs = std::abs(parameters.coefficients.FNOMIN * parameters.coefficients.LFZO);
      parameters.limits = mf::test::random_limits(seed, fz0_abs);
    }
    return parameters;
  }

  /// `source` with the coefficients and limits transformed by `transforms`.
  mf::mf5x_parameters transformed_parameters(const mf::mf5x_parameters &source, const sign_transforms &transforms) {
    mf::mf5x_parameters result;
    result.coefficients = mf::mf5x::transform_coefficients(source.coefficients, transforms);
    result.limits = mf::transform_limits(source.limits, mf::mf5x::LIMITS, transforms);
    result.metadata = source.metadata;
    return result;
  }

  /// The model of `v` for `parameters`; a creation failure is a test failure and gives null.
  std::unique_ptr<mf::tire_model> make_model(variant v, const mf::mf5x_parameters &parameters, mf::slip_mode mode) {
    const mf::model_options options{.mode = mode, .throw_if_model_failure = false};
    if (v == variant::MF52) {
      auto created = mf::mf52_model::create(parameters, options);
      if (!created.has_value()) {
        ADD_FAILURE() << mf::to_string(created.error());
        return nullptr;
      }
      return std::make_unique<mf::mf52_model>(std::move(*created));
    }
    mf::combined_method method = mf::combined_method::COSINE;
    if (is_ellipse(v)) {
      method = mf::combined_method::FRICTION_ELLIPSE;
    }
    auto created = mf::pac2002_model::create(parameters, method, options);
    if (!created.has_value()) {
      ADD_FAILURE() << mf::to_string(created.error());
      return nullptr;
    }
    return std::make_unique<mf::pac2002_model>(std::move(*created));
  }

  /// Force and moment scales of the comparison: |F'z0| [N] and |F'z0|*R0 [N*m].
  struct scales {
    double force_n = 0.0;
    double moment_nm = 0.0;
  };

  /// Forces and moments within `transform_rel_tol` of `expected`, clamp and speed flags exactly equal.
  void expect_outputs_match(const Eigen::MatrixXd &actual, const Eigen::MatrixXd &expected, const scales &scale,
                            Eigen::Index first_point = 0) {
    ASSERT_EQ(actual.rows(), expected.rows());
    ASSERT_EQ(actual.cols(), expected.cols());
    for (Eigen::Index c = 0; c < actual.cols(); ++c) {
      const bool is_flag = c >= mf::mf5x::FZ_CLAMPED;
      const bool is_force = c == mf::mf5x::FX || c == mf::mf5x::FY;
      const double column_scale = is_force ? scale.force_n : scale.moment_nm;
      for (Eigen::Index r = 0; r < actual.rows(); ++r) {
        if (is_flag) {
          ASSERT_TRUE(actual(r, c) == expected(r, c)) << "flag column " << c << " point " << (first_point + r);
        } else {
          ASSERT_TRUE(mf::test::near_scaled(actual(r, c), expected(r, c), transform_rel_tol, column_scale))
              << "column " << c << " point " << (first_point + r);
        }
      }
    }
  }

  /// One random set of `cfg`: the original model on x against the transformed model on the transformed x.
  void check_set(const configuration &cfg, std::uint64_t seed) {
    const variant v = variant_of(cfg);
    const sign_transforms plan = plan_of(cfg);
    const mf::mf5x_parameters drawn = drawn_parameters(v, cfg.ranges, seed);
    mf::mf5x_parameters start = drawn;
    if (start_of(cfg) == start_state::VERTICAL_FLIPPED) {
      start = transformed_parameters(drawn, sign_transforms{.vertical = true, .lateral = false});
    }
    const mf::mf5x_parameters target = transformed_parameters(start, plan);

    const std::unique_ptr<mf::tire_model> model = make_model(v, start, mode_of(cfg));
    const std::unique_ptr<mf::tire_model> transformed = make_model(v, target, mode_of(cfg));
    ASSERT_TRUE(model != nullptr);
    ASSERT_TRUE(transformed != nullptr);
    EXPECT_EQ(model->name(), transformed->name());
    if (is_pac(v)) {
      const auto *const pac = dynamic_cast<const mf::pac2002_model *>(model.get());
      const auto *const pac_transformed = dynamic_cast<const mf::pac2002_model *>(transformed.get());
      ASSERT_TRUE(pac != nullptr);
      ASSERT_TRUE(pac_transformed != nullptr);
      EXPECT_EQ(pac_transformed->uses_my_from_shifts(), pac->uses_my_from_shifts());
      EXPECT_EQ(pac->uses_my_from_shifts(), is_shift_my(v));
      EXPECT_EQ(pac_transformed->method(), pac->method());
    }

    const mf::model_layout &layout = model->layout();
    const double fz0_abs = std::abs(start.coefficients.FNOMIN * start.coefficients.LFZO);
    const scales scale{.force_n = fz0_abs, .moment_nm = fz0_abs * start.coefficients.UNLOADED_RADIUS};
    const double load_sign = std::copysign(1.0, start.coefficients.FNOMIN);

    const Eigen::MatrixXd x = mf::test::random_operating_points(seed, POINT_COUNT, load_sign);
    Eigen::MatrixXd x_transformed = x;
    mf::apply_sign_transforms(layout.inputs, plan, x_transformed);

    // Batch path.
    const Eigen::MatrixXd y = model->evaluate_batch(x);
    const Eigen::MatrixXd y_transformed = transformed->evaluate_batch(x_transformed);
    Eigen::MatrixXd y_expected = y;
    mf::apply_sign_transforms(layout.outputs, plan, y_expected);
    ASSERT_NO_FATAL_FAILURE(expect_outputs_match(y_transformed, y_expected, scale));

    // Per-point path.
    for (Eigen::Index r = 0; r < x.rows(); ++r) {
      const Eigen::VectorXd input = x.row(r).transpose();
      const Eigen::VectorXd input_transformed = x_transformed.row(r).transpose();
      const Eigen::MatrixXd output = model->evaluate(input).transpose();
      const Eigen::MatrixXd output_transformed = transformed->evaluate(input_transformed).transpose();
      Eigen::MatrixXd output_expected = output;
      mf::apply_sign_transforms(layout.outputs, plan, output_expected);
      ASSERT_NO_FATAL_FAILURE(expect_outputs_match(output_transformed, output_expected, scale, r));
    }
  }

  class coordinate_conversion_grid : public ::testing::TestWithParam<configuration> {};
} // namespace

TEST_P(coordinate_conversion_grid, Transform_BatchAndPoint_OutputsMapExactly) {
  const configuration &cfg = GetParam();
  for (std::uint64_t k = 0; k < SET_COUNT; ++k) {
    const std::uint64_t seed = FIRST_SEED + k;
    SCOPED_TRACE(std::format("seed {}", seed));
    ASSERT_NO_FATAL_FAILURE(check_set(cfg, seed));
  }
}

INSTANTIATE_TEST_SUITE_P(grid, coordinate_conversion_grid, ::testing::ValuesIn(make_grid()), name_of);

TEST(coordinate_conversion, Involution_CoefficientsAndLimits_BitExact) {
  for (std::uint64_t seed = 1; seed <= 50; ++seed) {
    const mf::mf5x_coefficients original = mf::test::random_mf5x_parameters(seed, random_profile::WILD).coefficients;
    const Eigen::VectorXd limits = mf::test::random_limits(seed, 4000.0);
    for (const sign_transforms &plan : ALL_PLANS) {
      SCOPED_TRACE(std::format("seed {} vertical {} lateral {}", seed, plan.vertical, plan.lateral));
      const mf::mf5x_coefficients once = mf::mf5x::transform_coefficients(original, plan);
      const mf::mf5x_coefficients twice = mf::mf5x::transform_coefficients(once, plan);
      EXPECT_TRUE(mf::test::coefficients_bitwise_equal(twice, original));
      const Eigen::VectorXd limits_once = mf::transform_limits(limits, mf::mf5x::LIMITS, plan);
      const Eigen::VectorXd limits_twice = mf::transform_limits(limits_once, mf::mf5x::LIMITS, plan);
      EXPECT_TRUE(mf::test::limits_bitwise_equal(limits_twice, limits));
    }
  }
}

TEST(coordinate_conversion, EmptyPlan_CreatePath_OutputsBitIdentical) {
  const sign_transforms identity{.vertical = false, .lateral = false};
  for (const variant v : ALL_VARIANTS) {
    for (const mf::slip_mode mode : ALL_MODES) {
      for (const bool ranges : {false, true}) {
        for (std::uint64_t seed = 1; seed <= 5; ++seed) {
          SCOPED_TRACE(std::format("seed {}", seed));
          const mf::mf5x_parameters drawn = drawn_parameters(v, ranges, seed);
          const mf::mf5x_parameters same = transformed_parameters(drawn, identity);
          const std::unique_ptr<mf::tire_model> model = make_model(v, drawn, mode);
          const std::unique_ptr<mf::tire_model> copy = make_model(v, same, mode);
          ASSERT_TRUE(model != nullptr);
          ASSERT_TRUE(copy != nullptr);
          const Eigen::MatrixXd x = mf::test::random_operating_points(seed, POINT_COUNT, 1.0);
          const Eigen::MatrixXd y = model->evaluate_batch(x);
          const Eigen::MatrixXd y_copy = copy->evaluate_batch(x);
          ASSERT_EQ(y.rows(), y_copy.rows());
          ASSERT_EQ(y.cols(), y_copy.cols());
          for (Eigen::Index c = 0; c < y.cols(); ++c) {
            for (Eigen::Index r = 0; r < y.rows(); ++r) {
              ASSERT_EQ(bits(y(r, c)), bits(y_copy(r, c))) << "column " << c << " row " << r;
            }
          }
        }
      }
    }
  }
}

namespace {
  /// The MF 5.x view of `model` (the model must be an MF 5.x model).
  const mf::mf5x_model *as_mf5x(const std::unique_ptr<mf::tire_model> &model) {
    return dynamic_cast<const mf::mf5x_model *>(model.get());
  }

  /// Success iff both batches have the same shape and the same bit pattern at every position.
  ::testing::AssertionResult outputs_bitwise_equal(const Eigen::MatrixXd &a, const Eigen::MatrixXd &b) {
    if (a.rows() != b.rows() || a.cols() != b.cols()) {
      return ::testing::AssertionFailure() << "shapes differ";
    }
    for (Eigen::Index c = 0; c < a.cols(); ++c) {
      for (Eigen::Index r = 0; r < a.rows(); ++r) {
        if (bits(a(r, c)) != bits(b(r, c))) {
          const std::string message = std::format("column {} row {}: {:.17g} vs {:.17g}", c, r, a(r, c), b(r, c));
          return ::testing::AssertionFailure() << message;
        }
      }
    }
    return ::testing::AssertionSuccess();
  }
} // namespace

TEST(coordinate_conversion, Transformed_EqualsCreatePath_Bitwise) {
  for (const variant v : ALL_VARIANTS) {
    for (const mf::slip_mode mode : ALL_MODES) {
      for (const bool ranges : {false, true}) {
        for (const sign_transforms &plan : ALL_PLANS) {
          for (std::uint64_t seed = 1; seed <= 5; ++seed) {
            SCOPED_TRACE(std::format("seed {} v {} l {} ranges {}", seed, plan.vertical, plan.lateral, ranges));
            const mf::mf5x_parameters drawn = drawn_parameters(v, ranges, seed);
            const std::unique_ptr<mf::tire_model> model = make_model(v, drawn, mode);
            const std::unique_ptr<mf::tire_model> by_create = make_model(v, transformed_parameters(drawn, plan), mode);
            ASSERT_TRUE(model != nullptr);
            ASSERT_TRUE(by_create != nullptr);
            const std::unique_ptr<mf::tire_model> by_transformed = model->transformed(plan);
            ASSERT_TRUE(by_transformed != nullptr);
            const mf::mf5x_model *const a = as_mf5x(by_transformed);
            const mf::mf5x_model *const b = as_mf5x(by_create);
            ASSERT_TRUE(a != nullptr);
            ASSERT_TRUE(b != nullptr);
            EXPECT_TRUE(mf::test::coefficients_bitwise_equal(a->coefficients(), b->coefficients()));
            EXPECT_TRUE(mf::test::limits_bitwise_equal(a->limits(), b->limits()));
            const double load_sign = std::copysign(1.0, b->coefficients().FNOMIN);
            const Eigen::MatrixXd x = mf::test::random_operating_points(seed, POINT_COUNT, load_sign);
            EXPECT_TRUE(outputs_bitwise_equal(by_transformed->evaluate_batch(x), by_create->evaluate_batch(x)));
          }
        }
      }
    }
  }
}

TEST(coordinate_conversion, Transformed_Twice_RestoresCoefficientsAndLimitsBitwise) {
  for (const variant v : ALL_VARIANTS) {
    for (const bool ranges : {false, true}) {
      for (const sign_transforms &plan : ALL_PLANS) {
        for (std::uint64_t seed = 1; seed <= 5; ++seed) {
          SCOPED_TRACE(std::format("seed {} v {} l {} ranges {}", seed, plan.vertical, plan.lateral, ranges));
          const mf::mf5x_parameters drawn = drawn_parameters(v, ranges, seed);
          const std::unique_ptr<mf::tire_model> model = make_model(v, drawn, mf::slip_mode::COMBINED);
          ASSERT_TRUE(model != nullptr);
          const std::unique_ptr<mf::tire_model> twice = model->transformed(plan)->transformed(plan);
          const mf::mf5x_model *const original = as_mf5x(model);
          const mf::mf5x_model *const restored = as_mf5x(twice);
          ASSERT_TRUE(original != nullptr);
          ASSERT_TRUE(restored != nullptr);
          EXPECT_TRUE(mf::test::coefficients_bitwise_equal(restored->coefficients(), original->coefficients()));
          EXPECT_TRUE(mf::test::limits_bitwise_equal(restored->limits(), original->limits()));
        }
      }
    }
  }
}

TEST(coordinate_conversion, Transformed_EmptyPlan_OutputsBitIdentical) {
  const sign_transforms identity{.vertical = false, .lateral = false};
  for (const variant v : ALL_VARIANTS) {
    for (const mf::slip_mode mode : ALL_MODES) {
      for (const bool ranges : {false, true}) {
        for (std::uint64_t seed = 1; seed <= 5; ++seed) {
          SCOPED_TRACE(std::format("seed {} ranges {}", seed, ranges));
          const std::unique_ptr<mf::tire_model> model = make_model(v, drawn_parameters(v, ranges, seed), mode);
          ASSERT_TRUE(model != nullptr);
          const std::unique_ptr<mf::tire_model> copy = model->transformed(identity);
          ASSERT_TRUE(copy != nullptr);
          const Eigen::MatrixXd x = mf::test::random_operating_points(seed, POINT_COUNT, 1.0);
          EXPECT_TRUE(outputs_bitwise_equal(model->evaluate_batch(x), copy->evaluate_batch(x)));
          const mf::mf5x_model *const copied = as_mf5x(copy);
          const mf::mf5x_model *const original = as_mf5x(model);
          ASSERT_TRUE(copied != nullptr);
          ASSERT_TRUE(original != nullptr);
          EXPECT_TRUE(mf::test::coefficients_bitwise_equal(copied->coefficients(), original->coefficients()));
          EXPECT_TRUE(mf::test::limits_bitwise_equal(copy->limits(), model->limits()));
        }
      }
    }
  }
}

TEST(coordinate_conversion, WildSets_ReferenceCorneringStiffness_NonZero) {
  constexpr double step_rad = 1e-4;
  for (const variant v : {variant::PAC_COSINE, variant::MF52}) {
    for (std::uint64_t seed = 1; seed <= 50; ++seed) {
      SCOPED_TRACE(std::format("seed {}", seed));
      const mf::mf5x_parameters parameters = drawn_parameters(v, false, seed);
      const std::unique_ptr<mf::tire_model> model = make_model(v, parameters, mf::slip_mode::COMBINED);
      ASSERT_TRUE(model != nullptr);
      Eigen::VectorXd plus = model->reference_input();
      Eigen::VectorXd minus = plus;
      plus(mf::mf5x::SA) += step_rad;
      minus(mf::mf5x::SA) -= step_rad;
      const double fy_plus = model->evaluate(plus)(mf::mf5x::FY);
      const double fy_minus = model->evaluate(minus)(mf::mf5x::FY);
      const double slope = (fy_plus - fy_minus) / (2.0 * step_rad);
      EXPECT_TRUE(std::isfinite(slope));
      EXPECT_NE(slope, 0.0);
    }
  }
}
