#pragma once
#include <Eigen/Core>
#include <expected>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf61_coefficients.hpp>
#include <magic_formula/mf6x_layout.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/sign_transform.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>
#include <string_view>
namespace mf {
  /// MF 6.1 with inflation pressure, optional turn slip, and rolling radii.
  class mf61_model final : public tire_model {
  public:
    using input_index = mf6x::input_index;
    using output_index = mf6x::output_index;
    using limit_index = mf6x::limit_index;
    using characteristic_index = mf6x::characteristic_index;
    /// Validates parameters and constructs an immutable model.
    [[nodiscard]] static std::expected<mf61_model, error> create(mf61_parameters parameters,
                                                                 const model_options &options = {});
    /// The MF 6.1 formulation name.
    [[nodiscard]] std::string_view name() const noexcept override;
    /// Seven input channels.
    [[nodiscard]] int input_length() const noexcept override;
    /// Forces, moments, conditioning flags and rolling radii.
    [[nodiscard]] int output_length() const noexcept override;
    /// Channel and limit definitions.
    [[nodiscard]] const model_layout &layout() const noexcept override;
    /// Nominal load, speed and pressure, zero slips and camber.
    [[nodiscard]] Eigen::VectorXd reference_input() const override;
    /// Immutable coefficient set.
    [[nodiscard]] const mf61_coefficients &coefficients() const noexcept;

  private:
    mf61_model(const mf61_parameters &parameters, const model_options &options);
    [[nodiscard]] std::unique_ptr<tire_model> rebuild(const sign_transforms &transforms,
                                                      const Eigen::VectorXd &limits) const override;
    void compute(const Eigen::Ref<const Eigen::VectorXd> &input,
                 Eigen::Ref<Eigen::VectorXd> output) const noexcept override;
    void compute_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                       Eigen::Ref<Eigen::MatrixXd> outputs) const noexcept override;
    void compute_characteristics(const Eigen::Ref<const Eigen::VectorXd> &input,
                                 Eigen::Ref<Eigen::VectorXd> characteristics) const noexcept override;
    void compute_characteristics_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                                       Eigen::Ref<Eigen::MatrixXd> characteristics) const noexcept override;
    mf61_coefficients coefficients_;
  };
} // namespace mf
