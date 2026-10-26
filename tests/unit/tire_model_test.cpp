#include <Eigen/Core>
#include <array>
#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/sign_transform.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace {
  /// The stub values are exact small integers; the tolerance only documents that.
  constexpr double VALUE_TOL = 1e-12;

  constexpr double QUIET_NAN = std::numeric_limits<double>::quiet_NaN();

  /// Inputs A (>= 0) and B; outputs X, Y and Z.
  constexpr std::array<mf::input_channel, 2> STUB_INPUTS{{
      {.name = "A", .unit = "-", .minimum = 0.0, .minimum_inclusive = true},
      {.name = "B", .unit = "-"},
  }};
  constexpr std::array<mf::output_channel, 3> STUB_OUTPUTS{{
      {.name = "X", .unit = "-"},
      {.name = "Y", .unit = "-"},
      {.name = "Z", .unit = "-"},
  }};
  constexpr mf::model_layout STUB_LAYOUT{.inputs = STUB_INPUTS, .outputs = STUB_OUTPUTS, .limits = {}};

  /// The same layout with limits: a LOWER/UPPER pair negated by VERTICAL and a VALUE row.
  constexpr std::array<mf::limit_channel, 3> STUB_LIMITS{{
      {.key = "LO",
       .role = mf::limit_role::LOWER,
       .default_value = -std::numeric_limits<double>::infinity(),
       .negated_by = mf::sign_transform::VERTICAL},
      {.key = "HI",
       .role = mf::limit_role::UPPER,
       .default_value = std::numeric_limits<double>::infinity(),
       .negated_by = mf::sign_transform::VERTICAL},
      {.key = "V", .role = mf::limit_role::VALUE, .default_value = 1.0},
  }};
  constexpr mf::model_layout STUB_LAYOUT_LIMITS{.inputs = STUB_INPUTS, .outputs = STUB_OUTPUTS, .limits = STUB_LIMITS};

  /// X = A + B, Y = A * B, Z = 1 (or NaN when `emit_nan`), with call counters. `rebuild` records the plan.
  class stub_model final : public mf::tire_model {
  public:
    explicit stub_model(const mf::model_options &options, bool emit_nan = false,
                        const Eigen::VectorXd &limits = Eigen::VectorXd(),
                        const mf::model_metadata &metadata = mf::model_metadata{}, bool with_limit_layout = false,
                        double reference_value = 0.5)
        : tire_model(2, 3, limits, metadata, options), emit_nan_(emit_nan), with_limit_layout_(with_limit_layout),
          reference_value_(reference_value) {}

    [[nodiscard]] std::string_view name() const noexcept override {
      return "stub";
    }
    [[nodiscard]] int input_length() const noexcept override {
      return 2;
    }
    [[nodiscard]] int output_length() const noexcept override {
      return 3;
    }
    [[nodiscard]] const mf::model_layout &layout() const noexcept override {
      return with_limit_layout_ ? STUB_LAYOUT_LIMITS : STUB_LAYOUT;
    }
    [[nodiscard]] Eigen::VectorXd reference_input() const override {
      return Eigen::VectorXd::Constant(2, reference_value_);
    }

    mutable int compute_calls = 0;
    mutable int batch_calls = 0;
    /// Set on a model made by `rebuild`: the plan it was made with.
    bool rebuilt = false;
    mf::sign_transforms rebuilt_plan;

  private:
    [[nodiscard]] std::unique_ptr<mf::tire_model> rebuild(const mf::sign_transforms &transforms,
                                                          const Eigen::VectorXd &limits) const override {
      auto copy =
          std::make_unique<stub_model>(options(), emit_nan_, limits, metadata(), with_limit_layout_, reference_value_);
      copy->rebuilt = true;
      copy->rebuilt_plan = transforms;
      return copy;
    }

    void compute(const Eigen::Ref<const Eigen::VectorXd> &input,
                 Eigen::Ref<Eigen::VectorXd> output) const noexcept override {
      ++compute_calls;
      output(0) = input(0) + input(1);
      output(1) = input(0) * input(1);
      output(2) = emit_nan_ ? QUIET_NAN : 1.0;
    }

    void compute_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                       Eigen::Ref<Eigen::MatrixXd> outputs) const noexcept override {
      ++batch_calls;
      outputs.col(0) = inputs.col(0) + inputs.col(1);
      outputs.col(1) = (inputs.col(0).array() * inputs.col(1).array()).matrix();
      outputs.col(2).setConstant(emit_nan_ ? QUIET_NAN : 1.0);
    }

    bool emit_nan_;
    bool with_limit_layout_;
    double reference_value_;
  };

  constexpr std::array<mf::input_channel, 2> LOAD_INPUTS{{{.name = "FZ", .unit = "N"}, {.name = "B", .unit = "-"}}};
  constexpr mf::model_layout LOAD_LAYOUT{.inputs = LOAD_INPUTS, .outputs = STUB_OUTPUTS, .limits = {}};

  /// A model with an FZ input and a configurable reference load; its outputs are constant.
  class load_stub final : public mf::tire_model {
  public:
    explicit load_stub(double reference_fz)
        : tire_model(2, 3, Eigen::VectorXd(), mf::model_metadata{}, mf::model_options{}), reference_fz_(reference_fz) {}

    [[nodiscard]] std::string_view name() const noexcept override {
      return "load-stub";
    }
    [[nodiscard]] int input_length() const noexcept override {
      return 2;
    }
    [[nodiscard]] int output_length() const noexcept override {
      return 3;
    }
    [[nodiscard]] const mf::model_layout &layout() const noexcept override {
      return LOAD_LAYOUT;
    }
    [[nodiscard]] Eigen::VectorXd reference_input() const override {
      Eigen::VectorXd input = Eigen::VectorXd::Zero(2);
      input(0) = reference_fz_;
      return input;
    }

  private:
    [[nodiscard]] std::unique_ptr<mf::tire_model> rebuild(const mf::sign_transforms & /*transforms*/,
                                                          const Eigen::VectorXd & /*limits*/) const override {
      return std::make_unique<load_stub>(reference_fz_);
    }

    void compute(const Eigen::Ref<const Eigen::VectorXd> & /*input*/,
                 Eigen::Ref<Eigen::VectorXd> output) const noexcept override {
      output.setZero();
    }

    void compute_batch(const Eigen::Ref<const Eigen::MatrixXd> & /*inputs*/,
                       Eigen::Ref<Eigen::MatrixXd> outputs) const noexcept override {
      outputs.setZero();
    }

    double reference_fz_;
  };

  mf::model_options checked_options() {
    return mf::model_options{.mode = mf::slip_mode::COMBINED, .throw_if_model_failure = true};
  }

  mf::model_options unchecked_options() {
    return mf::model_options{};
  }

  Eigen::VectorXd point_of(double a, double b) {
    Eigen::VectorXd input(2);
    input << a, b;
    return input;
  }

  /// A 5 x 2 matrix with rows (0, 1), (1, 2), (2, 3), (3, 4), (4, 5).
  Eigen::MatrixXd ramp_inputs() {
    Eigen::MatrixXd inputs(5, 2);
    for (Eigen::Index r = 0; r < inputs.rows(); ++r) {
      inputs(r, 0) = static_cast<double>(r);
      inputs(r, 1) = static_cast<double>(r + 1);
    }
    return inputs;
  }

  /// Runs `call` once and returns the `mf::model_failure` it throws, or nothing when it does not throw.
  template <typename Callable_T> std::optional<mf::model_failure> caught_failure(const Callable_T &call) {
    try {
      call();
    } catch (const mf::model_failure &failure) {
      return failure;
    }
    return std::nullopt;
  }

  /// Expects `call` to throw `mf::model_failure` with the given code, key and row.
  template <typename Callable_T>
  void expect_failure(const Callable_T &call, mf::error_code code, std::string_view key,
                      std::optional<std::size_t> row) {
    const std::optional<mf::model_failure> failure = caught_failure(call);
    if (!failure.has_value()) {
      ADD_FAILURE() << "expected mf::model_failure";
      return;
    }
    EXPECT_EQ(failure->cause().code, code);
    EXPECT_EQ(failure->cause().where.key, key);
    EXPECT_EQ(failure->row(), row);
  }

  /// Expects `call` to throw an INVALID_ARGUMENT `mf::model_failure` without key or row.
  template <typename Callable_T> void expect_invalid_argument(const Callable_T &call) {
    expect_failure(call, mf::error_code::INVALID_ARGUMENT, "", std::nullopt);
  }
} // namespace

