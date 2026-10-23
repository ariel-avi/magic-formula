#include <Eigen/Core>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/model_checks.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/tire_model.hpp>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace mf {
  namespace {
    /// Location that names only a key. Built through a named local and a move so that the `make_error` call sites
    /// carry no exception-cleanup branches of their own.
    location key_location(std::string_view key) {
      std::string text(key);
      return location{.file = {}, .line = 0, .key = std::move(text)};
    }

    /// An `error` value with the given code, message and key.
    error key_error(error_code code, std::string message, std::string_view key) {
      return make_error(code, std::move(message), key_location(key)).error();
    }

    /// A `batch_error` for `row`, built member by member.
    std::unexpected<batch_error> batch_failure(std::size_t row, error cause) {
      batch_error failure;
      failure.index = row;
      failure.cause = std::move(cause);
      return std::unexpected<batch_error>(std::move(failure));
    }

    /// True when `v` is finite and inside the domain of `channel`.
    bool channel_ok(const input_channel &channel, double v) noexcept {
      if (!std::isfinite(v)) {
        return false;
      }
      return channel.minimum_inclusive ? v >= channel.minimum : v > channel.minimum;
    }

    /// INVALID_INPUT for the value `v` of `channel`, which failed `channel_ok`.
    error channel_error(const tire_model &model, const input_channel &channel, double v) {
      if (!std::isfinite(v)) {
        return key_error(error_code::INVALID_INPUT, std::format("{} is not finite ({})", channel.name, v),
                         channel.name);
      }
      const std::string_view comparison = channel.minimum_inclusive ? ">=" : ">";
      const std::string domain = std::format("{} {} {}", channel.name, comparison, channel.minimum);
      const std::string head = std::format("{} = {} {}", channel.name, v, channel.unit);
      std::string message = std::format("{} is outside the {} domain ({})", head, model.name(), domain);
      return key_error(error_code::INVALID_INPUT, std::move(message), channel.name);
    }

    /// NUMERICAL for the non-finite value `v` of the output `channel`.
    error output_error(const output_channel &channel, double v) {
      std::string message = std::format("non-finite {} output ({})", channel.name, v);
      return key_error(error_code::NUMERICAL, std::move(message), channel.name);
    }

    /// INVALID_ARGUMENT "<what> has <got> <unit>, the <name> model <verb> <want>" for a size mismatch.
    std::unexpected<error> size_mismatch(std::string_view what, Eigen::Index got, std::string_view unit,
                                         std::string_view name, std::string_view verb, Eigen::Index want) {
      std::string message = std::format("{} has {} {}, the {} model {} {}", what, got, unit, name, verb, want);
      return make_error(error_code::INVALID_ARGUMENT, std::move(message));
    }

    /// The failing cell of a matrix scan; `col` is -1 when every cell passed.
    struct failing_cell {
      Eigen::Index row = 0;
      Eigen::Index col = -1;
    };

    /// Column-wise scan for the smallest failing row. Columns are visited in index order and a later column only
    /// replaces the best cell with a strictly smaller row, so a tie keeps the first channel. `ok(col, value)` tells
    /// whether a cell passes. No allocation.
    template <typename Ok_T>
    failing_cell first_failing_cell(const Eigen::Ref<const Eigen::MatrixXd> &matrix, const Ok_T &ok) {
      failing_cell best;
      best.row = matrix.rows();
      for (Eigen::Index c = 0; c < matrix.cols(); ++c) {
        for (Eigen::Index r = 0; r < best.row; ++r) {
          if (!ok(c, matrix(r, c))) {
            best.row = r;
            best.col = c;
            break;
          }
        }
      }
      return best;
    }

    /// Position `i` of a layout span.
    template <typename Channel_T> const Channel_T &at(std::span<const Channel_T> channels, Eigen::Index i) {
      return channels[static_cast<std::size_t>(i)];
    }
  } // namespace

  std::expected<void, error> validate_input(const tire_model &model, const Eigen::Ref<const Eigen::VectorXd> &input) {
    if (input.size() != model.input_length()) {
      return size_mismatch("input", input.size(), "values", model.name(), "expects", model.input_length());
    }
    const std::span<const input_channel> channels = model.layout().inputs;
    for (Eigen::Index i = 0; i < input.size(); ++i) {
      if (!channel_ok(at(channels, i), input(i))) {
        return std::unexpected<error>(channel_error(model, at(channels, i), input(i)));
      }
    }
    return {};
  }

  std::expected<void, batch_error> validate_inputs(const tire_model &model,
                                                   const Eigen::Ref<const Eigen::MatrixXd> &inputs) {
    if (inputs.cols() != model.input_length()) {
      return batch_failure(0, make_error(error_code::INVALID_ARGUMENT,
                                         std::format("input matrix has {} columns, the {} model expects {}",
                                                     inputs.cols(), model.name(), model.input_length()))
                                  .error());
    }
    const std::span<const input_channel> channels = model.layout().inputs;
    const auto ok = [channels](Eigen::Index c, double v) { return channel_ok(at(channels, c), v); };
    const failing_cell cell = first_failing_cell(inputs, ok);
    if (cell.col < 0) {
      return {};
    }
    return batch_failure(static_cast<std::size_t>(cell.row),
                         channel_error(model, at(channels, cell.col), inputs(cell.row, cell.col)));
  }

  std::expected<void, error> check_output(const tire_model &model, const Eigen::Ref<const Eigen::VectorXd> &output) {
    if (output.size() != model.output_length()) {
      return size_mismatch("output", output.size(), "values", model.name(), "produces", model.output_length());
    }
    const std::span<const output_channel> channels = model.layout().outputs;
    for (Eigen::Index j = 0; j < output.size(); ++j) {
      if (!std::isfinite(output(j))) {
        return std::unexpected<error>(output_error(at(channels, j), output(j)));
      }
    }
    return {};
  }

  std::expected<void, batch_error> check_outputs(const tire_model &model,
                                                 const Eigen::Ref<const Eigen::MatrixXd> &outputs) {
    if (outputs.cols() != model.output_length()) {
      return batch_failure(0, make_error(error_code::INVALID_ARGUMENT,
                                         std::format("output matrix has {} columns, the {} model produces {}",
                                                     outputs.cols(), model.name(), model.output_length()))
                                  .error());
    }
    const auto ok = [](Eigen::Index, double v) { return std::isfinite(v); };
    const failing_cell cell = first_failing_cell(outputs, ok);
    if (cell.col < 0) {
      return {};
    }
    const std::span<const output_channel> channels = model.layout().outputs;
    return batch_failure(static_cast<std::size_t>(cell.row),
                         output_error(at(channels, cell.col), outputs(cell.row, cell.col)));
  }

  std::expected<void, error> check_buffers(const tire_model &model, const double *input, int input_len,
                                           const double *data, int len) {
    if (input == nullptr) {
      return make_error(error_code::INVALID_ARGUMENT, "input buffer is null");
    }
    if (data == nullptr) {
      return make_error(error_code::INVALID_ARGUMENT, "output buffer is null");
    }
    if (input_len != model.input_length()) {
      return make_error(error_code::INVALID_ARGUMENT,
                        std::format("input length {} != {}", input_len, model.input_length()));
    }
    if (len != model.output_length()) {
      return make_error(error_code::INVALID_ARGUMENT,
                        std::format("output length {} != {}", len, model.output_length()));
    }
    return {};
  }

  std::expected<void, error> check_batch_buffers(const tire_model &model, const double *inputs, int rows,
                                                 const double *data, int len) {
    if (rows < 0) {
      return make_error(error_code::INVALID_ARGUMENT, std::format("row count {} is negative", rows));
    }
    if (rows > 0) {
      if (inputs == nullptr) {
        return make_error(error_code::INVALID_ARGUMENT, "input buffer is null");
      }
      if (data == nullptr) {
        return make_error(error_code::INVALID_ARGUMENT, "output buffer is null");
      }
    }
    const Eigen::Index expected = static_cast<Eigen::Index>(rows) * model.output_length();
    if (static_cast<Eigen::Index>(len) != expected) {
      return make_error(error_code::INVALID_ARGUMENT,
                        std::format("output length {} != {} rows * {} outputs", len, rows, model.output_length()));
    }
    return {};
  }

  std::expected<Eigen::MatrixXd, batch_error> checked_evaluate_batch(const tire_model &model,
                                                                     const Eigen::Ref<const Eigen::MatrixXd> &inputs) {
    if (const std::expected<void, batch_error> valid = validate_inputs(model, inputs); !valid.has_value()) {
      return std::unexpected<batch_error>(valid.error());
    }
    Eigen::MatrixXd outputs = model.evaluate_batch(inputs);
    if (const std::expected<void, batch_error> finite = check_outputs(model, outputs); !finite.has_value()) {
      return std::unexpected<batch_error>(finite.error());
    }
    return outputs;
  }
} // namespace mf
