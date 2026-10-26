# Architecture

This note describes the code as it is. The equations it implements are in [`docs/research/`](research/README.md); the project rules (defaults, errors, numerical guards, choices between alternative forms) are in [`research/policies.md`](research/policies.md). The routing rules are in [`research/model-routing.md`](research/model-routing.md).

## Layers

Dependencies point downwards only. The core model knows nothing about files or the command line.

| Layer | Headers (`include/magic_formula/`) | Sources (`src/`) | Namespace | Role |
|-------|------|------|-----------|------|
| core model | `tire_model.hpp`, `sign_transform.hpp`, `coordinate_system.hpp`, `coordinate_identification.hpp`, `model_layout.hpp`, `mf5x_layout.hpp`, `mf5x_signs.hpp`, `model_checks.hpp`, `mf5x_model.hpp`, `pac2002_model.hpp`, `mf52_model.hpp`, `mf5x_forms.hpp`, `mf5x_coefficients.hpp`, `evaluation_types.hpp`, `error.hpp` | `model/`, `error.cpp` | `mf`, `mf::mf5x` | Abstract formulation, the index maps, the two concrete models, the free check functions, the sign transforms, the data conversion between ISO and SAE, the coordinate-system identification and the model conversion. No I/O, no printing. |
| detail | `detail/lanes.hpp`, `detail/dual_lane.hpp`, `detail/mf5x_kernels.hpp`, `detail/mf5x_characteristics.hpp`, `detail/stationary_point.hpp`, `detail/identification_probes.hpp`, `detail/text.hpp` | `detail/` | `mf::detail` | Internal, lane-templated equation kernels, the forward-mode `dual_lane`, the characteristics kernel and the stationary-point finder, the identification probe planning and reading (`plan_probes`, `read_probes`) and helpers. Under `include/` so tests can include them; production code includes `detail/mf5x_kernels.hpp` only from `src/model/mf5x_model.cpp`. |
| io | `io/*.hpp` | `io/` | `mf::io` | `.tir` lexing (`tir_document`, `tir_reader`), unit check, model routing, key-to-coefficient bindings, the model loader (factory), the `.tir` text editor (`tir_editor`: `replace_value` changes only the bytes of one value, using each entry's `value_offset`/`value_length` span, and re-parses to check the result), the `.tir` text conversion between ISO and SAE (`tir_conversion`: `convert_tir`), the input-table parser (header, mandatory unit line, rows) and its mapping onto a model's input channels, the CSV result writer. |
| cli | `cli/cli.hpp` | `cli/cli.cpp` | `mf::cli` | Command-line parsing and the `run` driver. Compiled into `mflib` so module tests can run it in-process. |
| wasm adapter | none | `wasm/mf_wasm.cpp` (outside `src/`) | `mf::wasm` | embind adapter that only converts types and calls `io::tir_document::parse`, `io::load_tire_model`, `io::replace_value`, `io::convert_tir`, `tire_model::find_coordinate_system`, `checked_evaluate_batch` and `load_side_warning`; `inspect` returns the full identification, `convertModel(text, fileName, target)` returns the converted `.tir` text with the changed entries, `evaluate(text, inputs, uncombined)` takes raw inputs and returns the load-side warnings. Not part of `mflib`, not coverage-gated, built only by the `wasm` preset. |
| app | none | `app/main.cpp` | none | One call into `mf::cli::run`. Builds the executable `mf`. |
| python adapter | none | `python/bindings/mf_python.cpp` | `mf::python` | pybind11 module, target `mf_python` -> `magic_formula._core`. Statically embeds `mflib`; not part of `mflib` and outside the gcovr gate by layout. |
| python layer | none | `python/src/magic_formula/` | none | `load_tir`, `parse_tir`, `TireModel` (with `find_coordinate_system`, `convert_coordinate_system`, `convert_inputs`, `convert_outputs`, `characteristic_channels`, `characteristics_batch`, `characteristics`), `CoordinateIdentification`, `AxisDecision`, `IndicatorReading`, `Channel`, `MagicFormulaError`, `MagicFormulaWarning`. Own 100% pytest-cov gate. |

The library target is `mflib` (alias `mf::mflib`), built from the sources listed explicitly in `src/CMakeLists.txt`. The public headers are the INTERFACE target `mf_headers` (alias `mf::headers`) in `include/CMakeLists.txt`; it carries the include directory and Eigen, and `mflib` links it PUBLIC. The CLI executable `mf` is defined in `src/app/CMakeLists.txt`. The test helpers are the INTERFACE target `mf::test_support` (`tests/support/CMakeLists.txt`), which provides `MF_TEST_DATA_DIR`. Eigen 5.0.1 is a PUBLIC dependency: it appears in the public signatures, so every consumer receives it through `mf::headers` and `mflib`. The web app (`web/`, React + Vite) is a consumer of the wasm adapter, not a layer of the library: it holds the `.tir` text, re-parses it on each evaluate call, and contains no tire equations. See the README, "Web frontend (WebAssembly)". The web app shows the identified axis system in a coordinate panel (`web/src/components/coordinates/`: `CoordinateSystemPanel` with the identified label, the indicator table, the warnings and the ISO / SAE toggle that converts the model's coefficients, and the lazy-loaded three.js `TireScene` that draws the convention). Errors are values (see "Options and error model"); only `mf::cli::run` catches exceptions.

## Class hierarchy

```
tire_model                  abstract; non-virtual interface (NVI) with 4 evaluate forms and
                            evaluate_characteristics[_batch]; characteristics_length();
                            pure virtual reference_input(); private pure virtual rebuild(),
                            compute_characteristics[_batch]();
                            non-virtual find_coordinate_system() (two overloads), loaded_side(),
                            transformed(), convert_coordinate_system()
  `-- mf5x_model            abstract; owns coefficients and forms; compute / compute_batch and
        |                   compute_characteristics[_batch] are final;
        |                   rebuild is final and calls the private pure virtual with_parameters()
        |-- pac2002_model   final; PAC2002; with_parameters final
        `-- mf52_model      final; MF 5.2 (FITTYP 6); with_parameters final
```

- **`tire_model`** is abstract. Its four public `evaluate` forms are non-virtual and own the error policy. Pure virtual: `name`, `input_length`, `output_length`, `layout`, `reference_input` (the input vector of the reference operating point: nominal load, zero slips and inclination, reference speed, and the nominal pressure for formulations with a pressure input), the private hooks `compute` (one point) and `compute_batch` (a block of points), and the private `rebuild(sign_transforms, limits)` (a new model of the same formulation with transformed coefficients). Non-virtual, written once on the base (Template Method): `find_coordinate_system()` (default indicator table) and `find_coordinate_system(span<indicator_definition>)`, `loaded_side()`, `transformed(sign_transforms)` and `convert_coordinate_system(target)`, which returns a `model_conversion{model, applied, identification}`. It stores the limits vector, the `model_metadata`, the `model_options` and the per-point input and output lengths given to its protected constructor (they size the evaluate views without a virtual call), exposes `limits`, `metadata` and `options`, and is immutable after construction, so concurrent `evaluate` calls are safe. Models are built through static `create` functions that return `std::expected`, not through public constructors.
- **`tire_model` characteristics.** `characteristics_length()` (= `layout().characteristics.size()`), `evaluate_characteristics` (one point) and `evaluate_characteristics_batch` are non-virtual and own the error policy exactly like the evaluate forms: unchecked unless `throw_if_model_failure`, then `validate_input(s)`, the compute hook and `check_characteristics[_batch]`. The private pure virtual hooks are `compute_characteristics` and `compute_characteristics_batch` (no checks, no allocation). Interface Segregation: the characteristics vector is a second, separate result vector, so the output vector and every existing consumer are unchanged.
- **`mf5x_model`** is abstract. It owns an `mf5x_coefficients` (117 doubles named like the `.tir` keys) and an `mf5x_forms`. `input_length`, `output_length`, `reference_input` (FNOMIN*LFZO, zero slips and inclination, LONGVL), `compute` and `compute_batch` are final: the evaluation order is fixed and runs `detail::mf5x_pipeline` on lanes. `compute_characteristics` and `compute_characteristics_batch` are final too and run `detail::mf5x_characteristics` on the same lanes; `pac2002_model` and `mf52_model` are unchanged by the feature. `rebuild` is final: it applies `mf5x::transform_coefficients` and calls the private pure virtual `with_parameters(mf5x_parameters)`, which the two concrete models implement by constructing a model of their own type (forms, options and metadata are copied, not recomputed; the flips preserve every validated invariant, so `create` is not re-run). It aliases `input_index`, `output_index` and `limit_index` from `mf::mf5x`.
- **`pac2002_model`** and **`mf52_model`** are final. They differ only in `name`, `layout` (the Vx domain), `create`, `with_parameters` and the `mf5x_forms` they pass up (see "MF 5.x pipeline").
- **`mf5x_parameters`** = coefficients + limits vector + `model_metadata`. `mf5x_model::validate_parameters` runs `validate_coefficients` and then `validate_limits(limits, mf5x::LIMITS)` and returns the first error unchanged.

Free functions (all in `mf`):

| Function | Header | Role |
|----------|--------|------|
| `validate_input`, `validate_inputs` | `model_checks.hpp` | size, finite, domain of every input channel (one point, a matrix) |
| `check_output`, `check_outputs` | `model_checks.hpp` | size, finite results (one point, a matrix) |
| `check_buffers`, `check_batch_buffers` | `model_checks.hpp` | null pointers and lengths of the raw forms |
| `checked_evaluate_batch` | `model_checks.hpp` | `validate_inputs`, `evaluate_batch`, `check_outputs`; used by the CLI |
| `check_characteristics`, `check_characteristics_batch` | `model_checks.hpp` | size, then finite values; NaN passes only in a channel with `nan_allowed` |
| `checked_evaluate_characteristics_batch` | `model_checks.hpp` | `validate_inputs`, `evaluate_characteristics_batch`, `check_characteristics_batch`; used by `checked_evaluate_batch_with_characteristics` and the Python adapter |
| `checked_evaluate_batch_with_characteristics` | `model_checks.hpp` | `checked_evaluate_batch`, then `checked_evaluate_characteristics_batch`; first failure wins; returns `checked_batch` (outputs and characteristics); used by the CLI |
| `contains`, `negates`, `sign_transform_name` | `sign_transform.hpp` | the `sign_transform` enum (`NONE`, `VERTICAL`, `LATERAL`), the `sign_transforms` set and the test whether a plan negates a channel |
| `apply_sign_transforms`, `transform_limits`, `limit_signs_well_formed` | `model_layout.hpp` | exact sign flips of the channels (and negate-and-swap of the limit pairs) that declare `negated_by` in the set; self-inverse. An overload takes `characteristic_channel`s and a matrix (NaN stays NaN) |
| `convert_characteristics` | `coordinate_system.hpp` | data conversion of a characteristics matrix between two systems (never used inside evaluation) |
| `select_lanes`, `sgn_plus` | `detail/lanes.hpp`, `detail/dual_lane.hpp` | element-wise select and the sign that is +1 at +-0; each has a plain and a `dual_lane` overload |
| `ellipse_force` | `detail/mf5x_kernels.hpp` | the friction-ellipse force F0*r; plain and dual overloads (the dual one carries the 0/0 limit rule) |
| `first_stationary_point` | `detail/stationary_point.hpp` | the first root of an analytic slope on a segment (scan plus bisection) |
| `mf5x_planar_forces`, `seed_alpha`, `seed_kappa`, `peak_search_end`, `mf5x_characteristics` | `detail/mf5x_characteristics.hpp` | the forces of one point with their pure-slip terms, the dual-seeded inputs, the clipped search segment, and the eight characteristics |
| `transform_coefficients`, `COEFFICIENT_SIGNS` | `mf5x_signs.hpp` | the MF 5.x coefficient table (15 VERTICAL + 15 LATERAL) and the flip of an `mf5x_coefficients` |
| `coordinate_system_name`, `parse_coordinate_system`, `data_transforms` | `coordinate_system.hpp` | `coordinate_system` is `ISO` or `SAE`; the data transforms between two systems |
| `convert_inputs`, `convert_outputs` | `coordinate_system.hpp` | data conversion utility (never used inside evaluation): FZ on the inputs, FY, MZ and MY on the outputs |
| `census_fz`, `load_side_warning` | `coordinate_system.hpp` | FZ sign counts; the one warning for rows on the unloaded side of a system |
| `make_reading`, `decide_axis`, `effective_signs`, `classify_system`, `plan_conversion`, `assemble_identification`, `identification_warnings` | `coordinate_identification.hpp` | the pure functions of the identification; `IDENTIFICATION_INDICATORS` is the data-driven indicator table |
| `plan_probes`, `read_probes` | `detail/identification_probes.hpp` | the probe matrix of an indicator table for a layout, and the readings from the evaluated outputs |
| `convert_tir`, `negate_number_text` | `io/tir_conversion.hpp` | `.tir` text conversion to a target system |
| `default_limits`, `validate_limits`, `limit_pairs_well_formed` | `model_layout.hpp` | limits vector from a layout; LOWER <= UPPER check; layout well-formedness |
| `make_error`, `to_string` | `error.hpp` | error construction and formatting |

## Coordinate systems

ISO is x forward, y left, z up (a loaded tire has FZ > 0 and dFy/dSA > 0). SAE is x forward, y right, z down (FZ < 0 and dFy/dSA < 0). SA, SR and IA have the same definition in both. **The equations never change and the evaluation never converts**: raw inputs go straight in. A coefficient set belongs to one system, and converting a model between systems means flipping coefficient signs. The definitions, the flip tables and the identification rationale are in [`research/coordinate-systems.md`](research/coordinate-systems.md); the project choices are in `research/policies.md` section 8.

**Sign transforms (data on the layout).** `sign_transform` is `NONE`, `VERTICAL` (ISO <-> SAE) or `LATERAL` (the slip-angle convention); `sign_transforms{vertical, lateral}` is a plan. Every input channel, output channel and limit pair of a `model_layout` declares `negated_by`, the one transform that negates it (MF 5.x: FZ VERTICAL, SA LATERAL; FY, MZ, MY VERTICAL; FZMIN/FZMAX VERTICAL; ALPMIN/ALPMAX LATERAL). `apply_sign_transforms` negates the declared columns; `transform_limits` turns a transformed pair into (-upper, -lower). Both are exact and self-inverse, and the two transforms commute. The per-coefficient table of a family is data too: `mf5x::COEFFICIENT_SIGNS` (key, member pointer, transform), with `transform_coefficients`. Adding a quantity or a family adds rows, not code.

**Equations.** Three sign-agnostic generalisations make the property `evaluate(transformed(T), T_in(x)) == T_out(evaluate(x))` hold: FNOMIN must be non-zero, the unloaded rule is `fz*sgn(FNOMIN) > 0` (one `copysign` per pipeline call), and the PAC2002 shift-form My carries `sgn(FNOMIN)`. With FNOMIN > 0 everything reduces to the earlier behaviour.

**Data conversion.** `convert_inputs` and `convert_outputs` (`coordinate_system.hpp`) apply the data transforms between two systems: FZ flips on the inputs and FY, MZ and MY on the outputs; SA, SR, IA, VX, FX, MX and the flags stay. They are a boundary utility for the CLI, the UI and Python.

**Identification.** `tire_model::find_coordinate_system()` is a Template Method: the base class owns the algorithm and the formulation supplies only `reference_input()` (FZ = FNOMIN*LFZO with its sign). The steps:
1. `detail::plan_probes` turns the indicator table (`IDENTIFICATION_INDICATORS`, 11 rows: FZ0, dFy/dIA and My on the vertical axis; dFy/dSA, the Fy secants at 1, 3 and 6 degrees, the Fy odd part at 20 degrees and dMz/dSA on the lateral axis; dFx/dSR and the Fx secant at 5 % as checks) into one probe matrix at 0.5, 1 and 1.5 times F'z0 (about 55 rows).
2. One unchecked `compute_batch` evaluates it; the member is total and never throws `model_failure`.
3. `detail::read_probes` produces one `indicator_reading` per slot; a reading is a vote, below the threshold, not finite, clamped or on a missing channel (`make_reading`).
4. `decide_axis` takes a weighted vote per axis (minimum valid weight 0.25, minimum confidence 0.6); `effective_signs` lets an undetermined axis take the other axis's sign (ISO when both are undetermined); `classify_system` gives ISO, SAE or MIXED; `identification_warnings` reports undetermined axes, dissent, MIXED and failed checks.

All decisions live in these pure functions, so the member itself has no branches. The result `coordinate_identification` exposes every reading, both axis decisions, the effective signs, the classification and the warnings.

**Model conversion.** `transformed(plan)` calls `rebuild(plan, transform_limits(limits(), layout().limits, plan))`; `convert_coordinate_system(target)` identifies the model, plans with `plan_conversion` ((+,-) to ISO is {LATERAL}; ISO <-> SAE is {VERTICAL}; and so on) and returns the transformed copy with the plan and the source identification. `loaded_side()` is SAE when the reference FZ is negative. `io::convert_tir` does the same on `.tir` text: it parses, loads, plans, then splices the negated value texts at the entries' value offsets; the converted text loads to a model whose coefficients equal `transformed(applied)` value for value (signed zeros may differ: zero values are left as written, +0.0, where `transformed()` gives -0.0).

**Deleted.** `mf::solve` and the v1 `solve_result` no longer exist (`solver.hpp`, `solver.cpp` and their unit test are removed): each front end calls `checked_evaluate_batch` and `load_side_warning` itself, so the evaluation API carries no system option.

## Index-mapped inputs, outputs and limits

A model does not return a struct. It maps an input vector to an output vector, and the position of every value is named by an index enum of the model family. The MF 5.x family (PAC2002 and MF 5.2) uses the unscoped enums of `mf::mf5x` (`include/magic_formula/mf5x_layout.hpp`), exposed on the model classes as `pac2002_model::input_index`, `output_index` and `limit_index`:

| Enum | Positions (index 0 first) |
|------|---------------------------|
| `input_index` | FZ, SA, SR, IA, VX (5) |
| `output_index` | FX, FY, MZ, MX, MY, FZ_CLAMPED, SR_CLAMPED, SA_CLAMPED, IA_CLAMPED, BELOW_VXLOW (10) |
| `limit_index` | FZMIN, FZMAX, KPUMIN, KPUMAX, ALPMIN, ALPMAX, CAMMIN, CAMMAX, VXLOW (9) |

The five flag outputs hold 0.0 or 1.0. The enums are unscoped on purpose: an enumerator is the vector position and is usable directly as an index (`y(output_index::FX)`, `data[FX]`) with no cast. The family namespace scopes the names, and every enumerator name is unique across the three enums.

The descriptor tables of `model_layout.hpp` give the same positions as data, so generic code (checks, the CLI, the writer) needs no per-model knowledge. `model_layout` holds three spans:

- `input_channel`: `name` (also the table column), `unit`, the domain `minimum` with `minimum_inclusive` (PAC2002 VX >= 0, MF 5.2 VX > 0, everything else unrestricted) and `negated_by`.
- `output_channel`: `name`, `unit` (flags use "-") and `negated_by` (flags `NONE`).
- `limit_channel`: the `.tir` `key`, a `limit_role` (`LOWER`, `UPPER` or `VALUE`; a LOWER is always directly followed by its UPPER) the `default_value` (-inf for LOWER, +inf for UPPER, 1 m/s for VXLOW) and `negated_by` (the transform under which a LOWER/UPPER pair becomes (-upper, -lower); `NONE` for VALUE rows).

A fourth table, `characteristic_channel` (`name`, `unit`, `negated_by`, `nan_allowed`), describes the characteristics vector; see "Characteristics". The enum `characteristic_index` (DFY_DSA, DFX_DSR, FY_OVER_SA, FX_OVER_SR, DY, DX, SA_AT_PEAK_FY, SR_AT_PEAK_FX) names its positions; `layout.characteristics` is empty for a formulation without them.

The CSV result header has 23 columns, the input channels, the output channels, then the characteristic channels:

```
FZ[N],SA[rad],SR[-],IA[rad],VX[m/s],FX[N],FY[N],MZ[N*m],MX[N*m],MY[N*m],FZ_CLAMPED[-],SR_CLAMPED[-],SA_CLAMPED[-],IA_CLAMPED[-],BELOW_VXLOW[-],DFY_DSA[N/rad],DFX_DSR[N],FY_OVER_SA[N/rad],FX_OVER_SR[N],DY[N],DX[N],SA_AT_PEAK_FY[rad],SR_AT_PEAK_FX[-]
```

## Characteristics

Eight derived quantities of the evaluated force curves, returned by `evaluate_characteristics[_batch]` in the order of `characteristic_index`. All are analytic: derivatives come from forward-mode automatic differentiation of the unchanged closed-form kernels (exact chain rule, no step size, no finite difference). The derivation, the policies and the test values are in [`research/characteristics.md`](research/characteristics.md).

| Channel | Unit | Meaning | Sign parity |
|---------|------|---------|-------------|
| `DFY_DSA` | N/rad | dFy/dSA of the evaluated force at the row | VERTICAL and LATERAL |
| `DFX_DSR` | N | dFx/dSR of the evaluated force at the row | none |
| `FY_OVER_SA` | N/rad | secant: evaluated Fy over the raw SA | VERTICAL and LATERAL |
| `FX_OVER_SR` | N | secant: evaluated Fx over the raw SR | none |
| `DY` | N | pure-slip peak factor Dy | VERTICAL |
| `DX` | N | pure-slip peak factor Dx | none |
| `SA_AT_PEAK_FY` | rad | first stationary point of Fy(SA) | LATERAL |
| `SR_AT_PEAK_FX` | - | first stationary point of Fx(SR) | none |

Rules:

- **Combined and pure slip.** The slopes are those of the force the model evaluates (combined or uncombined, per `slip_mode`) at the row's other inputs; `DY` and `DX` are the pure-slip D parameters.
- **Unloaded and clamped rows.** A row without contact gives 0 in the six non-peak channels and NaN in the two peak channels. A slip clamped to the `.tir` range has slope 0 for that channel (the chain rule through the clamp).
- **Secant.** Evaluated force over the raw slip. At raw slip +-0 the secant is the row's tangent slope (the analytic derivative at slip 0), not NaN; without contact it is 0.
- **NaN policy.** NaN appears only in `SA_AT_PEAK_FY` and `SR_AT_PEAK_FX` (the channels with `nan_allowed`): no stationary point in the search segment, or no contact. Any other non-finite value is a NUMERICAL error, like the outputs.
- **Derivative rules.** Chain rule through the clamps and the contact masks; at a genuine kink (where the function is not differentiable) the right derivative; at the friction-ellipse 0/0 points the true limit (see below). Force values stay bit-identical to the evaluate path.
- **Peak slips.** The exact stationarity equation dF/ds = 0 with the analytic slope: side = `sgn_plus` of the row's slip, the segment is [0, end] with end = +-pi/2 rad for SA and +-1 for SR, clipped to the `.tir` slip range; a scan of 32 cells for the first sign change of the slope, then 52 bisections; NaN if none. The trip counts are fixed, so the code is branch-free on data.

**Dual lanes.** `detail::dual_lane<Lane_T>` holds a value lane and a derivative lane. Invariant: the value lane of every operation equals the plain-lane expression bit for bit, so the dual kernels reproduce the forces exactly (tests assert it). Instantiations: `dual_lane<point_lane>` and `dual_lane<block_lane>`. The kernels stay lane-generic: arithmetic plus the member math `dual_lane` provides (atan, sin, cos, exp, sqrt, abs, min) and `select_lanes` with named concrete lanes.

**Plain/dual overload pattern.** `select_lanes` (in `detail/lanes.hpp`) and `sgn_plus` (in `detail/dual_lane.hpp`) have generic plain and `dual_lane` overloads: piecewise rules whose dual overload selects value and derivative (`select_lanes`) or has derivative 0 (`sgn_plus`). Where a derivative at a point is a limit rather than a chain rule (today only `ellipse_force`), the family kernel header has a plain/dual overload pair. The plain overload is the exact plain expression and is all the hot path (`compute`, `compute_batch`) ever instantiates; the dual overload changes only the derivative lane.

**Friction-ellipse 0/0.** The ellipse ratio r = |P|/sqrt(P^2 + Q^2) is 0/0 wherever P = Q = 0 (P = own shifted slip times the other maximum, Q = own actual magnitude times the other slip). With zero shifts this is every row whose own slip is 0, i.e. every pure-slip row of the other axis, not only the origin. The evaluated value there is defined as 1 (force = pure force). The force slope is the pure slope times the directional limit of the ratio along the seeded slip, |P'|/sqrt(P'^2 + Q'^2), or 1 when both P' and Q' are 0.

## The four evaluate forms

All four are members of `tire_model`. Inputs are in the model's `input_index` order, outputs in its `output_index` order. A batch has one row per point and one column per input index.

| Form | Signature | Allocates |
|------|-----------|-----------|
| per point, Ref | `Eigen::VectorXd evaluate(const Eigen::Ref<const Eigen::VectorXd> &input)` | the returned vector |
| per point, raw | `void evaluate(const double *input, int input_len, double *data, int len)` | never |
| batch, Ref | `Eigen::MatrixXd evaluate_batch(const Eigen::Ref<const Eigen::MatrixXd> &inputs)` | the returned matrix |
| batch, raw | `void evaluate_batch(const double *inputs, int rows, double *data, int len)` | never |

The raw batch buffers are column-major (the layout of an `Eigen::MatrixXd`): `inputs[c*rows + r]` is input `c` of point `r`, `data[c*rows + r]` is output `c` of point `r`, and `len` is `rows * output_length()`. The raw forms never allocate (the hooks work on `Eigen::Map` views and stack-only lanes), so they are safe in real-time loops. Buffers must not overlap. Numerics contract: the batch results use vectorised math and agree with the per-point results within a 1e-12 scaled relative tolerance.

## Options and error model

`model_options{mode, throw_if_model_failure}` is fixed at construction (the loader and the `create` functions take it):

- `mode` is the `slip_mode`: `COMBINED` (default) or `UNCOMBINED` (pure-slip Fx0, Fy0, Mz0). The evaluate signatures carry no mode argument.
- `throw_if_model_failure` is false by default. Off, the evaluation path performs no check at all: no size test, no finite test, no domain test. On, each form runs, in order, `check_buffers` (raw forms), `validate_input(s)`, `compute`, `check_output(s)`, and throws `mf::model_failure{cause, row}` on the first failure (`row` is empty for a single point).

The free checks of `model_checks.hpp` are public and return `std::expected`, so a caller can validate explicitly with a model built without the flag. `checked_evaluate_batch` combines them around `evaluate_batch` and is what the CLI uses; input errors are reported before numerical errors.

- Recoverable problems are values: `std::expected<T, mf::error>`, or `std::expected<T, mf::batch_error>` (row index + error) for batches. `mf::error` = `error_code` + message + `location` (file, 1-based line, key). Codes: `IO`, `SYNTAX`, `INVALID_NUMBER`, `DUPLICATE_KEY`, `MISSING_KEY`, `INVALID_VALUE`, `UNSUPPORTED_UNIT`, `UNSUPPORTED_MODEL`, `NOT_IMPLEMENTED`, `INVALID_ARGUMENT` (wrong size, null buffer, bad limits vector), `INVALID_INPUT` (non-finite or out-of-domain input, naming the channel), `NUMERICAL` (non-finite output, naming the channel).
- Warnings are data (`mf::warning`), returned by the loader; table-level warnings are `mf::batch_warning` (row index + warning), returned by `load_side_warning`; the identification warnings come from `identification_warnings`. The library never prints. The CLI prints them as `mf: warning: ...`.
- `find_coordinate_system`, `transformed`, `convert_coordinate_system` and `loaded_side` are total: the identification carries its diagnostics (undetermined axes, warnings) as data and never fails. `io::convert_tir` returns `std::expected` and passes load errors through unchanged.
- Kernels, `compute` and `compute_batch` are total and `noexcept`; bad numerics surface through `check_output(s)`.
- `mf::model_failure` is the one library exception besides `std::bad_alloc`, and only the `evaluate*` functions of a model built with the flag throw it. `mf::cli::run` is the single place that catches exceptions.

## MF 5.x pipeline

`mf5x_model::compute` / `compute_batch` run `detail::mf5x_pipeline` (`include/magic_formula/detail/mf5x_kernels.hpp`), the same for every MF 5.x model:

1. `mf5x_condition`: the VXLOW flag (`Vx < VXLOW`, Vx is never modified); the contact mask (raw Fz and clamped Fz on the loaded side: `fz*sgn(FNOMIN) > 0`); branch-free clamps of Fz, SR, SA and IA to their limits (an infinite bound never moves a value); the clamp flags, masked by raw contact (a point on the unloaded side reports 0.0).
2. `mf5x_evaluate`: the slip state (`evaluate_slip_state`), then the forms. Combined mode: the combined forces, the equivalent slip angles, Mz, Mx and My. Uncombined mode: Fx0, Fy0 and Mz0, with Mx and My.
3. `contact.select(force, +0.0)` for Fx, Fy, Mz, Mx and My: a point without contact has exactly +0.0 forces and moments. The flags are copied from the conditioning.

The echoed inputs are never clamped; only the model's internal copy is. Clamping, the unloaded zero rule and the flags stay on the hot path as selects, with no branch.

The PAC2002 / MF 5.2 differences are the closed set `mf5x_forms` (`mf5x_forms.hpp`), chosen at construction (see `research/pac2002-vs-mf52.md`):

| Form | PAC2002 | MF 5.2 |
|------|---------|--------|
| `trail_cosine` | `COS_ALPHA`: cos(alpha) | `COS_PRIME`: cos'(alpha) = Vx/(Vc + eps) |
| `combined` | `COSINE`, or `FRICTION_ELLIPSE` when the file has no R* keys (chosen by the loader) | `COSINE` |
| `fy_prime` | `SVYK`: F'y = Fy - SVyk | `SVYK0`: F'y = Fy - SVyk0 (mu_y at zero camber) |
| `rolling_resistance` | `QSY`, or `SHIFTS` iff FITTYP is 5 and QSY1 = QSY2 = 0 | `QSY` |
| Vx domain | Vx >= 0 | Vx > 0 |
| `create` | takes the `combined_method` | forces LGAX = LGAY = LGAZ = 1 |

## Lanes and performance

The kernels are templates over a lane type, defined in headers (`detail/lanes.hpp`, `detail/mf5x_kernels.hpp`) and instantiated on two lanes:

- `detail::point_lane`: a 1x1 array. Scalar code with the libm functions; it is the reference the batch path is compared against (1e-12 scaled relative tolerance).
- `detail::block_lane`: a column of up to `BLOCK_ROWS` = 32 points with a fixed maximum size, so it lives on the stack and `compute_batch` never allocates. It is the SIMD path.

`compute_batch` walks the rows in blocks of `BLOCK_ROWS`. Each input and output column of a block is a structure-of-arrays lane, so the arithmetic runs on contiguous data. Eigen 5.0.1 vectorises these functions for double: `atan`, `exp`, and `sin` / `cos` (the latter two with `EIGEN_FAST_MATH`). `tan` and `hypot` stay scalar (element-wise loops). A batch result therefore differs from the per-point result by a few ULP; the tests compare them at a 1e-12 scaled relative tolerance and the CLI reference outputs at their printed precision. Consumers that instantiate the kernels must compile with the same SIMD flags as `mflib`.

## Loading a `.tir` file

`io::load_tire_model` (`include/magic_formula/io/model_loader.hpp`) is the factory. In order: `check_units` (SI only) -> `identify_model` (routing) -> bind the coefficient keys of `mf5x_common_bindings()` (plus the camber factors for PAC2002) with their `binding_role` (REQUIRED, SCALING, COEFFICIENT, COMBINED) -> R* rule -> LONGVL -> the limits vector in the index order of `mf5x::LIMITS` (FZ, KPU, ALP, CAM ranges, then VXLOW) -> `[MODEL]` metadata -> MF 5.2 dimension keys -> `pac2002_model::create` or `mf52_model::create` with the `model_options` -> warnings. The result is a `loaded_model`: `std::unique_ptr<const tire_model>`, warnings as data, and the ignored keys.

Routing (`io::identify_model`) maps PROPERTY_FILE_FORMAT and FITTYP to `io::model_kind` (`PAC2002`, `MF52`); see `research/model-routing.md` for the table.

## Input table

The input table has three parts: a header of column names, then a mandatory unit line with one unit per column, then the data rows. Comments (`#`) and blank lines are skipped everywhere, and one separator rule (any run of commas, blanks or tabs) applies on every line.

`src/io/input_table.cpp` parses it as a linear pipeline:

1. strip a leading UTF-8 BOM;
2. `next_content_line`: a cursor over content lines (CR stripped, blank and `#` lines skipped);
3. `parse_header`: names only; a field with `[` or `]` (the legacy `NAME[unit]` form) gives SYNTAX;
4. `next_content_line` again: no line left means a missing unit line, reported at the header line;
5. `parse_unit_line`: a data row in its place, the unit count, then the unit lookup per column;
6. `parse_rows`: numbers times the unit factor, giving SI values.

| Condition | Code | Line |
|-----------|------|------|
| legacy `NAME[unit]` header field | SYNTAX | header |
| text ends after the header | SYNTAX | header |
| line after the header starts with a number | SYNTAX | that line |
| unit count != column count (an empty entry included) | SYNTAX | unit line |
| unit not accepted for its column | UNSUPPORTED_UNIT | unit line (key = column) |

The other errors (missing header line, unknown column, DUPLICATE_KEY, MISSING_KEY, row field count, INVALID_NUMBER) are unchanged. A file reports its first error only.

The constexpr `UNITS` table in the parser is the single source of the accepted units and factors, and of the unit hints in the messages, so a new unit is one row. The doc comment of `mf::io::parse_input_table` is the specification.

## CLI flow

`mf::cli::run`:

1. parses the command line (`--system=iso|sae`, `--table-system=iso|sae`; the last occurrence wins; a bad value is a usage error);
2. loads the `.tir` file unchecked (`throw_if_model_failure` off) and identifies it (`find_coordinate_system()`);
3. with `--system`, converts the model (`convert_coordinate_system`, coefficient flips, only when its identified system differs); without it the model is evaluated as loaded, with no conversion;
4. identifies the evaluated model and takes its `loaded_side()`;
5. `io::read_model_inputs`: reads the table (header, mandatory unit line, rows; units converted to SI once) and arranges its columns in the model's `input_index` order; a table column that the model has no input for (P for MF 5.x) goes to `ignored_columns`;
6. the table system is `--table-system` or the evaluated model's loaded side; when it differs, `convert_inputs` flips a copy of the table to the loaded side (the data conversion);
7. `checked_evaluate_batch_with_characteristics` evaluates the raw values with `checked_evaluate_batch`; when that holds a value (an explicit `has_value()` check, not a monadic chain) the same inputs go through the characteristics batch, and the result is a `checked_batch` struct carrying both; an error is relocated to the table file and line;
8. `convert_outputs` brings the results back to the table system, and `convert_characteristics` does the same for the characteristics;
9. diagnostics on `err`, in this order: load warnings, the identification warnings of the evaluated model (always), `--verbose` information (the source identification with every reading, the conversion applied, the table system), one warning per ignored column, the load-side warning of the table (`load_side_warning`), then one warning per flag output that is set;
10. `io::write_results`: the 23-column CSV on `out`, the inputs echoed as read, the outputs and the eight characteristics in the table system; NaN prints as `nan`.

Output is all-or-nothing: nothing reaches stdout unless every row evaluated successfully. Exit codes 0 / 1 / 2.

## Python bindings

**Layering.** core <- adapter <- Python layer <- notebook. The core knows nothing about Python; the adapter is one translation unit (`mf::python`) and the Python layer depends only on the `magic_formula._core` contract.

**Build.** `MF_BUILD_PYTHON` is OFF by default; `python/pyproject.toml` (scikit-build-core, source dir = repo root, Release, Conan toolchain) turns it on. `python/` is always configured (its sources stay in `clang-format_all`), but the `mf_python` target is defined only when `MF_BUILD_PYTHON` is ON. In that build only, `mflib` gets position-independent code and hidden visibility so the module can embed it. The module is built with pybind11's `NO_EXTRAS` (no LTO, no strip). The wheel ships one native file.

**Data flow.**
1. `load_tir` calls `_core.load_tir`, which runs the normal loader (`loaded_model`, unchecked) and moves it into a `Model`; a failure is returned as an `Error`.
2. `evaluate` broadcasts the five channels with NumPy rules and packs one Fortran-ordered `(N, 5)` float64 matrix, then calls `evaluate_batch`. `evaluate_batch` takes such a matrix without a copy (any other layout or dtype is converted once).
3. The adapter runs `checked_evaluate_batch` (or the shape check and `evaluate_batch` with `check=False`) and `load_side_warning` with the GIL released, then moves the resulting matrix into a capsule-owned `(N, 10)` array. There is no output copy; the flag outputs stay floats (0.0 or 1.0).

**Characteristics.** `Model.evaluate_characteristics_batch` (adapter) runs `checked_evaluate_characteristics_batch` (or the shape check and the unchecked form with `check=False`) with the GIL released and returns a capsule-owned `(N, 8)` array. `TireModel.characteristic_channels` lists the `Channel`s, `characteristics_batch(inputs, *, check=True)` takes the packed matrix, and `characteristics(*, check=True, **inputs)` broadcasts named channels and returns a dict keyed by the lower-case channel names. NaN appears only in `sa_at_peak_fy` and `sr_at_peak_fx`.

**Coordinate systems.** `TireModel.find_coordinate_system()` returns a `CoordinateIdentification` (classification, per-axis `AxisDecision`, every `IndicatorReading`, warnings); `convert_coordinate_system(target)` returns a new `TireModel` with flipped coefficients; `convert_inputs` / `convert_outputs` expose the data conversion. `evaluate_batch(inputs, *, check=True)` and `evaluate(*, check=True, **inputs)` evaluate raw values and re-emit the load-side warnings as `MagicFormulaWarning`.

**Error model.** The adapter returns errors as values (`Model | Error`) and never throws or catches; pybind11's dispatcher is its boundary. The Python layer raises `MagicFormulaError` with `code`, `message`, `file`, `line`, `key` and `row`. A wrong input shape is always `INVALID_ARGUMENT`, whatever `check` is; `check=False` skips input and output validation. Load warnings are emitted as `MagicFormulaWarning` and kept on `TireModel.warnings`.

**Performance.** One virtual dispatch per batch, the same lane-templated kernels as the C++ API, no input copy for F-ordered float64 input, no output copy, GIL released during evaluation. Models are immutable, so Python threads can evaluate chunks in parallel.

## Design patterns

| Pattern | Where | Why |
|---------|-------|-----|
| Template Method (with the NVI) | `find_coordinate_system`, `transformed`, `convert_coordinate_system` on `tire_model`; `mf5x_model::rebuild` with the second-level hook `with_parameters` | The algorithms are written once on the base; the formulation supplies only what it alone knows: its nominal point, its coefficient storage and its constructor. |
| Template Method (NVI hooks) | `evaluate_characteristics[_batch]` and the private `compute_characteristics[_batch]` | Error policy written once on the base; the formulation supplies only the unchecked computation. |
| Interface Segregation | the characteristics vector next to the output vector | Consumers that need only forces never see the characteristics. |
| Table-driven channels | `characteristic_channel` rows in `mf5x::CHARACTERISTICS` | Name, unit, sign parity and NaN permission are data; checks, conversion and the writer follow the table. |
| Type adapter (dual lane) | `detail::dual_lane` | The unchanged closed-form kernels yield exact derivatives when instantiated on it. |
| Compile-time Strategy | the slope callable of `first_stationary_point` | The root finder holds no tire equations; the caller passes the analytic slope. |
| Prototype-style rebuild | `rebuild` / `with_parameters` | A new immutable model with modified parameters, keeping forms, options and metadata, with no setters (immutability and thread safety stay). |
| Table-driven design (open/closed) | `negated_by` on channels and limit pairs, `mf5x::COEFFICIENT_SIGNS`, `IDENTIFICATION_INDICATORS` | The sign behaviour of a quantity is data, like its unit. A new family adds rows, not code, and each row is unit-testable. |
| Pure functions and lookup tables | the identification functions, `load_side_warning`, `plan_probes`, `read_probes`, `negate_number_text` | Every branch is reachable with literal inputs, so the model-dependent members have no branches. |
| Errors and warnings as values | identification diagnostics, `convert_tir`, `load_side_warning` | Matches the error model; only `cli::run` catches. |

## Extending to MF 6.x

MF 6.0 / 6.1 / 6.2 (FITTYP 60 / 61 / 62) are currently rejected with `UNSUPPORTED_MODEL` (`model-routing.md`, routing table row "any / 60, 61, 62, 70"). Thanks to the index-mapped interface, adding one touches the layout, one model class, the bindings and the routing. `tire_model`, the checks, the CLI column mapping and the result writer need no change.

1. **Research first.** Add a note (for example `docs/research/mf61.md`) with the equations, the coefficient list and a row-by-row difference table against MF 5.2, in the style of `pac2002-vs-mf52.md`, written as the project's own definitions like every note in `docs/research/` (see `research/README.md`). Record new project rules in `policies.md`.
2. **Layout namespace.** Add `mf::mf6x` (`mf6x_layout.hpp`) next to `mf::mf5x` with its own unscoped `input_index`, `output_index` and `limit_index` enums and the matching channel tables. The inputs append P (pressure) as input 5 after FZ, SA, SR, IA, VX; the limits add PRESMIN / PRESMAX (a LOWER / UPPER pair); the outputs add a P_CLAMPED flag. Enumerator names stay unique within the namespace.
3. **Coefficients.** MF 6.1 has many more coefficients than the 117 of `mf5x_coefficients`. Add a new struct (for example `mf61_coefficients` plus `mf61_parameters`) in its own header, with `validate_coefficients`-style checks (required keys, values the equations divide by). `model_metadata` and the limits machinery are reused.
4. **Model class.** Add `mf61_model final : public tire_model` (or an abstract `mf6x_model` if 6.0, 6.1 and 6.2 share an evaluation order). Implement `name`, `input_length`, `output_length`, `layout`, and the private `compute` and `compute_batch` with lane-templated kernels instantiated on `point_lane` and `block_lane`, reusing the `detail/lanes.hpp` primitives (`clamp_to_limits`, `sgn_plus`, `add_epsilon`, `to_flag`). Pass `INPUT_LENGTH` and `OUTPUT_LENGTH` to the protected `tire_model` constructor; they must equal `input_length()`, `output_length()` and the sizes of the layout (`model_contract_test` checks this). Add a static `create` returning `std::expected`. Reuse the existing kernels where the equations are unchanged. New kernels that compute one Magic Formula quantity are named after its symbol (cpp-conventions).
5. **CLI wording.** Add a flag wording row for P_CLAMPED to the CLI's flag-warning table. The column mapping and the writer follow the layout automatically, and the "column P is ignored" warning disappears for models whose layout has a P input.
6. **Binding table.** Add a binding table in `io/` next to `mf5x_bindings` (for example `mf61_bindings.hpp/.cpp`) with the key, the member pointer and the `binding_role` for every key, and a "foreign keys" list if needed (compare `mf52_foreign_keys()`).
7. **Routing.** Add `MF61` (and `MF60`, `MF62` as they are implemented) to `io::model_kind`, remove FITTYP 60 / 61 / 62 from the "not implemented" rule in `io::model_routing`, and add the new kind to the model-creation step of `model_loader.cpp`. FITTYP 70 stays unsupported until the temperature-and-velocity model exists. Keep the rule order: an unknown PFF first, a non-integer FITTYP second. Update `model-routing.md`, the routing-order rule in `research/policies.md` section 3 and the README "Models" section in the same PR.
8. **Open items (to be resolved by research before implementation).** The pressure unit and name (the unit line accepts only Pa for P today; another pressure unit is one row of the parser's unit table), whether and how pressure is clamped to PRESMIN / PRESMAX, and the value to use when the table has no P column.
9. **Tests to add.** 100% line and branch on core, nothing skipped:
   - unit tests for each new kernel and for the coefficient validation (every branch), on both lane types, with every `select` predicate driven both ways;
   - the batch-vs-point agreement test at the 1e-12 scaled tolerance, and the pipeline tests (flags, the unloaded rule, clamping, finite checks) for the new model;
   - layout tests (enum positions match the channel tables), routing tests (FITTYP 61 routes to the new model, 70 still gives `UNSUPPORTED_MODEL`), loader tests for the binding table and the pressure range;
   - a module test and a CLI test with a 6.x `.tir` file and a P column that is used; update the existing test that expects 6.x to be rejected.
10. **Build registration.** Register every new header in `include/CMakeLists.txt` (`MF_PUBLIC_HEADERS`), every new source in `src/CMakeLists.txt` (`MF_LIB_SOURCES`) and every new test in `tests/unit/CMakeLists.txt` or `tests/module/CMakeLists.txt`. These are explicit, `LC_ALL=C` sorted lists with no globs; an unlisted file is not built and fails the registration check (`build-and-test` skill).
11. **Coordinate systems.** Keep the channel-name contract that the identification relies on: inputs `FZ`, `SA`, `SR`, `IA`; outputs `FX`, `FY`, `MZ`, `MY`; clamp flags named `*_CLAMPED`. Then, in order:
    1. Declare `negated_by` on every new input and output channel and limit pair (for example turn slip and Vy per research; pressure, PRESMIN/PRESMAX and the flags are `NONE`).
    2. Add an `mf6x::COEFFICIENT_SIGNS` table, derived from the equations and verified with the property pattern of `tests/unit/coordinate_conversion_test.cpp` (random coefficient sets, both transforms, round trip exact).
    3. Implement `rebuild` and `with_parameters` for the new model (or reuse an `mf6x_model` base that owns them).
    4. Select the coefficient table by model kind in `io::convert_tir`.
    5. Make `reference_input()` return the nominal operating point, including the nominal pressure when the layout has a P input.
    6. Append indicator rows to the table, or add a virtual `identification_indicators()` hook if a row must be family-specific (`find_coordinate_system(span)` already takes a table).
    Characteristics: add a characteristics table to the layout (`characteristic_channel` rows), implement the two hooks `compute_characteristics` and `compute_characteristics_batch`, and reuse `dual_lane` and `first_stationary_point`. Keep the kernels lane-generic (`select_lanes` with named concrete lanes, member math only). Where a derivative at a point is a limit rather than a chain rule, add a plain/dual overload pair in the family kernel header: the plain overload is the exact plain expression, the dual overload changes only the derivative lane.
12. **Python bindings.** They are layout-driven (channels, limits and output names come from the model layout), so they need no change; `evaluate(p=...)` becomes valid once the layout has a P input.
13. **Docs.** Update this file, `CLAUDE.md` and the README "Models" section in the same PR.