TEST(tire_model, EvaluateRef_Unchecked_ReturnsComputeOutputsCallsComputeOnce) {
  const stub_model model(unchecked_options());
  const Eigen::VectorXd output = model.evaluate(point_of(2.0, 3.0));
  ASSERT_EQ(output.size(), 3);
  EXPECT_NEAR(output(0), 5.0, VALUE_TOL);
  EXPECT_NEAR(output(1), 6.0, VALUE_TOL);
  EXPECT_NEAR(output(2), 1.0, VALUE_TOL);
  EXPECT_EQ(model.compute_calls, 1);
  EXPECT_EQ(model.batch_calls, 0);
}

TEST(tire_model, EvaluateRaw_Unchecked_WritesBufferCallsCompute) {
  const stub_model model(unchecked_options());
  const std::array<double, 2> input{2.0, 3.0};
  std::array<double, 3> data{};
  model.evaluate(input.data(), 2, data.data(), 3);
  EXPECT_NEAR(data[0], 5.0, VALUE_TOL);
  EXPECT_NEAR(data[1], 6.0, VALUE_TOL);
  EXPECT_NEAR(data[2], 1.0, VALUE_TOL);
  EXPECT_EQ(model.compute_calls, 1);
}

TEST(tire_model, EvaluateRaw_Unchecked_IgnoresLengthArguments) {
  const stub_model model(unchecked_options());
  const std::array<double, 2> input{2.0, 3.0};
  std::array<double, 3> data{};
  EXPECT_NO_THROW(model.evaluate(input.data(), 0, data.data(), -5));
  EXPECT_NEAR(data[0], 5.0, VALUE_TOL);
  EXPECT_NEAR(data[1], 6.0, VALUE_TOL);
}

