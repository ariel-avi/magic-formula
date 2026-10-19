#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
  constexpr double QUIET_NAN = std::numeric_limits<double>::quiet_NaN();
  constexpr double POS_INF = std::numeric_limits<double>::infinity();
  constexpr double NEG_INF = -std::numeric_limits<double>::infinity();
  /// Tolerance for values that are stored and read back without arithmetic.
  constexpr double STORED_TOL = 1e-12;
  /// Limit interval used by the clamp tests, for every variable.
  constexpr double CLAMP_LOWER = 1.0;
  constexpr double CLAMP_UPPER = 2.0;
  constexpr double INSIDE_VALUE = 1.5;
  constexpr std::array<std::string_view, 5> OUTPUT_KEYS{"FX", "FY", "MZ", "MX", "MY"};

  /// Bit pattern of a double. Echoed inputs and zero outputs are compared bit-exactly (identity vectors, see
  /// cpp-conventions: the documented exception to "no EXPECT_EQ on doubles").
  std::uint64_t bits(double value) {
    return std::bit_cast<std::uint64_t>(value);
  }

  /// Test model: echoes the CLAMPED inputs into the outputs and counts `compute` calls.
  /// fx = Fz, fy = kappa, mz = alpha, mx = gamma, my = (UNCOMBINED ? -Vx : Vx).
  /// When `nan_output` holds k (0..4), output k (FX, FY, MZ, MX, MY) is a quiet NaN.
  class stub_model final : public mf::tire_model {
  public:
    explicit stub_model(const mf::operating_limits &limits = {}, const mf::model_metadata &metadata = {},
                        std::optional<std::size_t> nan_output = std::nullopt)
        : tire_model(limits, metadata), nan_output_(nan_output) {}

    [[nodiscard]] std::string_view name() const noexcept override {
      return "stub";
    }

    /// Number of `compute` calls so far.
    [[nodiscard]] std::size_t compute_calls() const noexcept {
      return calls_;
    }

  private:
    [[nodiscard]] mf::tire_outputs compute(const mf::operating_point &point,
                                           mf::slip_mode mode) const noexcept override {
      ++calls_;
      mf::tire_outputs out{};
      out.fx_n = point.fz_n;
      out.fy_n = point.kappa;
      out.mz_nm = point.alpha_rad;
      out.mx_nm = point.gamma_rad;
      out.my_nm = mode == mf::slip_mode::UNCOMBINED ? -point.vx_mps : point.vx_mps;
      const std::array<double mf::tire_outputs::*, 5> members{{
          &mf::tire_outputs::fx_n,
          &mf::tire_outputs::fy_n,
          &mf::tire_outputs::mz_nm,
          &mf::tire_outputs::mx_nm,
          &mf::tire_outputs::my_nm,
      }};
      std::size_t index = 0;
      for (double mf::tire_outputs::*member : members) {
        if (nan_output_ == index) {
          out.*member = QUIET_NAN;
        }
        ++index;
      }
      return out;
    }

    std::optional<std::size_t> nan_output_;
    mutable std::size_t calls_ = 0;
  };

  mf::operating_point nominal_point() {
    return {.fz_n = 4000.0, .kappa = 0.1, .alpha_rad = 0.05, .gamma_rad = 0.02, .vx_mps = 10.0};
  }

  int count_clamp_flags(const mf::input_flags &flags) {
    return static_cast<int>(flags.fz_clamped) + static_cast<int>(flags.kappa_clamped) +
           static_cast<int>(flags.alpha_clamped) + static_cast<int>(flags.gamma_clamped);
  }

  bool contains(const std::string &text, std::string_view part) {
    return text.find(part) != std::string::npos;
  }

  /// True when all five outputs are bit-equal to +0.0 (the AD-01 zero outputs).
  bool all_outputs_positive_zero(const mf::tire_outputs &out) {
    const std::uint64_t zero = bits(0.0);
    return bits(out.fx_n) == zero && bits(out.fy_n) == zero && bits(out.mz_nm) == zero && bits(out.mx_nm) == zero &&
           bits(out.my_nm) == zero;
  }
} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// validate_limits
// ---------------------------------------------------------------------------------------------------------------------

