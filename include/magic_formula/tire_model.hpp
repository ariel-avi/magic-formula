#pragma once

#include <expected>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <span>
#include <string_view>
#include <vector>

namespace mf {
  /// Rejects any present interval whose lower bound exceeds its upper bound.
  /// `lower == upper` is valid. Absent intervals are not checked.
  /// @return success, or INVALID_VALUE whose `where.key` is the lower-limit key (FZMIN, KPUMIN, ALPMIN or CAMMIN)
  [[nodiscard]] std::expected<void, error> validate_limits(const operating_limits &limits);

  /// Abstract base tire-model formulation.
  ///
  /// `evaluate` is a non-virtual Template Method, so every model honours the same pipeline:
  /// validate_input -> flags.below_vxlow -> Fz <= 0 gives zero outputs -> clamp to the limits -> compute ->
  /// finite check.
  /// Derived classes supply the formulation (`compute`) and a name, and may refine the input domain
  /// (`validate_input`).
  /// After construction a model is immutable, so concurrent `evaluate` calls are safe.
  class tire_model {
  public:
    // Coverage exclusion justification: the compiler also emits the D0 (deleting) destructor of this abstract class.
    // It runs only for an object whose dynamic type is exactly tire_model, which cannot exist (pure virtual
    // members), so no test can execute it. The base-object destructor (D2) is covered through derived classes.
    virtual ~tire_model() = default; // GCOVR_EXCL_LINE: unreachable D0 deleting destructor, see comment above
    tire_model(const tire_model &) = delete;
    tire_model &operator=(const tire_model &) = delete;

    /// Evaluates one operating point.
    ///
    /// Pipeline: (1) `validate_input`; (2) `flags.below_vxlow = Vx < VXLOW`; (3) raw Fz <= 0 (including -0.0)
    /// returns exactly zero outputs without calling `compute`; (4) Fz, SR, SA and IA are clamped to their
    /// limits when both bounds exist, flagging each clamped variable; a clamped Fz that is still <= 0 (a
    /// validated interval may end at or below zero) also gives the zero outputs; (5) `compute`; (6) every output must
    /// be finite, otherwise NUMERICAL naming the first offending output.
    /// @param point operating point in SI units (angles in rad)
    /// @param mode combined or pure-slip evaluation (forwarded to `compute`)
    /// @return outputs with the applied-adjustment flags, or the first error of the pipeline
    [[nodiscard]] std::expected<tire_outputs, error> evaluate(const operating_point &point,
                                                              slip_mode mode = slip_mode::COMBINED) const;

    /// Evaluates a sequence of operating points in order and stops at the first failure. This is the seam for a later
    /// vectorised (Eigen) batch path; today it loops over `evaluate`.
    /// @return one output per point, or the index and cause of the first failing point
    [[nodiscard]] std::expected<std::vector<tire_outputs>, batch_error>
    evaluate_batch(std::span<const operating_point> points, slip_mode mode = slip_mode::COMBINED) const;

    /// Human-readable model name ("PAC2002", "MF 5.2").
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /// Validity ranges and VXLOW this model was constructed with.
    [[nodiscard]] const operating_limits &limits() const noexcept;

    /// [MODEL] metadata this model was constructed with.
    [[nodiscard]] const model_metadata &metadata() const noexcept;

  protected:
    /// Stores copies of the limits and the metadata. The caller has already run `validate_limits`.
    tire_model(const operating_limits &limits, const model_metadata &metadata);
    tire_model(tire_model &&) noexcept = default;
    tire_model &operator=(tire_model &&) noexcept = default;

    /// Input-domain hook. The default accepts an operating point when every field is finite (INVALID_INPUT naming the
    /// first non-finite field, checked in the order FZ, SR, SA, IA, VX) and Vx >= 0 (0 and -0.0 pass).
    /// Derived models may narrow the domain, for example MF 5.2 requires Vx > 0.
    [[nodiscard]] virtual std::expected<void, error> validate_input(const operating_point &point) const;

  private:
    /// Formulation hook. Called only with validated, clamped inputs and Fz > 0.
    [[nodiscard]] virtual tire_outputs compute(const operating_point &point, slip_mode mode) const noexcept = 0;

    operating_limits limits_;
    model_metadata metadata_;
  };
} // namespace mf