TEST(tire_model, EvaluateBatchRef_Unchecked_RowsByOutputsCallsComputeBatchOnce) {
  const stub_model model(unchecked_options());
  Eigen::MatrixXd inputs(3, 2);
  inputs << 1.0, 2.0, 3.0, 4.0, 5.0, 6.0;
  const Eigen::MatrixXd outputs = model.evaluate_batch(inputs);
  ASSERT_EQ(outputs.rows(), 3);
  ASSERT_EQ(outputs.cols(), 3);
  EXPECT_NEAR(outputs(0, 0), 3.0, VALUE_TOL);
  EXPECT_NEAR(outputs(1, 0), 7.0, VALUE_TOL);
  EXPECT_NEAR(outputs(2, 0), 11.0, VALUE_TOL);
  EXPECT_NEAR(outputs(0, 1), 2.0, VALUE_TOL);
  EXPECT_NEAR(outputs(1, 1), 12.0, VALUE_TOL);
  EXPECT_NEAR(outputs(2, 1), 30.0, VALUE_TOL);
  EXPECT_NEAR(outputs(2, 2), 1.0, VALUE_TOL);
  EXPECT_EQ(model.batch_calls, 1);
  EXPECT_EQ(model.compute_calls, 0);
}

TEST(tire_model, EvaluateBatchRaw_Unchecked_ColumnMajorBuffers) {
  const stub_model model(unchecked_options());
  // Column-major 3 x 2: column A = (1, 3, 5), column B = (2, 4, 6).
  const std::array<double, 6> inputs{1.0, 3.0, 5.0, 2.0, 4.0, 6.0};
  std::array<double, 9> data{};
  model.evaluate_batch(inputs.data(), 3, data.data(), 9);
  // Column-major 3 x 3: X = (3, 7, 11), Y = (2, 12, 30), Z = (1, 1, 1).
  const std::array<double, 9> expected{3.0, 7.0, 11.0, 2.0, 12.0, 30.0, 1.0, 1.0, 1.0};
  const Eigen::Map<const Eigen::ArrayXd> actual_values(data.data(), 9);
  const Eigen::Map<const Eigen::ArrayXd> expected_values(expected.data(), 9);
  EXPECT_LE((actual_values - expected_values).abs().maxCoeff(), VALUE_TOL);
  EXPECT_EQ(model.batch_calls, 1);
}

