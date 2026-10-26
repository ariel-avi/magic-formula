#include "../support/fixtures.hpp"

#include <Eigen/Core>
#include <array>
#include <cstddef>
#include <expected>
#include <gtest/gtest.h>
#include <limits>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf52_model.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/model_checks.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/pac2002_model.hpp>
#include <magic_formula/sign_transform.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {
  constexpr double QUIET_NAN = std::numeric_limits<double>::quiet_NaN();
  constexpr double POSITIVE_INF = std::numeric_limits<double>::infinity();

  /// Inputs A (>= 0) and B; outputs X, Y and Z (NaN when `emit_nan`). A local copy of the stub of tire_model_test.
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

  class stub_model final : public mf::tire_model {
  public:
    explicit stub_model(bool emit_nan)
        : tire_model(2, 3, Eigen::VectorXd(), mf::model_metadata{}, {}), emit_nan_(emit_nan) {}

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
    [[nodiscard]] Eigen::VectorXd reference_input() const override {
      return Eigen::VectorXd::Zero(input_length());
    }

  private:
    [[nodiscard]] std::unique_ptr<mf::tire_model> rebuild(const mf::sign_transforms & /*transforms*/,
                                                          const Eigen::VectorXd & /*limits*/) const override {
      return std::make_unique<stub_model>(emit_nan_);
    }

    void compute(const Eigen::Ref<const Eigen::VectorXd> &input,
                 Eigen::Ref<Eigen::VectorXd> output) const noexcept override {
      output(0) = input(0) + input(1);
      output(1) = input(0) * input(1);
      output(2) = emit_nan_ ? QUIET_NAN : 1.0;
    }

    void compute_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                       Eigen::Ref<Eigen::MatrixXd> outputs) const noexcept override {
      outputs.col(0) = inputs.col(0) + inputs.col(1);
      outputs.col(1) = (inputs.col(0).array() * inputs.col(1).array()).matrix();
      outputs.col(2).setConstant(emit_nan_ ? QUIET_NAN : 1.0);
    }

    bool emit_nan_;
  };

  mf::pac2002_model make_pac2002() {
    return std::move(mf::pac2002_model::create(mf::test::fixture_s())).value();
  }

  mf::mf52_model make_mf52() {
    return std::move(mf::mf52_model::create(mf::test::fixture_s52())).value();
  }

  /// One nominal point: FZ 4000 N, SA 0.05 rad, SR 0.02, IA 0, VX 16.6 m/s.
  Eigen::VectorXd nominal_input() {
    Eigen::VectorXd input(5);
    input << 4000.0, 0.05, 0.02, 0.0, 16.6;
    return input;
  }

  /// `rows` nominal points, one per row.
  Eigen::MatrixXd nominal_inputs(Eigen::Index rows) {
    Eigen::MatrixXd inputs(rows, 5);
    for (Eigen::Index r = 0; r < rows; ++r) {
      inputs.row(r) = nominal_input().transpose();
    }
    return inputs;
  }

  /// `rows` x 10 outputs of zeros (the 10 output channels).
  Eigen::MatrixXd finite_outputs(Eigen::Index rows) {
    return Eigen::MatrixXd::Zero(rows, 10);
  }

  bool contains(const std::string &text, std::string_view part) {
    return text.find(part) != std::string::npos;
  }
} // namespace

TEST(validate_input, WrongSize_InvalidArgument) {
  const mf::pac2002_model model = make_pac2002();
  const Eigen::VectorXd input = Eigen::VectorXd::Zero(3);
  const auto result = mf::validate_input(model, input);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_ARGUMENT);
  EXPECT_TRUE(contains(result.error().message, "input has 3 values, the PAC2002 model expects 5"));
}

