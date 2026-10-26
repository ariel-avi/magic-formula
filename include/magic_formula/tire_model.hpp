#pragma once

#include <Eigen/Core>
#include <magic_formula/coordinate_identification.hpp>
#include <magic_formula/error.hpp>
#include <magic_formula/evaluation_types.hpp>
#include <magic_formula/model_layout.hpp>
#include <magic_formula/sign_transform.hpp>
#include <memory>
#include <span>
#include <string_view>

namespace mf {
  struct model_conversion;

  /// Abstract base tire-model formulation with an index-mapped, vectorised evaluation interface.
  ///
  /// A model maps an input vector (one value per `layout().inputs` channel) to an output vector (one value per
  /// `layout().outputs` channel); each concrete model names its positions with index enums (for example
  /// `pac2002_model::input_index`). The four public `evaluate` forms and the two `evaluate_characteristics` forms are
  /// the only entry points (a non-virtual interface) and own the error policy: the hot path performs no checks unless
  /// the model was constructed with `model_options::throw_if_model_failure`, in which case every form runs the public
  /// free checks of `model_checks.hpp` and throws `mf::model_failure`. The derived classes only supply the unchecked,
  /// allocation-free computation. After construction a model is immutable, so concurrent `evaluate` calls are safe.
  ///
  /// Writable `Eigen::Ref<...>` parameters of the private hooks are taken by value (the Eigen idiom, so that they bind
  /// `Eigen::Map` temporaries). This is the documented exception to the rule of never passing Eigen objects by value.
  class tire_model {
  public:
    // Coverage exclusion justification: the compiler also emits the D0 (deleting) destructor of this abstract class.
    // It runs only for an object whose dynamic type is exactly tire_model, which cannot exist (pure virtual
    // members), so no test can execute it. The base-object destructor (D2) is covered through derived classes.
    virtual ~tire_model() = default; // GCOVR_EXCL_LINE: unreachable D0 deleting destructor, see comment above
    tire_model(const tire_model &) = delete;
    tire_model &operator=(const tire_model &) = delete;

    /// Human-readable model name ("PAC2002", "MF 5.2").
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /// Number of input values per point; equals `layout().inputs.size()`.
    [[nodiscard]] virtual int input_length() const noexcept = 0;

    /// Number of output values per point; equals `layout().outputs.size()`.
    [[nodiscard]] virtual int output_length() const noexcept = 0;

    /// The index map (names, units, domains) of the inputs, outputs and limits.
    [[nodiscard]] virtual const model_layout &layout() const noexcept = 0;

    /// Evaluates one point. `input` holds `input_length()` values in the model's input_index order. Returns
    /// `output_length()` values in output_index order. Unchecked unless `options().throw_if_model_failure` (then
    /// `validate_input`, then `check_output`).
    [[nodiscard]] Eigen::VectorXd evaluate(const Eigen::Ref<const Eigen::VectorXd> &input) const;

    /// Evaluates one point into a caller buffer; never allocates. `input_len` and `len` are read only when
    /// `options().throw_if_model_failure` is set (`check_buffers`); otherwise they must equal `input_length()` and
    /// `output_length()`. The buffers must not overlap.
    void evaluate(const double *input, int input_len, double *data, int len) const;

    /// Evaluates rows x `input_length()` points (one row per point, one column per input_index) into a new
    /// rows x `output_length()` matrix. The batch is processed in blocks with vectorised math.
    [[nodiscard]] Eigen::MatrixXd evaluate_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs) const;

    /// Batch evaluation on column-major buffers (the memory layout of an `Eigen::MatrixXd`): `inputs[c*rows + r]` is
    /// input `c` of point `r` and `data[c*rows + r]` is output `c` of point `r`. `len` must equal
    /// `rows*output_length()` and `rows` must be >= 0; `rows` and `len` are read only when
    /// `options().throw_if_model_failure` is set, otherwise they are a precondition. Never allocates. The buffers
    /// must not overlap.
    void evaluate_batch(const double *inputs, int rows, double *data, int len) const;

    /// Number of characteristic values per point: `layout().characteristics.size()`.
    [[nodiscard]] int characteristics_length() const noexcept;

    /// The characteristics of one point (`layout().characteristics` order) for `input` (input_index order).
    /// Unchecked unless `options().throw_if_model_failure`: validate_input, compute, check_characteristics, throwing
    /// `model_failure` on the first failure. A NaN in a channel with `nan_allowed` is a value, not a failure.
    [[nodiscard]] Eigen::VectorXd evaluate_characteristics(const Eigen::Ref<const Eigen::VectorXd> &input) const;