TEST(tire_model, EvaluateBatch_ZeroRows_RefAndRawWithNullBuffers_Succeed) {
  const stub_model model(unchecked_options());
  const Eigen::MatrixXd empty_inputs(0, 2);
  const Eigen::MatrixXd outputs = model.evaluate_batch(empty_inputs);
  EXPECT_EQ(outputs.rows(), 0);
  EXPECT_EQ(outputs.cols(), 3);
  EXPECT_NO_THROW(model.evaluate_batch(nullptr, 0, nullptr, 0));
}

TEST(tire_model, Unchecked_InvalidInput_NotValidated) {
  const stub_model model(unchecked_options());
  Eigen::VectorXd output;
  EXPECT_NO_THROW(output = model.evaluate(point_of(-1.0, 3.0)));
  ASSERT_EQ(output.size(), 3);
  EXPECT_NEAR(output(0), 2.0, VALUE_TOL);
  EXPECT_NEAR(output(1), -3.0, VALUE_TOL);
}

TEST(tire_model, Unchecked_NanOutput_ReturnedWithoutThrow) {
  const stub_model model(unchecked_options(), true);
  Eigen::VectorXd output;
  EXPECT_NO_THROW(output = model.evaluate(point_of(2.0, 3.0)));
  ASSERT_EQ(output.size(), 3);
  EXPECT_TRUE(std::isnan(output(2)));
}

TEST(tire_model, EvaluateRef_Checked_ValidInput_Succeeds) {
  const stub_model model(checked_options());
  Eigen::VectorXd output;
  EXPECT_NO_THROW(output = model.evaluate(point_of(2.0, 3.0)));
  ASSERT_EQ(output.size(), 3);
  EXPECT_NEAR(output(0), 5.0, VALUE_TOL);
  EXPECT_NEAR(output(1), 6.0, VALUE_TOL);
}

TEST(tire_model, EvaluateRef_Checked_WrongSize_ThrowsInvalidArgumentNoRow) {
  const stub_model model(checked_options());
  Eigen::VectorXd input(3);
  input << 1.0, 2.0, 3.0;
  expect_invalid_argument([&] { (void)model.evaluate(input); });
  EXPECT_EQ(model.compute_calls, 0);
}

TEST(tire_model, EvaluateRef_Checked_NanInput_ThrowsInvalidInputKeyB) {
  const stub_model model(checked_options());
  expect_failure([&] { (void)model.evaluate(point_of(2.0, QUIET_NAN)); }, mf::error_code::INVALID_INPUT, "B",
                 std::nullopt);
  EXPECT_EQ(model.compute_calls, 0);
}

TEST(tire_model, EvaluateRef_Checked_OutOfDomain_ThrowsInvalidInputKeyA) {
  const stub_model model(checked_options());
  const auto out_of_domain = [&] { (void)model.evaluate(point_of(-1.0, 3.0)); };
  expect_failure(out_of_domain, mf::error_code::INVALID_INPUT, "A", std::nullopt);
}

TEST(tire_model, EvaluateRef_Checked_NanOutput_ThrowsNumericalKeyZ) {
  const stub_model model(checked_options(), true);
  expect_failure([&] { (void)model.evaluate(point_of(2.0, 3.0)); }, mf::error_code::NUMERICAL, "Z", std::nullopt);
  EXPECT_EQ(model.compute_calls, 1);
}

TEST(tire_model, EvaluateRaw_Checked_NullInput_Throws) {
  const stub_model model(checked_options());
  std::array<double, 3> data{};
  expect_invalid_argument([&] { model.evaluate(nullptr, 2, data.data(), 3); });
}

TEST(tire_model, EvaluateRaw_Checked_NullData_Throws) {
  const stub_model model(checked_options());
  const std::array<double, 2> input{2.0, 3.0};
  expect_invalid_argument([&] { model.evaluate(input.data(), 2, nullptr, 3); });
}

TEST(tire_model, EvaluateRaw_Checked_WrongInputLen_Throws) {
  const stub_model model(checked_options());
  const std::array<double, 2> input{2.0, 3.0};
  std::array<double, 3> data{};
  expect_invalid_argument([&] { model.evaluate(input.data(), 1, data.data(), 3); });
}

