#include <Eigen/Core>
#include <array>
#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/model_layout.hpp>
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

  /// X = A + B, Y = A * B, Z = 1 (or NaN when `emit_nan`), with call counters.
  class stub_model final : public mf::tire_model {
  public:
    explicit stub_model(const mf::model_options &options, bool emit_nan = false,
                        const Eigen::VectorXd &limits = Eigen::VectorXd(),
                        const mf::model_metadata &metadata = mf::model_metadata{})
        : tire_model(2, 3, limits, metadata, options), emit_nan_(emit_nan) {}

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
      return STUB_LAYOUT;
    }

    mutable int compute_calls = 0;
    mutable int batch_calls = 0;

  private:
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
