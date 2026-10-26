#pragma once

#include <Eigen/Core>
#include <expected>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/model_layout.hpp>

namespace mf {
  class tire_model;

  // The free check functions of the evaluation interface. A model built with `model_options::throw_if_model_failure`
  // runs them inside `evaluate` and throws `model_failure`; any caller can run them explicitly on a model built
  // without the flag. Every error names the offending channel in `where.key`; file and line are empty. The batch
  // forms report the smallest failing row (the first channel in index order on a tie) in `batch_error::index`.

  /// Checks one input vector: the size (INVALID_ARGUMENT), then every channel in index order: finite first, then
  /// inside the model domain of its `input_channel` (INVALID_INPUT, e.g. "VX = -1 m/s is outside the PAC2002 domain
  /// (VX >= 0)").
  [[nodiscard]] std::expected<void, error> validate_input(const tire_model &model,
                                                          const Eigen::Ref<const Eigen::VectorXd> &input);

  /// Checks only the column count of a rows x input_length() matrix: INVALID_ARGUMENT at row 0 with the message of
  /// `validate_inputs` ("input matrix has <c> columns, the <model> model expects <n>"). `validate_inputs` runs it
  /// first.
  [[nodiscard]] std::expected<void, batch_error> check_input_shape(const tire_model &model,
                                                                   const Eigen::Ref<const Eigen::MatrixXd> &inputs);

  /// Checks a rows x input_length() matrix like `validate_input`, column by column. A wrong column count is an
  /// INVALID_ARGUMENT at row 0. No allocation on success; zero rows succeed.
  [[nodiscard]] std::expected<void, batch_error> validate_inputs(const tire_model &model,
                                                                 const Eigen::Ref<const Eigen::MatrixXd> &inputs);

  /// Checks one output vector: the size (INVALID_ARGUMENT), then the first non-finite value (NUMERICAL, key = the
  /// output channel name).
  [[nodiscard]] std::expected<void, error> check_output(const tire_model &model,
                                                        const Eigen::Ref<const Eigen::VectorXd> &output);

  /// Checks a rows x output_length() matrix like `check_output`, column by column.
  [[nodiscard]] std::expected<void, batch_error> check_outputs(const tire_model &model,
                                                               const Eigen::Ref<const Eigen::MatrixXd> &outputs);

  /// Checks one characteristics vector: the size (INVALID_ARGUMENT), then the first value, in channel order, that is
  /// neither finite nor a NaN of a channel with `nan_allowed` (NUMERICAL, key = the characteristic channel name).
  [[nodiscard]] std::expected<void, error>
  check_characteristics(const tire_model &model, const Eigen::Ref<const Eigen::VectorXd> &characteristics);

  /// Checks a rows x characteristics_length() matrix like `check_characteristics`, column by column. A wrong column
  /// count is an INVALID_ARGUMENT at row 0. No allocation on success; zero rows succeed.
  [[nodiscard]] std::expected<void, batch_error>
  check_characteristics_batch(const tire_model &model, const Eigen::Ref<const Eigen::MatrixXd> &characteristics);

  /// Checks the raw one-point buffers of `tire_model::evaluate`: a null input, a null output, then the two lengths
  /// (INVALID_ARGUMENT).
  [[nodiscard]] std::expected<void, error> check_buffers(const tire_model &model, const double *input, int input_len,
                                                         const double *data, int len);

  /// Checks the raw column-major buffers of `tire_model::evaluate_batch`: a negative row count, null buffers when
  /// `rows > 0`, then `len == rows*output_length()` (INVALID_ARGUMENT).
  [[nodiscard]] std::expected<void, error> check_batch_buffers(const tire_model &model, const double *inputs, int rows,
                                                               const double *data, int len);

  /// `validate_inputs`, `model.evaluate_batch`, `check_outputs`; returns the first failure. Input errors are
  /// reported before numerical errors. Meant for models built without `throw_if_model_failure`; with the flag set,
  /// `evaluate_batch` throws first.
  [[nodiscard]] std::expected<Eigen::MatrixXd, batch_error>
  checked_evaluate_batch(const tire_model &model, const Eigen::Ref<const Eigen::MatrixXd> &inputs);

  /// `validate_inputs`, `model.evaluate_characteristics_batch`, `check_characteristics_batch`; returns the first
  /// failure. Input errors are reported before numerical errors. Meant for models built without
  /// `throw_if_model_failure`; with the flag set, `evaluate_characteristics_batch` throws first.
  [[nodiscard]] std::expected<Eigen::MatrixXd, batch_error>
  checked_evaluate_characteristics_batch(const tire_model &model, const Eigen::Ref<const Eigen::MatrixXd> &inputs);

  /// The outputs and the characteristics of every row of a batch.
  struct checked_batch {
    /// rows x output_length()
    Eigen::MatrixXd outputs;
    /// rows x characteristics_length()
    Eigen::MatrixXd characteristics;
  };

  /// `checked_evaluate_batch`, then `checked_evaluate_characteristics_batch`; returns the first failure (the outputs
  /// are checked before the characteristics).
  [[nodiscard]] std::expected<checked_batch, batch_error>
  checked_evaluate_batch_with_characteristics(const tire_model &model, const Eigen::Ref<const Eigen::MatrixXd> &inputs);
} // namespace mf