TEST(validate_limits, ValidateLimits_AllAbsent_Succeeds) {
  EXPECT_TRUE(mf::validate_limits(mf::operating_limits{}).has_value());
}

TEST(validate_limits, ValidateLimits_ValidIntervals_Succeeds) {
  mf::operating_limits limits;
  limits.fz_n = mf::interval{.lower = 200.0, .upper = 10000.0};
  limits.kappa = mf::interval{.lower = -1.0, .upper = 1.0};
  limits.alpha_rad = mf::interval{.lower = 0.5, .upper = 0.5}; // lower == upper is valid
  limits.gamma_rad = mf::interval{.lower = -0.2, .upper = 0.2};
  EXPECT_TRUE(mf::validate_limits(limits).has_value());
}

namespace {
  struct limits_case {
    std::string min_key;
    std::string max_key;
    std::optional<mf::interval> mf::operating_limits::*member;
  };
} // namespace

class validate_limits_invalid : public ::testing::TestWithParam<limits_case> {};

TEST_P(validate_limits_invalid, ValidateLimits_LowerAboveUpper_ReturnsInvalidValueNamingBothKeys) {
  const limits_case &param = GetParam();
  mf::operating_limits limits;
  limits.*(param.member) = mf::interval{.lower = 2.0, .upper = 1.0};
  const std::expected<void, mf::error> result = mf::validate_limits(limits);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_VALUE);
  EXPECT_EQ(result.error().where.key, param.min_key);
  EXPECT_TRUE(contains(result.error().message, param.min_key));
  EXPECT_TRUE(contains(result.error().message, param.max_key));
}

namespace {
  std::vector<limits_case> limits_cases() {
    const std::array<limits_case, 4> cases{{
        {.min_key = "FZMIN", .max_key = "FZMAX", .member = &mf::operating_limits::fz_n},
        {.min_key = "KPUMIN", .max_key = "KPUMAX", .member = &mf::operating_limits::kappa},
        {.min_key = "ALPMIN", .max_key = "ALPMAX", .member = &mf::operating_limits::alpha_rad},
        {.min_key = "CAMMIN", .max_key = "CAMMAX", .member = &mf::operating_limits::gamma_rad},
    }};
    return {cases.begin(), cases.end()};
  }

  std::string limits_case_name(const ::testing::TestParamInfo<limits_case> &param_info) {
    return param_info.param.min_key;
  }
} // namespace

INSTANTIATE_TEST_SUITE_P(limit_pairs, validate_limits_invalid, ::testing::ValuesIn(limits_cases()), limits_case_name);

// ---------------------------------------------------------------------------------------------------------------------
// tire_model: input validation (AD-02)
// ---------------------------------------------------------------------------------------------------------------------

namespace {
  struct non_finite_case {
    std::string field;
    double mf::operating_point::*member;
    double value;
    std::string kind;
  };

  std::vector<non_finite_case> non_finite_cases() {
    const std::array<std::pair<std::string, double mf::operating_point::*>, 5> fields{{
        {"FZ", &mf::operating_point::fz_n},
        {"SR", &mf::operating_point::kappa},
        {"SA", &mf::operating_point::alpha_rad},
        {"IA", &mf::operating_point::gamma_rad},
        {"VX", &mf::operating_point::vx_mps},
    }};
    const std::array<std::pair<std::string, double>, 3> values{{
        {"nan", QUIET_NAN},
        {"pinf", POS_INF},
        {"ninf", NEG_INF},
    }};
    std::vector<non_finite_case> cases;
    for (const auto &[field, member] : fields) {
      for (const auto &[kind, value] : values) {
        cases.push_back(non_finite_case{.field = field, .member = member, .value = value, .kind = kind});
      }
    }
    return cases;
  }
} // namespace