    /// rows x `input_length()` points into a new rows x `characteristics_length()` matrix (blocks, vectorised math);
    /// same error policy with `validate_inputs` / `check_characteristics_batch`.
    [[nodiscard]] Eigen::MatrixXd evaluate_characteristics_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs) const;

    /// The input vector (`input_length()` values, input order) of the model's reference operating point: nominal
    /// load, zero slip ratio, slip angle and inclination, reference speed (and the nominal pressure for formulations
    /// with a pressure input). `find_coordinate_system` probes around it.
    [[nodiscard]] virtual Eigen::VectorXd reference_input() const = 0;

    /// `find_coordinate_system(IDENTIFICATION_INDICATORS)`.
    [[nodiscard]] coordinate_identification find_coordinate_system() const;

    /// Identifies the coordinate system of the coefficient set from its evaluated behaviour (coordinate-systems.md):
    /// reference = `reference_input()`; probes = `detail::plan_probes(layout(), reference, indicators)`; one unchecked
    /// `compute_batch` on the probe rows (so it never throws `model_failure`, whatever the options);
    /// `assemble_identification(detail::read_probes(...), reference)`. Total: may throw only std::bad_alloc. Const and
    /// thread-safe. The returned readings copy the definitions' string_views: the string data of `indicators` must
    /// outlive the returned identification.
    [[nodiscard]] coordinate_identification
    find_coordinate_system(std::span<const indicator_definition> indicators) const;

    /// SAE when the reference FZ (`reference_input()`) is negative, else ISO; also ISO when the layout has no FZ input.
    [[nodiscard]] coordinate_system loaded_side() const;

    /// A new model of the same formulation, forms, options and metadata with its coefficients and limits transformed
    /// by `transforms` (exact sign flips; an empty set gives an identical copy):
    /// evaluate(transformed(T), T_in(x)) == T_out(evaluate(x)); applying it twice restores the values.
    /// = rebuild(transforms, transform_limits(limits(), layout().limits, transforms)).
    [[nodiscard]] std::unique_ptr<tire_model> transformed(const sign_transforms &transforms) const;

    /// `find_coordinate_system()`, then `plan_conversion(identification.effective, target)`, then `transformed(plan)`.
    [[nodiscard]] model_conversion convert_coordinate_system(coordinate_system target) const;

    /// Limits vector, indexed by the model's limit_index.
    [[nodiscard]] const Eigen::VectorXd &limits() const noexcept;

    /// [MODEL] metadata this model was constructed with.
    [[nodiscard]] const model_metadata &metadata() const noexcept;

    /// Options this model was constructed with.
    [[nodiscard]] const model_options &options() const noexcept;

  protected:
    /// Stores the per-point input and output lengths (the values `input_length()` and `output_length()` return; kept
    /// as plain data so that the evaluate forms size their views without virtual calls), the limits, the metadata and
    /// the options. The caller has already validated the limits.
    tire_model(int input_len, int output_len, Eigen::VectorXd limits, const model_metadata &metadata,
               const model_options &options);
    tire_model(tire_model &&) noexcept = default;
    tire_model &operator=(tire_model &&) noexcept = default;

  private:
    /// Primitive of `transformed`: a new model of this formulation with its coefficients transformed by `transforms`
    /// and the given (already transformed) limits; options and metadata copied.
    [[nodiscard]] virtual std::unique_ptr<tire_model> rebuild(const sign_transforms &transforms,
                                                              const Eigen::VectorXd &limits) const = 0;

    /// One point, no checks, no allocation. The sizes are guaranteed by the caller.
    virtual void compute(const Eigen::Ref<const Eigen::VectorXd> &input,
                         Eigen::Ref<Eigen::VectorXd> output) const noexcept = 0;

    /// `inputs.rows()` (>= 0) points, no checks, no allocation.
    virtual void compute_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                               Eigen::Ref<Eigen::MatrixXd> outputs) const noexcept = 0;

    /// One point of characteristics, no checks, no allocation. The sizes are guaranteed by the caller.
    virtual void compute_characteristics(const Eigen::Ref<const Eigen::VectorXd> &input,
                                         Eigen::Ref<Eigen::VectorXd> characteristics) const noexcept = 0;

    /// `inputs.rows()` (>= 0) points of characteristics, no checks, no allocation.
    virtual void compute_characteristics_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                                               Eigen::Ref<Eigen::MatrixXd> characteristics) const noexcept = 0;

    /// The one-point error policy around `compute`.
    void evaluate_into(const Eigen::Ref<const Eigen::VectorXd> &input, Eigen::Ref<Eigen::VectorXd> output) const;

    /// The batch error policy around `compute_batch`.
    void evaluate_batch_into(const Eigen::Ref<const Eigen::MatrixXd> &inputs,
                             Eigen::Ref<Eigen::MatrixXd> outputs) const;

    int input_length_;
    int output_length_;
    Eigen::VectorXd limits_;
    model_metadata metadata_;
    model_options options_;
  };

  /// The result of `tire_model::convert_coordinate_system`.
  struct model_conversion {
    /// Never null; an identical copy when nothing was applied.
    std::unique_ptr<tire_model> model;
    /// The plan that was applied.
    sign_transforms applied;
    /// Of the source model.
    coordinate_identification identification;
  };
} // namespace mf