TEST(validate_input, NonFinite_FirstChannelInIndexOrder) {
  const mf::pac2002_model model = make_pac2002();
  Eigen::VectorXd input = nominal_input();
  input(mf::mf5x::SA) = QUIET_NAN;
  input(mf::mf5x::IA) = QUIET_NAN;
  const auto result = mf::validate_input(model, input);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "SA");
  EXPECT_EQ(result.error().message, "SA is not finite (nan)");
}

TEST(validate_input, InfiniteValue_NotFinite) {
  const mf::pac2002_model model = make_pac2002();
  Eigen::VectorXd input = nominal_input();
  input(mf::mf5x::VX) = POSITIVE_INF;
  const auto result = mf::validate_input(model, input);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
  EXPECT_EQ(result.error().message, "VX is not finite (inf)");
}

TEST(validate_input, Pac2002_NegativeVx_DomainMessage) {
  const mf::pac2002_model model = make_pac2002();
  Eigen::VectorXd input = nominal_input();
  input(mf::mf5x::VX) = -1.0;
  const auto result = mf::validate_input(model, input);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
  EXPECT_EQ(result.error().message, "VX = -1 m/s is outside the PAC2002 domain (VX >= 0)");
}

TEST(validate_input, Pac2002_ZeroAndNegativeZeroVx_Accepted) {
  const mf::pac2002_model model = make_pac2002();
  Eigen::VectorXd input = nominal_input();
  input(mf::mf5x::VX) = 0.0;
  EXPECT_TRUE(mf::validate_input(model, input).has_value());
  input(mf::mf5x::VX) = -0.0;
  EXPECT_TRUE(mf::validate_input(model, input).has_value());
}

TEST(validate_input, Mf52_ZeroVx_DomainMessage) {
  const mf::mf52_model model = make_mf52();
  Eigen::VectorXd input = nominal_input();
  input(mf::mf5x::VX) = 0.0;
  const auto result = mf::validate_input(model, input);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
  EXPECT_EQ(result.error().message, "VX = 0 m/s is outside the MF 5.2 domain (VX > 0)");
}

TEST(validate_input, Mf52_NegativeZeroVx_Rejected) {
  const mf::mf52_model model = make_mf52();
  Eigen::VectorXd input = nominal_input();
  input(mf::mf5x::VX) = -0.0;
  const auto result = mf::validate_input(model, input);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().where.key, "VX");
}

TEST(validate_input, Mf52_PositiveVx_Accepted) {
  const mf::mf52_model model = make_mf52();
  EXPECT_TRUE(mf::validate_input(model, nominal_input()).has_value());
}

TEST(validate_input, Valid_Succeeds) {
  const mf::pac2002_model model = make_pac2002();
  EXPECT_TRUE(mf::validate_input(model, nominal_input()).has_value());
}

TEST(validate_inputs, WrongColumns_InvalidArgumentIndexZero) {
  const mf::pac2002_model model = make_pac2002();
  const Eigen::MatrixXd inputs = Eigen::MatrixXd::Zero(3, 4);
  const auto result = mf::validate_inputs(model, inputs);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().index, 0U);
  EXPECT_EQ(result.error().cause.code, mf::error_code::INVALID_ARGUMENT);
  EXPECT_TRUE(contains(result.error().cause.message, "input matrix has 4 columns, the PAC2002 model expects 5"));
}

TEST(validate_inputs, CheckInputShape_MatchingColumns_Succeeds) {
  const mf::pac2002_model model = make_pac2002();
  EXPECT_TRUE(mf::check_input_shape(model, Eigen::MatrixXd::Zero(3, 5)).has_value());
  EXPECT_TRUE(mf::check_input_shape(model, Eigen::MatrixXd(0, 5)).has_value());
}

TEST(validate_inputs, CheckInputShape_WrongColumns_InvalidArgumentAtRowZero) {
  const stub_model model(false);
  const auto result = mf::check_input_shape(model, Eigen::MatrixXd::Zero(3, 4));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().index, 0U);
  EXPECT_EQ(result.error().cause.code, mf::error_code::INVALID_ARGUMENT);
  EXPECT_EQ(result.error().cause.message, "input matrix has 4 columns, the stub model expects 2");
}