class tire_model_non_finite : public ::testing::TestWithParam<non_finite_case> {};

TEST_P(tire_model_non_finite, Evaluate_NonFiniteField_ReturnsInvalidInputNamingField) {
  const non_finite_case &param = GetParam();
  const stub_model model;
  mf::operating_point point = nominal_point();
  point.*(param.member) = param.value;
  const std::expected<mf::tire_outputs, mf::error> result = model.evaluate(point);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, param.field);
  EXPECT_TRUE(contains(result.error().message, param.field));
  EXPECT_EQ(model.compute_calls(), 0U);
}

namespace {
  std::string non_finite_case_name(const ::testing::TestParamInfo<non_finite_case> &param_info) {
    return param_info.param.field + "_" + param_info.param.kind;
  }
} // namespace

INSTANTIATE_TEST_SUITE_P(fields_and_values, tire_model_non_finite, ::testing::ValuesIn(non_finite_cases()),
                         non_finite_case_name);

TEST(tire_model, Evaluate_SeveralNonFiniteFields_ReportsFirstInFieldOrder) {
  const stub_model model;
  mf::operating_point point = nominal_point();
  point.vx_mps = QUIET_NAN;
  point.alpha_rad = QUIET_NAN;
  point.fz_n = POS_INF;
  const std::expected<mf::tire_outputs, mf::error> result = model.evaluate(point);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().where.key, "FZ");
}

class tire_model_negative_vx : public ::testing::TestWithParam<double> {};

TEST_P(tire_model_negative_vx, Evaluate_NegativeVx_ReturnsInvalidInputVx) {
  const stub_model model;
  mf::operating_point point = nominal_point();
  point.vx_mps = GetParam();
  const std::expected<mf::tire_outputs, mf::error> result = model.evaluate(point);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
  EXPECT_TRUE(contains(result.error().message, "AD-02"));
  EXPECT_EQ(model.compute_calls(), 0U);
}

INSTANTIATE_TEST_SUITE_P(speeds, tire_model_negative_vx, ::testing::Values(-1.0, -1e-300));

TEST(tire_model, Evaluate_NegativeZeroVx_IsAccepted) {
  const stub_model model;
  mf::operating_point point = nominal_point();
  point.vx_mps = -0.0;
  EXPECT_TRUE(model.evaluate(point).has_value());
  EXPECT_EQ(model.compute_calls(), 1U);
}

TEST(tire_model, Evaluate_ZeroVx_IsAccepted) {
  const stub_model model;
  mf::operating_point point = nominal_point();
  point.vx_mps = 0.0;
  EXPECT_TRUE(model.evaluate(point).has_value());
  EXPECT_EQ(model.compute_calls(), 1U);
}

// ---------------------------------------------------------------------------------------------------------------------
// tire_model: Fz <= 0 (AD-01) and VXLOW (AD-05)
// ---------------------------------------------------------------------------------------------------------------------

class tire_model_non_positive_fz : public ::testing::TestWithParam<double> {};

TEST_P(tire_model_non_positive_fz, Evaluate_NonPositiveFz_ReturnsExactZeros) {
  mf::operating_limits limits;
  limits.fz_n = mf::interval{.lower = 200.0, .upper = 10000.0}; // the raw Fz decides, before clamping
  const stub_model model(limits, mf::model_metadata{});
  mf::operating_point point = nominal_point();
  point.fz_n = GetParam();
  const std::expected<mf::tire_outputs, mf::error> result = model.evaluate(point);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(all_outputs_positive_zero(*result));
  EXPECT_FALSE(result->flags.fz_clamped);
  EXPECT_EQ(model.compute_calls(), 0U);
}

