#include <Eigen/Core>
#include <cstddef>
#include <expected>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/coordinate_system.hpp>
#include <magic_formula/detail/identification_probes.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/model_checks.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/sign_transform.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <optional>
#include <span>
#include <utility>

namespace mf {
  namespace {
    /// Throws `model_failure` for a failed one-point check.
    void require(const std::expected<void, error> &result) {
      if (!result.has_value()) {
        throw model_failure(result.error(), std::nullopt);
      }
    }

    /// Throws `model_failure` carrying the row of a failed batch check.
    void require(const std::expected<void, batch_error> &result) {
      if (!result.has_value()) {
        throw model_failure(result.error().cause, result.error().index);
      }
    }
  } // namespace

  tire_model::tire_model(int input_len, int output_len, Eigen::VectorXd limits, const model_metadata &metadata,
                         const model_options &options)
      : input_length_(input_len), output_length_(output_len), limits_(std::move(limits)), metadata_(metadata),
        options_(options) {}

  const Eigen::VectorXd &tire_model::limits() const noexcept {
    return limits_;
  }

  const model_metadata &tire_model::metadata() const noexcept {
    return metadata_;
  }

  const model_options &tire_model::options() const noexcept {
    return options_;
  }

  // NOLINTBEGIN(performance-unnecessary-value-param): a writable Eigen::Ref is taken by value so that it binds Map
  // temporaries (the Eigen idiom); a const reference to it would not bind the writable hook parameter.
  void tire_model::evaluate_into(const Eigen::Ref<const Eigen::VectorXd> &input,
                                 Eigen::Ref<Eigen::VectorXd> output) const {
    if (options_.throw_if_model_failure) {
      require(validate_input(*this, input));
    }
    compute(input, output);
    if (options_.throw_if_model_failure) {
      require(check_output(*this, output));
    }
  }

  void tire_model::evaluate_batch_into(const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                                       Eigen::Ref<Eigen::MatrixXd> outputs) const {
    if (options_.throw_if_model_failure) {
      require(validate_inputs(*this, inputs));
    }
    compute_batch(inputs, outputs);
    if (options_.throw_if_model_failure) {
      require(check_outputs(*this, outputs));
    }
  }
  // NOLINTEND(performance-unnecessary-value-param)

  Eigen::VectorXd tire_model::evaluate(const Eigen::Ref<const Eigen::VectorXd> &input) const {
    Eigen::VectorXd output(static_cast<Eigen::Index>(output_length_));
    evaluate_into(input, output);
    return output;
  }

  void tire_model::evaluate(const double *input, int input_len, double *data, int len) const {
    if (options_.throw_if_model_failure) {
      require(check_buffers(*this, input, input_len, data, len));
    }
    const Eigen::Map<const Eigen::VectorXd> in(input, input_length_);
    Eigen::Map<Eigen::VectorXd> out(data, output_length_);
    evaluate_into(in, out);
  }

  Eigen::MatrixXd tire_model::evaluate_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs) const {
    Eigen::MatrixXd outputs(inputs.rows(), static_cast<Eigen::Index>(output_length_));
    evaluate_batch_into(inputs, outputs);
    return outputs;
  }

  coordinate_identification tire_model::find_coordinate_system() const {
    return find_coordinate_system(IDENTIFICATION_INDICATORS);
  }

  coordinate_identification tire_model::find_coordinate_system(std::span<const indicator_definition> indicators) const {
    Eigen::VectorXd reference = reference_input();
    detail::identification_probes probes = detail::plan_probes(layout(), reference, indicators);
    Eigen::MatrixXd outputs(probes.inputs.rows(), output_length());
    compute_batch(probes.inputs, outputs);
    return assemble_identification(detail::read_probes(layout(), indicators, probes, outputs), std::move(reference));
  }

  coordinate_system tire_model::loaded_side() const {
    const std::optional<std::size_t> fz = find_channel(layout().inputs, "FZ");
    if (!fz.has_value()) {
      return coordinate_system::ISO;
    }
    const Eigen::VectorXd reference = reference_input();
    const bool negative = reference(static_cast<Eigen::Index>(*fz)) < 0.0;
    return negative ? coordinate_system::SAE : coordinate_system::ISO;
  }

  std::unique_ptr<tire_model> tire_model::transformed(const sign_transforms &transforms) const {
    return rebuild(transforms, transform_limits(limits_, layout().limits, transforms));
  }

  model_conversion tire_model::convert_coordinate_system(coordinate_system target) const {
    coordinate_identification identification = find_coordinate_system();
    const sign_transforms plan = plan_conversion(identification.effective, target);
    return model_conversion{.model = transformed(plan), .applied = plan, .identification = std::move(identification)};
  }

  void tire_model::evaluate_batch(const double *inputs, int rows, double *data, int len) const {
    if (options_.throw_if_model_failure) {
      require(check_batch_buffers(*this, inputs, rows, data, len));
    }
    const Eigen::Map<const Eigen::MatrixXd> in(inputs, rows, input_length_);
    Eigen::Map<Eigen::MatrixXd> out(data, rows, output_length_);
    evaluate_batch_into(in, out);
  }
} // namespace mf