TEST(validate_inputs, ZeroRows_Succeeds) {
  const mf::pac2002_model model = make_pac2002();
  const Eigen::MatrixXd inputs(0, 5);
  EXPECT_TRUE(mf::validate_inputs(model, inputs).has_value());
}

TEST(validate_inputs, SmallestFailingRowWins) {
  const mf::pac2002_model model = make_pac2002();
  Eigen::MatrixXd inputs = nominal_inputs(6);
  inputs(1, mf::mf5x::VX) = -1.0;
  inputs(3, mf::mf5x::FZ) = QUIET_NAN;
  const auto result = mf::validate_inputs(model, inputs);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().index, 1U);
  EXPECT_EQ(result.error().cause.code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().cause.where.key, "VX");
}

TEST(validate_inputs, SameRowTwoChannels_FirstChannelWins) {
  const mf::pac2002_model model = make_pac2002();
  Eigen::MatrixXd inputs = nominal_inputs(4);
  inputs(2, mf::mf5x::SA) = QUIET_NAN;
  inputs(2, mf::mf5x::IA) = QUIET_NAN;
  const auto result = mf::validate_inputs(model, inputs);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().index, 2U);
  EXPECT_EQ(result.error().cause.where.key, "SA");
}

TEST(validate_inputs, LaterColumnSmallerRow_Replaces) {
  const mf::pac2002_model model = make_pac2002();
  Eigen::MatrixXd inputs = nominal_inputs(6);
  inputs(4, mf::mf5x::FZ) = QUIET_NAN;
  inputs(2, mf::mf5x::SA) = QUIET_NAN;
  const auto result = mf::validate_inputs(model, inputs);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().index, 2U);
  EXPECT_EQ(result.error().cause.where.key, "SA");
}

TEST(validate_inputs, AllValid_Succeeds) {
  const mf::pac2002_model model = make_pac2002();
  EXPECT_TRUE(mf::validate_inputs(model, nominal_inputs(5)).has_value());
}

TEST(check_output, WrongSize_InvalidArgument) {
  const mf::pac2002_model model = make_pac2002();
  const Eigen::VectorXd output = Eigen::VectorXd::Zero(3);
  const auto result = mf::check_output(model, output);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_ARGUMENT);
  EXPECT_TRUE(contains(result.error().message, "output has 3 values, the PAC2002 model produces 10"));
}

TEST(check_output, NonFinite_NumericalWithName) {
  const mf::pac2002_model model = make_pac2002();
  Eigen::VectorXd output = Eigen::VectorXd::Zero(10);
  output(mf::mf5x::MZ) = QUIET_NAN;
  output(mf::mf5x::MY) = POSITIVE_INF;
  const auto result = mf::check_output(model, output);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::NUMERICAL);
  EXPECT_EQ(result.error().where.key, "MZ");
  EXPECT_EQ(result.error().message, "non-finite MZ output (nan)");
}

TEST(check_output, Finite_Succeeds) {
  const mf::pac2002_model model = make_pac2002();
  EXPECT_TRUE(mf::check_output(model, Eigen::VectorXd::Zero(10)).has_value());
}

TEST(check_outputs, WrongColumns_InvalidArgument) {
  const mf::pac2002_model model = make_pac2002();
  const Eigen::MatrixXd outputs = Eigen::MatrixXd::Zero(3, 4);
  const auto result = mf::check_outputs(model, outputs);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().index, 0U);
  EXPECT_EQ(result.error().cause.code, mf::error_code::INVALID_ARGUMENT);
  EXPECT_TRUE(contains(result.error().cause.message, "output matrix has 4 columns, the PAC2002 model produces 10"));
}