INSTANTIATE_TEST_SUITE_P(loads, tire_model_non_positive_fz, ::testing::Values(0.0, -0.0, -100.0));

class tire_model_fz_interval_end : public ::testing::TestWithParam<double> {};

TEST_P(tire_model_fz_interval_end, Evaluate_ClampedFzNonPositive_ReturnsExactZerosWithoutCompute) {
  mf::operating_limits limits;
  limits.fz_n = mf::interval{.lower = -10.0, .upper = GetParam()}; // accepted by validate_limits
  const stub_model model(limits, mf::model_metadata{});
  const std::expected<mf::tire_outputs, mf::error> result = model.evaluate(nominal_point());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(all_outputs_positive_zero(*result));
  EXPECT_TRUE(result->flags.fz_clamped);
  EXPECT_EQ(model.compute_calls(), 0U);
}

INSTANTIATE_TEST_SUITE_P(upper_limits, tire_model_fz_interval_end, ::testing::Values(0.0, -5.0));

TEST(tire_model, Evaluate_NonPositiveFzBelowVxlow_PreservesFlag) {
  mf::operating_limits limits;
  limits.vxlow_mps = 1.0;
  const stub_model model(limits, mf::model_metadata{});
  mf::operating_point point = nominal_point();
  point.fz_n = 0.0;
  point.vx_mps = 0.5;
  const std::expected<mf::tire_outputs, mf::error> result = model.evaluate(point);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->flags.below_vxlow);
  EXPECT_EQ(bits(result->fx_n), bits(0.0));
  EXPECT_EQ(bits(result->my_nm), bits(0.0));
  EXPECT_EQ(model.compute_calls(), 0U);
}

TEST(tire_model, Evaluate_VxBelowVxlow_SetsFlag) {
  mf::operating_limits limits;
  limits.vxlow_mps = 1.0;
  const stub_model model(limits, mf::model_metadata{});
  mf::operating_point point = nominal_point();
  point.vx_mps = 0.5;
  const std::expected<mf::tire_outputs, mf::error> result = model.evaluate(point);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->flags.below_vxlow);
}

TEST(tire_model, Evaluate_VxAtVxlow_ClearsFlag) {
  mf::operating_limits limits;
  limits.vxlow_mps = 1.0;
  const stub_model model(limits, mf::model_metadata{});
  mf::operating_point point = nominal_point();
  point.vx_mps = 1.0;
  const std::expected<mf::tire_outputs, mf::error> result = model.evaluate(point);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->flags.below_vxlow);
}

// ---------------------------------------------------------------------------------------------------------------------
// tire_model: clamping to the limits (AD-15)
// ---------------------------------------------------------------------------------------------------------------------

namespace {
  /// One clampable variable: where its limit lives, where the input is, where the stub echoes it, which flag it sets.
  struct clamp_case {
    std::string name;
    std::optional<mf::interval> mf::operating_limits::*limit_member = nullptr;
    double mf::operating_point::*input_member = nullptr;
    double mf::tire_outputs::*output_member = nullptr;
    bool mf::input_flags::*flag_member = nullptr;
  };

  constexpr std::array<std::string_view, 4> CLAMP_NAMES{"fz", "kappa", "alpha", "gamma"};

  const std::array<std::optional<mf::interval> mf::operating_limits::*, 4> LIMIT_MEMBERS{{
      &mf::operating_limits::fz_n,
      &mf::operating_limits::kappa,
      &mf::operating_limits::alpha_rad,
      &mf::operating_limits::gamma_rad,
  }};

  const std::array<double mf::operating_point::*, 4> INPUT_MEMBERS{{
      &mf::operating_point::fz_n,
      &mf::operating_point::kappa,
      &mf::operating_point::alpha_rad,
      &mf::operating_point::gamma_rad,
  }};

