#pragma once

#include <Eigen/Core>
#include <expected>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/mf5x_coefficients.hpp>
#include <magic_formula/mf5x_forms.hpp>
#include <magic_formula/mf5x_layout.hpp>
#include <magic_formula/sign_transform.hpp>
#include <magic_formula/tire_model.hpp>
#include <memory>

namespace mf {
  /// Abstract MF 5.x-family formulation: the shared steady-state evaluation order of PAC2002 and MF 5.2, built from
  /// the kernels of `mf::detail`. The evaluation order is fixed (`compute` and `compute_batch` are final); the
  /// PAC2002 / MF 5.2 differences are the closed set of `mf5x_forms` chosen at construction. Equations:
  /// docs/research/pac2002-vs-mf52.md.
  ///
  /// The vectors are indexed by the unscoped enums of `mf::mf5x`, exposed here as class aliases. Because the enums
  /// are unscoped (`enum : std::uint8_t`), an enumerator is a position: `output(output_index::FX)`.
  ///
  /// Each point is conditioned without branches: Vx < VXLOW sets the BELOW_VXLOW flag (Vx is never modified); Fz, SR,
  /// SA and IA are clamped to their limits (an infinite bound never moves a value) and the flags FZ_CLAMPED,
  /// SR_CLAMPED, SA_CLAMPED and IA_CLAMPED hold 1.0 where a value moved; a point whose raw or clamped Fz is zero, NaN
  /// or on the opposite side of FNOMIN has exactly zero forces and moments, and its clamp flags are 0.0 when its raw Fz
  /// is not on the side of FNOMIN. The evaluation runs on blocks of points with vectorised math and allocates nothing.
  class mf5x_model : public tire_model {
  public:
    /// Positions of the input vector (aliases of `mf5x::input_index`).
    using input_index = mf5x::input_index;
    /// Positions of the output vector (aliases of `mf5x::output_index`).
    using output_index = mf5x::output_index;
    /// Positions of the limits vector (aliases of `mf5x::limit_index`).
    using limit_index = mf5x::limit_index;

    /// `mf5x::INPUT_LENGTH`.
    [[nodiscard]] int input_length() const noexcept final;

    /// `mf5x::OUTPUT_LENGTH`.
    [[nodiscard]] int output_length() const noexcept final;

    /// FZ = FNOMIN*LFZO, SA = SR = IA = 0, VX = LONGVL (in `mf5x::input_index` order).
    [[nodiscard]] Eigen::VectorXd reference_input() const final;

    /// The coefficient set this model was built from.
    [[nodiscard]] const mf5x_coefficients &coefficients() const noexcept;

    /// The formulation forms this model was built with.
    [[nodiscard]] const mf5x_forms &forms() const noexcept;

  protected:
    /// Stores the coefficients, the forms, the limits, the metadata and the options. `parameters` has already passed
    /// `validate_parameters`.
    mf5x_model(const mf5x_parameters &parameters, const mf5x_forms &forms, const model_options &options);

    /// `validate_coefficients` followed by `validate_limits(parameters.limits, mf5x::LIMITS)`. The first error is
    /// returned unchanged, including its `where.key`. Used by the `create` functions of the derived classes.
    [[nodiscard]] static std::expected<void, error> validate_parameters(const mf5x_parameters &parameters);

  private:
    /// `with_parameters` of this model's coefficients transformed by `transforms` (`mf5x::transform_coefficients`),
    /// the given limits and this model's metadata.
    [[nodiscard]] std::unique_ptr<tire_model> rebuild(const sign_transforms &transforms,
                                                      const Eigen::VectorXd &limits) const final;

    /// A new model of this concrete formulation with `parameters` and this model's forms and options; no `create`
    /// validation, because sign flips keep every validated invariant.
    [[nodiscard]] virtual std::unique_ptr<tire_model> with_parameters(const mf5x_parameters &parameters) const = 0;

    /// Runs `detail::mf5x_pipeline` on one-point lanes.
    void compute(const Eigen::Ref<const Eigen::VectorXd> &input,
                 Eigen::Ref<Eigen::VectorXd> output) const noexcept final;

    /// Runs `detail::mf5x_pipeline` on blocks of `detail::BLOCK_ROWS` points.
    void compute_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                       Eigen::Ref<Eigen::MatrixXd> outputs) const noexcept final;

    mf5x_coefficients coefficients_;
    mf5x_forms forms_;
  };
} // namespace mf