TEST(tire_model, EvaluateRaw_Checked_WrongLen_Throws) {
  const stub_model model(checked_options());
  const std::array<double, 2> input{2.0, 3.0};
  std::array<double, 3> data{};
  expect_invalid_argument([&] { model.evaluate(input.data(), 2, data.data(), 2); });
}

TEST(tire_model, EvaluateRaw_Checked_Valid_Succeeds) {
  const stub_model model(checked_options());
  const std::array<double, 2> input{2.0, 3.0};
  std::array<double, 3> data{};
  EXPECT_NO_THROW(model.evaluate(input.data(), 2, data.data(), 3));
  EXPECT_NEAR(data[0], 5.0, VALUE_TOL);
  EXPECT_NEAR(data[1], 6.0, VALUE_TOL);
}

TEST(tire_model, EvaluateBatchRef_Checked_BadRow_ThrowsWithRowIndex) {
  const stub_model model(checked_options());
  Eigen::MatrixXd inputs = ramp_inputs();
  inputs(2, 0) = -1.0;
  expect_failure([&] { (void)model.evaluate_batch(inputs); }, mf::error_code::INVALID_INPUT, "A", 2U);
  EXPECT_EQ(model.batch_calls, 0);
}

TEST(tire_model, EvaluateBatchRef_Checked_NanOutput_ThrowsNumericalWithRowZero) {
  const stub_model model(checked_options(), true);
  expect_failure([&] { (void)model.evaluate_batch(ramp_inputs()); }, mf::error_code::NUMERICAL, "Z", 0U);
}

TEST(tire_model, EvaluateBatchRef_Checked_Valid_Succeeds) {
  const stub_model model(checked_options());
  Eigen::MatrixXd outputs;
  EXPECT_NO_THROW(outputs = model.evaluate_batch(ramp_inputs()));
  ASSERT_EQ(outputs.rows(), 5);
  ASSERT_EQ(outputs.cols(), 3);
  EXPECT_NEAR(outputs(4, 0), 9.0, VALUE_TOL);
  EXPECT_NEAR(outputs(4, 1), 20.0, VALUE_TOL);
}

TEST(tire_model, EvaluateBatchRaw_Checked_NegativeRows_Throws) {
  const stub_model model(checked_options());
  const std::array<double, 2> inputs{};
  std::array<double, 3> data{};
  expect_invalid_argument([&] { model.evaluate_batch(inputs.data(), -1, data.data(), 3); });
  EXPECT_EQ(model.batch_calls, 0);
}

TEST(tire_model, EvaluateBatchRaw_Checked_NullInputs_Throws) {
  const stub_model model(checked_options());
  std::array<double, 3> data{};
  expect_invalid_argument([&] { model.evaluate_batch(nullptr, 1, data.data(), 3); });
}

TEST(tire_model, EvaluateBatchRaw_Checked_NullData_Throws) {
  const stub_model model(checked_options());
  const std::array<double, 2> inputs{1.0, 2.0};
  expect_invalid_argument([&] { model.evaluate_batch(inputs.data(), 1, nullptr, 3); });
}

TEST(tire_model, EvaluateBatchRaw_Checked_WrongLen_Throws) {
  const stub_model model(checked_options());
  const std::array<double, 2> inputs{1.0, 2.0};
  std::array<double, 3> data{};
  expect_invalid_argument([&] { model.evaluate_batch(inputs.data(), 1, data.data(), 2); });
}

TEST(tire_model, EvaluateBatchRaw_Checked_ZeroRowsNullBuffers_Succeeds) {
  const stub_model model(checked_options());
  EXPECT_NO_THROW(model.evaluate_batch(nullptr, 0, nullptr, 0));
}

TEST(tire_model, EvaluateBatchRaw_Checked_Valid_Succeeds) {
  const stub_model model(checked_options());
  const std::array<double, 4> inputs{1.0, 3.0, 2.0, 4.0};
  std::array<double, 6> data{};
  EXPECT_NO_THROW(model.evaluate_batch(inputs.data(), 2, data.data(), 6));
  EXPECT_NEAR(data[0], 3.0, VALUE_TOL);
  EXPECT_NEAR(data[1], 7.0, VALUE_TOL);
  EXPECT_NEAR(data[2], 2.0, VALUE_TOL);
  EXPECT_NEAR(data[3], 12.0, VALUE_TOL);
}