  /// The stub echoes Fz, SR, SA and IA into FX, FY, MZ and MX.
  const std::array<double mf::tire_outputs::*, 4> ECHO_MEMBERS{{
      &mf::tire_outputs::fx_n,
      &mf::tire_outputs::fy_n,
      &mf::tire_outputs::mz_nm,
      &mf::tire_outputs::mx_nm,
  }};

  const std::array<bool mf::input_flags::*, 4> FLAG_MEMBERS{{
      &mf::input_flags::fz_clamped,
      &mf::input_flags::kappa_clamped,
      &mf::input_flags::alpha_clamped,
      &mf::input_flags::gamma_clamped,
  }};

  std::vector<clamp_case> clamp_cases() {
    std::vector<clamp_case> cases;
    for (std::size_t index = 0; index < CLAMP_NAMES.size(); ++index) {
      clamp_case entry;
      entry.name = std::string(CLAMP_NAMES.at(index));
      entry.limit_member = LIMIT_MEMBERS.at(index);
      entry.input_member = INPUT_MEMBERS.at(index);
      entry.output_member = ECHO_MEMBERS.at(index);
      entry.flag_member = FLAG_MEMBERS.at(index);
      cases.push_back(entry);
    }
    return cases;
  }

  std::string clamp_case_name(const ::testing::TestParamInfo<clamp_case> &param_info) {
    return param_info.param.name;
  }

  mf::operating_limits limits_for(const clamp_case &param) {
    mf::operating_limits limits;
    limits.*(param.limit_member) = mf::interval{.lower = CLAMP_LOWER, .upper = CLAMP_UPPER};
    return limits;
  }

  /// All clampable inputs inside the interval, except `param`'s variable, which gets `value`.
  mf::operating_point point_with(const clamp_case &param, double value) {
    mf::operating_point point{.fz_n = INSIDE_VALUE,
                              .kappa = INSIDE_VALUE,
                              .alpha_rad = INSIDE_VALUE,
                              .gamma_rad = INSIDE_VALUE,
                              .vx_mps = 10.0};
    point.*(param.input_member) = value;
    return point;
  }

  /// Evaluates with `param`'s variable set to `value` and expects it to reach `compute` unchanged and unflagged.
  void expect_unflagged_echo(const clamp_case &param, double value) {
    const stub_model model(limits_for(param), mf::model_metadata{});
    const std::expected<mf::tire_outputs, mf::error> result = model.evaluate(point_with(param, value));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(bits((*result).*(param.output_member)), bits(value));
    EXPECT_EQ(count_clamp_flags(result->flags), 0);
  }
} // namespace

class tire_model_clamp : public ::testing::TestWithParam<clamp_case> {};

TEST_P(tire_model_clamp, Evaluate_InputBelowLowerLimit_ClampsAndFlags) {
  const clamp_case &param = GetParam();
  const stub_model model(limits_for(param), mf::model_metadata{});
  const std::expected<mf::tire_outputs, mf::error> result = model.evaluate(point_with(param, 0.5));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(bits((*result).*(param.output_member)), bits(CLAMP_LOWER));
  EXPECT_TRUE((*result).flags.*(param.flag_member));
  EXPECT_EQ(count_clamp_flags(result->flags), 1);
}

TEST_P(tire_model_clamp, Evaluate_InputAboveUpperLimit_ClampsAndFlags) {
  const clamp_case &param = GetParam();
  const stub_model model(limits_for(param), mf::model_metadata{});
  const std::expected<mf::tire_outputs, mf::error> result = model.evaluate(point_with(param, 3.0));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(bits((*result).*(param.output_member)), bits(CLAMP_UPPER));
  EXPECT_TRUE((*result).flags.*(param.flag_member));
  EXPECT_EQ(count_clamp_flags(result->flags), 1);
}

TEST_P(tire_model_clamp, Evaluate_InputAtLimits_PassesThroughUnflagged) {
  const clamp_case &param = GetParam();
  expect_unflagged_echo(param, CLAMP_LOWER);
  expect_unflagged_echo(param, CLAMP_UPPER);
}

