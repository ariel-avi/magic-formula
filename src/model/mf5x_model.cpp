#include <Eigen/Core>
#include <algorithm>
#include <array>
#include <expected>
#include <magic_formula/detail/lanes.hpp>
#include <magic_formula/detail/mf5x_characteristics.hpp>
#include <magic_formula/detail/mf5x_kernels.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_forms.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/mf5x_model.hpp>
#include <magic_formula/mf5x_signs.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/sign_transform.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>

namespace mf {
  namespace {
    using detail::block_lane;
    using detail::point_lane;

    /// The input lanes of one point.
    detail::input_lanes<point_lane> load_point(const Eigen::Ref<const Eigen::VectorXd> &input) noexcept {
      return detail::input_lanes<point_lane>{.fz_n = point_lane::Constant(input(mf5x::FZ)),
                                             .alpha_rad = point_lane::Constant(input(mf5x::SA)),
                                             .kappa = point_lane::Constant(input(mf5x::SR)),
                                             .gamma_rad = point_lane::Constant(input(mf5x::IA)),
                                             .vx_mps = point_lane::Constant(input(mf5x::VX))};
    }

    /// The input lanes of the `count` rows starting at `first`.
    detail::input_lanes<block_lane> load_block(const Eigen::Ref<const Eigen::MatrixXd> &inputs, Eigen::Index first,
                                               Eigen::Index count) noexcept {
      const block_lane fz = inputs.col(mf5x::FZ).segment(first, count).array();
      const block_lane alpha = inputs.col(mf5x::SA).segment(first, count).array();
      const block_lane kappa = inputs.col(mf5x::SR).segment(first, count).array();
      const block_lane gamma = inputs.col(mf5x::IA).segment(first, count).array();
      const block_lane vx = inputs.col(mf5x::VX).segment(first, count).array();
      return detail::input_lanes<block_lane>{
          .fz_n = fz, .alpha_rad = alpha, .kappa = kappa, .gamma_rad = gamma, .vx_mps = vx};
    }

    /// Writes the one-point output lanes into the output vector.
    void store_point(const detail::output_lanes<point_lane> &lanes, Eigen::Ref<Eigen::VectorXd> output) noexcept {
      output(mf5x::FX) = lanes.fx_n(0);
      output(mf5x::FY) = lanes.fy_n(0);
      output(mf5x::MZ) = lanes.mz_nm(0);
      output(mf5x::MX) = lanes.mx_nm(0);
      output(mf5x::MY) = lanes.my_nm(0);
      output(mf5x::FZ_CLAMPED) = lanes.fz_clamped(0);
      output(mf5x::SR_CLAMPED) = lanes.kappa_clamped(0);
      output(mf5x::SA_CLAMPED) = lanes.alpha_clamped(0);
      output(mf5x::IA_CLAMPED) = lanes.gamma_clamped(0);
      output(mf5x::BELOW_VXLOW) = lanes.below_vxlow(0);
    }

    /// Writes the output lanes of the `count` rows starting at `first` into the output matrix.
    void store_block(const detail::output_lanes<block_lane> &lanes, Eigen::Ref<Eigen::MatrixXd> outputs,
                     Eigen::Index first, Eigen::Index count) noexcept {
      outputs.col(mf5x::FX).segment(first, count) = lanes.fx_n.matrix();
      outputs.col(mf5x::FY).segment(first, count) = lanes.fy_n.matrix();
      outputs.col(mf5x::MZ).segment(first, count) = lanes.mz_nm.matrix();
      outputs.col(mf5x::MX).segment(first, count) = lanes.mx_nm.matrix();
      outputs.col(mf5x::MY).segment(first, count) = lanes.my_nm.matrix();
      outputs.col(mf5x::FZ_CLAMPED).segment(first, count) = lanes.fz_clamped.matrix();
      outputs.col(mf5x::SR_CLAMPED).segment(first, count) = lanes.kappa_clamped.matrix();
      outputs.col(mf5x::SA_CLAMPED).segment(first, count) = lanes.alpha_clamped.matrix();
      outputs.col(mf5x::IA_CLAMPED).segment(first, count) = lanes.gamma_clamped.matrix();
      outputs.col(mf5x::BELOW_VXLOW).segment(first, count) = lanes.below_vxlow.matrix();
    }

    /// Writes the one-point characteristic lanes into the characteristics vector.
    void store_point_characteristics(const detail::characteristic_lanes<point_lane> &lanes,
                                     Eigen::Ref<Eigen::VectorXd> characteristics) noexcept {
      characteristics(mf5x::DFY_DSA) = lanes.dfy_dsa_n_per_rad(0);
      characteristics(mf5x::DFX_DSR) = lanes.dfx_dsr_n(0);
      characteristics(mf5x::FY_OVER_SA) = lanes.fy_over_sa_n_per_rad(0);
      characteristics(mf5x::FX_OVER_SR) = lanes.fx_over_sr_n(0);
      characteristics(mf5x::DY) = lanes.dy_n(0);
      characteristics(mf5x::DX) = lanes.dx_n(0);
      characteristics(mf5x::SA_AT_PEAK_FY) = lanes.sa_at_peak_fy_rad(0);
      characteristics(mf5x::SR_AT_PEAK_FX) = lanes.sr_at_peak_fx(0);
    }