TEST(tire_model, Accessors_ReturnLimitsMetadataOptions) {
  Eigen::VectorXd limits(3);
  limits << 1.0, 2.0, 3.0;
  const mf::model_metadata metadata{.fittyp = 5, .use_mode = 4, .side = mf::tyre_side::RIGHT};
  const mf::model_options options{.mode = mf::slip_mode::UNCOMBINED, .throw_if_model_failure = true};
  const stub_model model(options, false, limits, metadata);
  ASSERT_EQ(model.limits().size(), 3);
  EXPECT_NEAR(model.limits()(1), 2.0, VALUE_TOL);
  EXPECT_EQ(model.metadata().fittyp, 5);
  EXPECT_EQ(model.metadata().use_mode, 4);
  EXPECT_EQ(model.metadata().side, mf::tyre_side::RIGHT);
  EXPECT_EQ(model.options().mode, mf::slip_mode::UNCOMBINED);
  EXPECT_TRUE(model.options().throw_if_model_failure);
}

TEST(tire_model, Destruction_ThroughBasePointer) {
  std::unique_ptr<mf::tire_model> model = std::make_unique<stub_model>(unchecked_options());
  EXPECT_EQ(model->name(), "stub");
  EXPECT_EQ(model->input_length(), 2);
  EXPECT_EQ(model->output_length(), 3);
  EXPECT_EQ(model->layout().inputs.size(), 2U);
  model.reset();
  EXPECT_FALSE(model);
}

TEST(tire_model, FindCoordinateSystem_StubWithoutFz_AllMissingChannel) {
  const stub_model model(unchecked_options());
  const mf::coordinate_identification result = model.find_coordinate_system();
  ASSERT_EQ(result.readings.size(), 29U);
  for (const mf::indicator_reading &reading : result.readings) {
    EXPECT_EQ(reading.status, mf::reading_status::MISSING_CHANNEL) << reading.definition.name;
  }
  EXPECT_EQ(result.classification, mf::system_classification::ISO);
  EXPECT_EQ(result.vertical.basis, mf::decision_basis::INSUFFICIENT_EVIDENCE);
  EXPECT_EQ(result.lateral.basis, mf::decision_basis::INSUFFICIENT_EVIDENCE);
  EXPECT_EQ(result.warnings.size(), 2U);
  ASSERT_EQ(result.reference_input.size(), 2);
  EXPECT_NEAR(result.reference_input(0), 0.5, VALUE_TOL);
  EXPECT_NEAR(result.reference_input(1), 0.5, VALUE_TOL);
  // One unchecked batch call, even with no probe rows.
  EXPECT_EQ(model.batch_calls, 1);
}