TEST_P(tire_model_clamp, Evaluate_InputInside_PassesThroughUnflagged) {
  expect_unflagged_echo(GetParam(), INSIDE_VALUE);
}

INSTANTIATE_TEST_SUITE_P(variables, tire_model_clamp, ::testing::ValuesIn(clamp_cases()), clamp_case_name);

TEST(tire_model, Evaluate_LimitAbsent_PassesThroughUnflagged) {
  const stub_model model;
  const mf::operating_point point{.fz_n = 1e12, .kappa = -50.0, .alpha_rad = 7.0, .gamma_rad = -3.0, .vx_mps = 10.0};
  const std::expected<mf::tire_outputs, mf::error> result = model.evaluate(point);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(bits(result->fx_n), bits(1e12));
  EXPECT_EQ(bits(result->fy_n), bits(-50.0));
  EXPECT_EQ(bits(result->mz_nm), bits(7.0));
  EXPECT_EQ(bits(result->mx_nm), bits(-3.0));
  EXPECT_EQ(count_clamp_flags(result->flags), 0);
  EXPECT_EQ(model.compute_calls(), 1U);
}

TEST(tire_model, Evaluate_PositiveFzBelowFzmin_RaisedToFzmin) {
  mf::operating_limits limits;
  limits.fz_n = mf::interval{.lower = 200.0, .upper = 10000.0};
  const stub_model model(limits, mf::model_metadata{});
  mf::operating_point point = nominal_point();
  point.fz_n = 1e-9;
  const std::expected<mf::tire_outputs, mf::error> result = model.evaluate(point);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(bits(result->fx_n), bits(200.0));
  EXPECT_TRUE(result->flags.fz_clamped);
  EXPECT_EQ(model.compute_calls(), 1U);
}

// ---------------------------------------------------------------------------------------------------------------------
// tire_model: finite-output check (AD-22) and mode forwarding
// ---------------------------------------------------------------------------------------------------------------------

class tire_model_nan_output : public ::testing::TestWithParam<std::size_t> {};

TEST_P(tire_model_nan_output, Evaluate_NonFiniteOutput_ReturnsNumericalNamingOutput) {
  const std::size_t index = GetParam();
  const stub_model model(mf::operating_limits{}, mf::model_metadata{}, index);
  const std::expected<mf::tire_outputs, mf::error> result = model.evaluate(nominal_point());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::NUMERICAL);
  EXPECT_EQ(result.error().where.key, OUTPUT_KEYS.at(index));
  EXPECT_TRUE(contains(result.error().message, OUTPUT_KEYS.at(index)));
  EXPECT_TRUE(contains(result.error().message, "AD-22"));
  EXPECT_EQ(model.compute_calls(), 1U);
}

namespace {
  std::string output_name(const ::testing::TestParamInfo<std::size_t> &param_info) {
    return std::string(OUTPUT_KEYS.at(param_info.param));
  }
} // namespace

INSTANTIATE_TEST_SUITE_P(outputs, tire_model_nan_output, ::testing::Range<std::size_t>(0, 5), output_name);

TEST(tire_model, Evaluate_Mode_ForwardedToCompute) {
  const stub_model model;
  const mf::operating_point point = nominal_point();
  const std::expected<mf::tire_outputs, mf::error> combined = model.evaluate(point);
  const std::expected<mf::tire_outputs, mf::error> uncombined = model.evaluate(point, mf::slip_mode::UNCOMBINED);
  ASSERT_TRUE(combined.has_value());
  ASSERT_TRUE(uncombined.has_value());
  EXPECT_EQ(bits(combined->my_nm), bits(10.0));
  EXPECT_EQ(bits(uncombined->my_nm), bits(-10.0));
}

// ---------------------------------------------------------------------------------------------------------------------
// tire_model: batch evaluation
// ---------------------------------------------------------------------------------------------------------------------