    /// Writes the characteristic lanes of the `count` rows starting at `first` into the characteristics matrix.
    void store_block_characteristics(const detail::characteristic_lanes<block_lane> &lanes,
                                     Eigen::Ref<Eigen::MatrixXd> characteristics, Eigen::Index first,
                                     Eigen::Index count) noexcept {
      characteristics.col(mf5x::DFY_DSA).segment(first, count) = lanes.dfy_dsa_n_per_rad.matrix();
      characteristics.col(mf5x::DFX_DSR).segment(first, count) = lanes.dfx_dsr_n.matrix();
      characteristics.col(mf5x::FY_OVER_SA).segment(first, count) = lanes.fy_over_sa_n_per_rad.matrix();
      characteristics.col(mf5x::FX_OVER_SR).segment(first, count) = lanes.fx_over_sr_n.matrix();
      characteristics.col(mf5x::DY).segment(first, count) = lanes.dy_n.matrix();
      characteristics.col(mf5x::DX).segment(first, count) = lanes.dx_n.matrix();
      characteristics.col(mf5x::SA_AT_PEAK_FY).segment(first, count) = lanes.sa_at_peak_fy_rad.matrix();
      characteristics.col(mf5x::SR_AT_PEAK_FX).segment(first, count) = lanes.sr_at_peak_fx.matrix();
    }
  } // namespace

  mf5x_model::mf5x_model(const mf5x_parameters &parameters, const mf5x_forms &forms, const model_options &options)
      : tire_model(mf5x::INPUT_LENGTH, mf5x::OUTPUT_LENGTH, parameters.limits, parameters.metadata, options),
        coefficients_(parameters.coefficients), forms_(forms) {}

  int mf5x_model::input_length() const noexcept {
    return mf5x::INPUT_LENGTH;
  }

  int mf5x_model::output_length() const noexcept {
    return mf5x::OUTPUT_LENGTH;
  }

  Eigen::VectorXd mf5x_model::reference_input() const {
    // Plain array first: only the final vector owns memory, so no cleanup path exists between its steps.
    std::array<double, mf5x::INPUT_LENGTH> values{};
    values[mf5x::FZ] = coefficients_.FNOMIN * coefficients_.LFZO;
    values[mf5x::VX] = coefficients_.LONGVL;
    return Eigen::Map<const Eigen::VectorXd>(values.data(), mf5x::INPUT_LENGTH);
  }

  const mf5x_coefficients &mf5x_model::coefficients() const noexcept {
    return coefficients_;
  }

  const mf5x_forms &mf5x_model::forms() const noexcept {
    return forms_;
  }

  std::expected<void, error> mf5x_model::validate_parameters(const mf5x_parameters &parameters) {
    if (const std::expected<void, error> valid = validate_coefficients(parameters.coefficients); !valid.has_value()) {
      return std::unexpected<error>(valid.error());
    }
    return validate_limits(parameters.limits, mf5x::LIMITS);
  }

  std::unique_ptr<tire_model> mf5x_model::rebuild(const sign_transforms &transforms,
                                                  const Eigen::VectorXd &limits) const {
    mf5x_parameters parameters;
    parameters.coefficients = mf5x::transform_coefficients(coefficients_, transforms);
    parameters.limits = limits;
    parameters.metadata = metadata();
    return with_parameters(parameters);
  }

  void mf5x_model::compute(const Eigen::Ref<const Eigen::VectorXd> &input,
                           Eigen::Ref<Eigen::VectorXd> output) const noexcept {
    store_point(detail::mf5x_pipeline(coefficients_, forms_, options().mode, limits(), load_point(input)), output);
  }

  void mf5x_model::compute_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                                 Eigen::Ref<Eigen::MatrixXd> outputs) const noexcept {
    for (Eigen::Index first = 0; first < inputs.rows(); first += detail::BLOCK_ROWS) {
      const Eigen::Index count = std::min(detail::BLOCK_ROWS, inputs.rows() - first);
      const detail::input_lanes<block_lane> raw = load_block(inputs, first, count);
      const detail::output_lanes<block_lane> result =
          detail::mf5x_pipeline(coefficients_, forms_, options().mode, limits(), raw);
      store_block(result, outputs, first, count);
    }
  }

  void mf5x_model::compute_characteristics(const Eigen::Ref<const Eigen::VectorXd> &input,
                                           Eigen::Ref<Eigen::VectorXd> characteristics) const noexcept {
    store_point_characteristics(
        detail::mf5x_characteristics(coefficients_, forms_, options().mode, limits(), load_point(input)),
        characteristics);
  }

  void mf5x_model::compute_characteristics_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                                                 Eigen::Ref<Eigen::MatrixXd> characteristics) const noexcept {
    for (Eigen::Index first = 0; first < inputs.rows(); first += detail::BLOCK_ROWS) {
      const Eigen::Index count = std::min(detail::BLOCK_ROWS, inputs.rows() - first);
      const detail::input_lanes<block_lane> raw = load_block(inputs, first, count);
      const detail::characteristic_lanes<block_lane> result =
          detail::mf5x_characteristics(coefficients_, forms_, options().mode, limits(), raw);
      store_block_characteristics(result, characteristics, first, count);
    }
  }
} // namespace mf