TEST(tire_model, Transformed_PassesPlanAndGenericallyTransformedLimits) {
  Eigen::VectorXd limits(3);
  limits << -10.0, 20.0, 5.0;
  const mf::model_metadata metadata{.fittyp = 5, .use_mode = 4, .side = mf::tyre_side::RIGHT};
  const stub_model model(unchecked_options(), false, limits, metadata, true);

  const std::unique_ptr<mf::tire_model> vertical = model.transformed(mf::sign_transforms{.vertical = true});
  const auto *const vertical_stub = dynamic_cast<const stub_model *>(vertical.get());
  ASSERT_TRUE(vertical_stub != nullptr);
  EXPECT_TRUE(vertical_stub->rebuilt);
  EXPECT_TRUE(vertical_stub->rebuilt_plan.vertical);
  EXPECT_FALSE(vertical_stub->rebuilt_plan.lateral);
  ASSERT_EQ(vertical_stub->limits().size(), 3);
  EXPECT_NEAR(vertical_stub->limits()(0), -20.0, VALUE_TOL);
  EXPECT_NEAR(vertical_stub->limits()(1), 10.0, VALUE_TOL);
  EXPECT_NEAR(vertical_stub->limits()(2), 5.0, VALUE_TOL);
  EXPECT_EQ(vertical_stub->metadata().fittyp, 5);

  const std::unique_ptr<mf::tire_model> lateral = model.transformed(mf::sign_transforms{.lateral = true});
  const auto *const lateral_stub = dynamic_cast<const stub_model *>(lateral.get());
  ASSERT_TRUE(lateral_stub != nullptr);
  EXPECT_TRUE(lateral_stub->rebuilt_plan.lateral);
  EXPECT_FALSE(lateral_stub->rebuilt_plan.vertical);
  EXPECT_NEAR(lateral_stub->limits()(0), -10.0, VALUE_TOL);
  EXPECT_NEAR(lateral_stub->limits()(1), 20.0, VALUE_TOL);

  const std::unique_ptr<mf::tire_model> identity = model.transformed(mf::sign_transforms{});
  const auto *const identity_stub = dynamic_cast<const stub_model *>(identity.get());
  ASSERT_TRUE(identity_stub != nullptr);
  EXPECT_FALSE(identity_stub->rebuilt_plan.vertical);
  EXPECT_FALSE(identity_stub->rebuilt_plan.lateral);
  EXPECT_NEAR(identity_stub->limits()(0), -10.0, VALUE_TOL);
  EXPECT_NEAR(identity_stub->limits()(1), 20.0, VALUE_TOL);

  // The source model is untouched.
  EXPECT_FALSE(model.rebuilt);
  EXPECT_NEAR(model.limits()(0), -10.0, VALUE_TOL);
}

TEST(tire_model, LoadedSide_PositiveReferenceFz_Iso) {
  const load_stub model(4000.0);
  EXPECT_EQ(model.loaded_side(), mf::coordinate_system::ISO);
}

TEST(tire_model, LoadedSide_NegativeReferenceFz_Sae) {
  const load_stub model(-4000.0);
  EXPECT_EQ(model.loaded_side(), mf::coordinate_system::SAE);
}

TEST(tire_model, LoadedSide_ZeroReferenceFz_Iso) {
  const load_stub model(0.0);
  EXPECT_EQ(model.loaded_side(), mf::coordinate_system::ISO);
}

TEST(tire_model, LoadedSide_NoFzChannel_Iso) {
  const stub_model model(unchecked_options());
  EXPECT_EQ(model.loaded_side(), mf::coordinate_system::ISO);
}

TEST(tire_model, LoadedSide_NoFzChannelNegativeReference_Iso) {
  const stub_model model(unchecked_options(), false, Eigen::VectorXd(), mf::model_metadata{}, false, -0.5);
  EXPECT_EQ(model.loaded_side(), mf::coordinate_system::ISO);
}

TEST(tire_model, ConvertCoordinateSystem_StubWithoutFz_IsoTargetEmptyPlanSaeTargetVertical) {
  const stub_model model(unchecked_options());

  const mf::model_conversion to_iso = model.convert_coordinate_system(mf::coordinate_system::ISO);
  ASSERT_TRUE(to_iso.model != nullptr);
  EXPECT_FALSE(to_iso.applied.vertical);
  EXPECT_FALSE(to_iso.applied.lateral);
  EXPECT_EQ(to_iso.identification.classification, mf::system_classification::ISO);
  EXPECT_EQ(to_iso.identification.readings.size(), 29U);
  const auto *const iso_stub = dynamic_cast<const stub_model *>(to_iso.model.get());
  ASSERT_TRUE(iso_stub != nullptr);
  EXPECT_TRUE(iso_stub->rebuilt);
  EXPECT_FALSE(iso_stub->rebuilt_plan.vertical);
  EXPECT_FALSE(iso_stub->rebuilt_plan.lateral);

  const mf::model_conversion to_sae = model.convert_coordinate_system(mf::coordinate_system::SAE);
  ASSERT_TRUE(to_sae.model != nullptr);
  EXPECT_TRUE(to_sae.applied.vertical);
  EXPECT_FALSE(to_sae.applied.lateral);
  const auto *const sae_stub = dynamic_cast<const stub_model *>(to_sae.model.get());
  ASSERT_TRUE(sae_stub != nullptr);
  EXPECT_TRUE(sae_stub->rebuilt_plan.vertical);
  EXPECT_FALSE(sae_stub->rebuilt_plan.lateral);
}