TEST(tire_model, EvaluateBatch_AllValid_ReturnsOutputsInOrder) {
  const stub_model model;
  std::vector<mf::operating_point> points(3, nominal_point());
  points[0].fz_n = 1000.0;
  points[1].fz_n = 2000.0;
  points[2].fz_n = 3000.0;
  const std::expected<std::vector<mf::tire_outputs>, mf::batch_error> result = model.evaluate_batch(points);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 3U);
  EXPECT_EQ(bits(result->at(0).fx_n), bits(1000.0));
  EXPECT_EQ(bits(result->at(1).fx_n), bits(2000.0));
  EXPECT_EQ(bits(result->at(2).fx_n), bits(3000.0));
  EXPECT_EQ(model.compute_calls(), 3U);
}

TEST(tire_model, EvaluateBatch_FailureAtIndex_ReturnsBatchErrorWithIndex) {
  const stub_model model;
  std::vector<mf::operating_point> points(4, nominal_point());
  points[2].alpha_rad = QUIET_NAN;
  points[3].alpha_rad = QUIET_NAN;
  const std::expected<std::vector<mf::tire_outputs>, mf::batch_error> result = model.evaluate_batch(points);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().index, 2U);
  EXPECT_EQ(result.error().cause.code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().cause.where.key, "SA");
  EXPECT_EQ(model.compute_calls(), 2U); // stops at the first failure
}

TEST(tire_model, EvaluateBatch_EmptySpan_ReturnsEmptyVector) {
  const stub_model model;
  const std::expected<std::vector<mf::tire_outputs>, mf::batch_error> result =
      model.evaluate_batch(std::span<const mf::operating_point>{});
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST(tire_model, EvaluateBatch_ModeForwarded) {
  const stub_model model;
  const std::vector<mf::operating_point> points(1, nominal_point());
  const std::expected<std::vector<mf::tire_outputs>, mf::batch_error> result =
      model.evaluate_batch(points, mf::slip_mode::UNCOMBINED);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(bits(result->at(0).my_nm), bits(-10.0));
}

// ---------------------------------------------------------------------------------------------------------------------
// tire_model: accessors
// ---------------------------------------------------------------------------------------------------------------------

TEST(tire_model, Destroy_ThroughBasePointer_EvaluatesAndReleases) {
  const std::unique_ptr<const mf::tire_model> owned = std::make_unique<stub_model>();
  const std::expected<mf::tire_outputs, mf::error> result = owned->evaluate(nominal_point());
  EXPECT_TRUE(result.has_value());
}

TEST(tire_model, Accessors_Constructed_ReturnLimitsMetadataAndName) {
  mf::operating_limits limits;
  limits.fz_n = mf::interval{.lower = 200.0, .upper = 10000.0};
  limits.vxlow_mps = 3.5;
  const mf::model_metadata metadata{.fittyp = 6, .use_mode = 4, .side = mf::tyre_side::RIGHT};
  const stub_model model(limits, metadata);
  const mf::tire_model &base = model;
  EXPECT_EQ(base.name(), "stub");
  EXPECT_NEAR(base.limits().vxlow_mps, 3.5, STORED_TOL);
  const std::optional<mf::interval> &fz = base.limits().fz_n;
  ASSERT_TRUE(fz.has_value());
  const mf::interval fz_range = fz.value_or(mf::interval{});
  EXPECT_NEAR(fz_range.lower, 200.0, STORED_TOL);
  EXPECT_NEAR(fz_range.upper, 10000.0, STORED_TOL);
  EXPECT_FALSE(base.limits().kappa.has_value());
  EXPECT_EQ(base.metadata().fittyp, std::optional<int>(6));
  EXPECT_EQ(base.metadata().use_mode, std::optional<int>(4));
  EXPECT_EQ(base.metadata().side, mf::tyre_side::RIGHT);
}