TEST(check_outputs, SmallestRowFirstChannel_Numerical) {
  const mf::pac2002_model model = make_pac2002();
  Eigen::MatrixXd outputs = finite_outputs(5);
  outputs(3, mf::mf5x::FY) = QUIET_NAN;
  outputs(1, mf::mf5x::MX) = QUIET_NAN;
  outputs(1, mf::mf5x::MY) = POSITIVE_INF;
  const auto result = mf::check_outputs(model, outputs);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().index, 1U);
  EXPECT_EQ(result.error().cause.code, mf::error_code::NUMERICAL);
  EXPECT_EQ(result.error().cause.where.key, "MX");
}

TEST(check_outputs, ZeroRows_Succeeds) {
  const mf::pac2002_model model = make_pac2002();
  EXPECT_TRUE(mf::check_outputs(model, finite_outputs(0)).has_value());
}

TEST(check_outputs, Finite_Succeeds) {
  const mf::pac2002_model model = make_pac2002();
  EXPECT_TRUE(mf::check_outputs(model, finite_outputs(4)).has_value());
}

TEST(check_buffers, NullInput) {
  const mf::pac2002_model model = make_pac2002();
  const std::array<double, 10> data{};
  const auto result = mf::check_buffers(model, nullptr, 5, data.data(), 10);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_ARGUMENT);
  EXPECT_TRUE(contains(result.error().message, "input buffer is null"));
}

TEST(check_buffers, NullData) {
  const mf::pac2002_model model = make_pac2002();
  const std::array<double, 5> input{};
  const auto result = mf::check_buffers(model, input.data(), 5, nullptr, 10);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_ARGUMENT);
  EXPECT_TRUE(contains(result.error().message, "output buffer is null"));
}

TEST(check_buffers, WrongInputLen) {
  const mf::pac2002_model model = make_pac2002();
  const std::array<double, 5> input{};
  const std::array<double, 10> data{};
  const auto result = mf::check_buffers(model, input.data(), 4, data.data(), 10);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_ARGUMENT);
  EXPECT_TRUE(contains(result.error().message, "input length 4 != 5"));
}

TEST(check_buffers, WrongLen) {
  const mf::pac2002_model model = make_pac2002();
  const std::array<double, 5> input{};
  const std::array<double, 10> data{};
  const auto result = mf::check_buffers(model, input.data(), 5, data.data(), 9);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_ARGUMENT);
  EXPECT_TRUE(contains(result.error().message, "output length 9 != 10"));
}

TEST(check_buffers, Valid_Succeeds) {
  const mf::pac2002_model model = make_pac2002();
  const std::array<double, 5> input{};
  const std::array<double, 10> data{};
  EXPECT_TRUE(mf::check_buffers(model, input.data(), 5, data.data(), 10).has_value());
}

TEST(check_batch_buffers, NegativeRows) {
  const mf::pac2002_model model = make_pac2002();
  const std::array<double, 5> inputs{};
  const std::array<double, 10> data{};
  const auto result = mf::check_batch_buffers(model, inputs.data(), -1, data.data(), 10);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_ARGUMENT);
  EXPECT_TRUE(contains(result.error().message, "row count -1 is negative"));
}

TEST(check_batch_buffers, PositiveRowsNullInputs) {
  const mf::pac2002_model model = make_pac2002();
  const std::array<double, 10> data{};
  const auto result = mf::check_batch_buffers(model, nullptr, 1, data.data(), 10);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_ARGUMENT);
  EXPECT_TRUE(contains(result.error().message, "input buffer is null"));
}

TEST(check_batch_buffers, PositiveRowsNullData) {
  const mf::pac2002_model model = make_pac2002();
  const std::array<double, 5> inputs{};
  const auto result = mf::check_batch_buffers(model, inputs.data(), 1, nullptr, 10);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_ARGUMENT);
  EXPECT_TRUE(contains(result.error().message, "output buffer is null"));
}

TEST(check_batch_buffers, WrongLen) {
  const mf::pac2002_model model = make_pac2002();
  const std::array<double, 5> inputs{};
  const std::array<double, 10> data{};
  const auto result = mf::check_batch_buffers(model, inputs.data(), 1, data.data(), 9);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_ARGUMENT);
  EXPECT_TRUE(contains(result.error().message, "output length 9 != 1 rows * 10 outputs"));
}

TEST(check_batch_buffers, ZeroRowsNullPointersZeroLen_Succeeds) {
  const mf::pac2002_model model = make_pac2002();
  EXPECT_TRUE(mf::check_batch_buffers(model, nullptr, 0, nullptr, 0).has_value());
}

TEST(check_batch_buffers, ZeroRowsNonZeroLen_Fails) {
  const mf::pac2002_model model = make_pac2002();
  const auto result = mf::check_batch_buffers(model, nullptr, 0, nullptr, 10);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, mf::error_code::INVALID_ARGUMENT);
}

TEST(check_batch_buffers, Valid_Succeeds) {
  const mf::pac2002_model model = make_pac2002();
  const std::array<double, 10> inputs{};
  const std::array<double, 20> data{};
  EXPECT_TRUE(mf::check_batch_buffers(model, inputs.data(), 2, data.data(), 20).has_value());
}

TEST(checked_evaluate_batch, Valid_ReturnsSameAsEvaluateBatch) {
  const mf::pac2002_model model = make_pac2002();
  Eigen::MatrixXd inputs = nominal_inputs(4);
  inputs(1, mf::mf5x::SA) = -0.1;
  inputs(2, mf::mf5x::SR) = 0.1;
  inputs(3, mf::mf5x::FZ) = 3000.0;
  const auto result = mf::checked_evaluate_batch(model, inputs);
  ASSERT_TRUE(result.has_value());
  const Eigen::MatrixXd expected = model.evaluate_batch(inputs);
  ASSERT_EQ(result->rows(), expected.rows());
  ASSERT_EQ(result->cols(), expected.cols());
  for (Eigen::Index c = 0; c < expected.cols(); ++c) {
    for (Eigen::Index r = 0; r < expected.rows(); ++r) {
      // The same code runs twice, so the results are bit-equal.
      EXPECT_EQ(mf::test::bits((*result)(r, c)), mf::test::bits(expected(r, c))) << "row " << r << " col " << c;
    }
  }
}

TEST(checked_evaluate_batch, InvalidInput_ReturnsInputError) {
  const mf::pac2002_model model = make_pac2002();
  Eigen::MatrixXd inputs = nominal_inputs(3);
  inputs(1, mf::mf5x::VX) = -1.0;
  const auto result = mf::checked_evaluate_batch(model, inputs);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().index, 1U);
  EXPECT_EQ(result.error().cause.code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().cause.where.key, "VX");
}

TEST(checked_evaluate_batch, InputErrorReportedBeforeNumerical) {
  const stub_model model(true);
  Eigen::MatrixXd inputs(4, 2);
  inputs << 1.0, 2.0, 3.0, 4.0, -1.0, 5.0, 6.0, 7.0;
  const auto result = mf::checked_evaluate_batch(model, inputs);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().index, 2U);
  EXPECT_EQ(result.error().cause.code, mf::error_code::INVALID_INPUT);
  EXPECT_EQ(result.error().cause.where.key, "A");
}

TEST(checked_evaluate_batch, NonFiniteOutput_ReturnsNumericalWithRow) {
  const mf::pac2002_model model = make_pac2002();
  Eigen::MatrixXd inputs = nominal_inputs(3);
  inputs(1, mf::mf5x::FZ) = 1e308;
  const auto result = mf::checked_evaluate_batch(model, inputs);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().index, 1U);
  EXPECT_EQ(result.error().cause.code, mf::error_code::NUMERICAL);
}
