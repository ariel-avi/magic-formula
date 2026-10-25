# Architecture

This note describes the code as it is. The equations it implements are in [`docs/research/`](research/README.md); the project rules (defaults, errors, numerical guards, choices between alternative forms) are in [`research/policies.md`](research/policies.md). The routing rules are in [`research/model-routing.md`](research/model-routing.md).

## Layers

Dependencies point downwards only. The core model knows nothing about files or the command line.

| Layer | Headers (`include/magic_formula/`) | Sources (`src/`) | Namespace | Role |
|-------|------|------|-----------|------|
| core model | `tire_model.hpp`, `model_layout.hpp`, `mf5x_layout.hpp`, `model_checks.hpp`, `mf5x_model.hpp`, `pac2002_model.hpp`, `mf52_model.hpp`, `mf5x_forms.hpp`, `mf5x_coefficients.hpp`, `evaluation_types.hpp`, `error.hpp` | `model/`, `error.cpp` | `mf`, `mf::mf5x` | Abstract formulation, the index maps, the two concrete models, the free check functions. No I/O, no printing. |
| detail | `detail/lanes.hpp`, `detail/mf5x_kernels.hpp`, `detail/text.hpp` | `detail/` | `mf::detail` | Internal, lane-templated equation kernels and helpers. Under `include/` so tests can include them; production code includes `detail/mf5x_kernels.hpp` only from `src/model/mf5x_model.cpp`. |
| io | `io/*.hpp` | `io/` | `mf::io` | `.tir` lexing (`tir_document`, `tir_reader`), unit check, model routing, key-to-coefficient bindings, the model loader (factory), the `.tir` text editor (`tir_editor`: `replace_value` changes only the bytes of one value, using each entry's `value_offset`/`value_length` span, and re-parses to check the result), the input-table parser (header, mandatory unit line, rows) and its mapping onto a model's input channels, the CSV result writer. |
| cli | `cli/cli.hpp` | `cli/cli.cpp` | `mf::cli` | Command-line parsing and the `run` driver. Compiled into `mflib` so module tests can run it in-process. |
| wasm adapter | none | `wasm/mf_wasm.cpp` (outside `src/`) | `mf::wasm` | embind adapter that only converts types and calls `io::tir_document::parse`, `io::load_tire_model`, `io::replace_value` and `checked_evaluate_batch`. Not part of `mflib`, not coverage-gated, built only by the `wasm` preset. |
| app | none | `app/main.cpp` | none | One call into `mf::cli::run`. Builds the executable `mf`. |
| python adapter | none | `python/bindings/mf_python.cpp` | `mf::python` | pybind11 module, target `mf_python` -> `magic_formula._core`. Statically embeds `mflib`; not part of `mflib` and outside the gcovr gate by layout. |
| python layer | none | `python/src/magic_formula/` | none | `load_tir`, `parse_tir`, `TireModel`, `Channel`, `MagicFormulaError`, `MagicFormulaWarning`. Own 100% pytest-cov gate. |

The library target is `mflib` (alias `mf::mflib`, every `src/**/*.cpp` except `src/app/`). Eigen 5.0.1 is a PUBLIC dependency of `mflib`: it appears in the public signatures, so every consumer links it through `mflib`. The web app (`web/`, React + Vite) is a consumer of the wasm adapter, not a layer of the library: it holds the `.tir` text, re-parses it on each evaluate call, and contains no tire equations. See the README, "Web frontend (WebAssembly)". Errors are values (see "Options and error model"); only `mf::cli::run` catches exceptions.

## Class hierarchy

```
tire_model                  abstract; non-virtual interface (NVI) with 4 evaluate forms
  `-- mf5x_model            abstract; owns coefficients and forms; compute / compute_batch are final
        |-- pac2002_model   final; PAC2002
        `-- mf52_model      final; MF 5.2 (FITTYP 6)
```

- **`tire_model`** is abstract. Its four public `evaluate` forms are non-virtual and own the error policy. Pure virtual: `name`, `input_length`, `output_length`, `layout`, and the private hooks `compute` (one point) and `compute_batch` (a block of points). It stores the limits vector, the `model_metadata`, the `model_options` and the per-point input and output lengths given to its protected constructor (they size the evaluate views without a virtual call), exposes `limits`, `metadata` and `options`, and is immutable after construction, so concurrent `evaluate` calls are safe. Models are built through static `create` functions that return `std::expected`, not through public constructors.
- **`mf5x_model`** is abstract. It owns an `mf5x_coefficients` (117 doubles named like the `.tir` keys) and an `mf5x_forms`. `input_length`, `output_length`, `compute` and `compute_batch` are final: the evaluation order is fixed and runs `detail::mf5x_pipeline` on lanes. It aliases `input_index`, `output_index` and `limit_index` from `mf::mf5x`.
- **`pac2002_model`** and **`mf52_model`** are final. They differ only in `name`, `layout` (the Vx domain), `create` and the `mf5x_forms` they pass up (see "MF 5.x pipeline").
- **`mf5x_parameters`** = coefficients + limits vector + `model_metadata`. `mf5x_model::validate_parameters` runs `validate_coefficients` and then `validate_limits(limits, mf5x::LIMITS)` and returns the first error unchanged.

Free functions (all in `mf`):

| Function | Header | Role |
|----------|--------|------|
| `validate_input`, `validate_inputs` | `model_checks.hpp` | size, finite, domain of every input channel (one point, a matrix) |
| `check_output`, `check_outputs` | `model_checks.hpp` | size, finite results (one point, a matrix) |
| `check_buffers`, `check_batch_buffers` | `model_checks.hpp` | null pointers and lengths of the raw forms |
| `checked_evaluate_batch` | `model_checks.hpp` | `validate_inputs`, `evaluate_batch`, `check_outputs`; used by the CLI |
| `default_limits`, `validate_limits`, `limit_pairs_well_formed` | `model_layout.hpp` | limits vector from a layout; LOWER <= UPPER check; layout well-formedness |
| `make_error`, `to_string` | `error.hpp` | error construction and formatting |

## Index-mapped inputs, outputs and limits

A model does not return a struct. It maps an input vector to an output vector, and the position of every value is named by an index enum of the model family. The MF 5.x family (PAC2002 and MF 5.2) uses the unscoped enums of `mf::mf5x` (`include/magic_formula/mf5x_layout.hpp`), exposed on the model classes as `pac2002_model::input_index`, `output_index` and `limit_index`:

| Enum | Positions (index 0 first) |
|------|---------------------------|
| `input_index` | FZ, SA, SR, IA, VX (5) |
| `output_index` | FX, FY, MZ, MX, MY, FZ_CLAMPED, SR_CLAMPED, SA_CLAMPED, IA_CLAMPED, BELOW_VXLOW (10) |
| `limit_index` | FZMIN, FZMAX, KPUMIN, KPUMAX, ALPMIN, ALPMAX, CAMMIN, CAMMAX, VXLOW (9) |

The five flag outputs hold 0.0 or 1.0. The enums are unscoped on purpose: an enumerator is the vector position and is usable directly as an index (`y(output_index::FX)`, `data[FX]`) with no cast. The family namespace scopes the names, and every enumerator name is unique across the three enums.

The descriptor tables of `model_layout.hpp` give the same positions as data, so generic code (checks, the CLI, the writer) needs no per-model knowledge. `model_layout` holds three spans:

- `input_channel`: `name` (also the table column), `unit`, and the domain `minimum` with `minimum_inclusive` (PAC2002 VX >= 0, MF 5.2 VX > 0, everything else unrestricted).
- `output_channel`: `name` and `unit` (flags use "-").
- `limit_channel`: the `.tir` `key`, a `limit_role` (`LOWER`, `UPPER` or `VALUE`; a LOWER is always directly followed by its UPPER) and the `default_value` (-inf for LOWER, +inf for UPPER, 1 m/s for VXLOW).

The CSV result header has 15 columns, the input channels then the output channels:

```
FZ[N],SA[rad],SR[-],IA[rad],VX[m/s],FX[N],FY[N],MZ[N*m],MX[N*m],MY[N*m],FZ_CLAMPED[-],SR_CLAMPED[-],SA_CLAMPED[-],IA_CLAMPED[-],BELOW_VXLOW[-]
```

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
- Warnings are data (`mf::warning`), returned by the loader; the library never prints. The CLI prints them as `mf: warning: ...`.
- Kernels, `compute` and `compute_batch` are total and `noexcept`; bad numerics surface through `check_output(s)`.
- `mf::model_failure` is the one library exception besides `std::bad_alloc`, and only the `evaluate*` functions of a model built with the flag throw it. `mf::cli::run` is the single place that catches exceptions.

## MF 5.x pipeline

`mf5x_model::compute` / `compute_batch` run `detail::mf5x_pipeline` (`include/magic_formula/detail/mf5x_kernels.hpp`), the same for every MF 5.x model:

1. `mf5x_condition`: the VXLOW flag (`Vx < VXLOW`, Vx is never modified); the contact mask (raw Fz > 0 and clamped Fz > 0); branch-free clamps of Fz, SR, SA and IA to their limits (an infinite bound never moves a value); the clamp flags, masked by raw contact (a point with raw Fz <= 0 reports 0.0).
2. `mf5x_evaluate`: the slip state (`evaluate_slip_state`), then the forms. Combined mode: the combined forces, the equivalent slip angles, Mz, Mx and My. Uncombined mode: Fx0, Fy0 and Mz0, with Mx and My.
3. `contact.select(force, +0.0)` for Fx, Fy, Mz, Mx and My: a point without contact has exactly +0.0 forces and moments. The flags are copied from the conditioning.

The echoed inputs are never clamped; only the model's internal copy is. Clamping, the Fz <= 0 zero rule and the flags stay on the hot path as selects, with no branch.

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

1. parses the command line;
2. loads the `.tir` file unchecked (`throw_if_model_failure` off);
3. `io::read_model_inputs`: reads the table (header, mandatory unit line, rows; units converted to SI once) and arranges its columns in the model's `input_index` order; a table column that the model has no input for (P for MF 5.x) goes to `ignored_columns`;
4. `checked_evaluate_batch`: input validation, the batch evaluation and the output check, on the whole table;
5. diagnostics on `err`: load warnings, `--verbose` information, one warning per ignored column, then one warning per flag output that is set (first table line and row count);
6. `io::write_results`: the 15-column CSV on `out`.

Output is all-or-nothing: nothing reaches stdout unless every row evaluated successfully. Exit codes 0 / 1 / 2.

## Python bindings

**Layering.** core <- adapter <- Python layer <- notebook. The core knows nothing about Python; the adapter is one translation unit (`mf::python`) and the Python layer depends only on the `magic_formula._core` contract.

**Build.** `MF_BUILD_PYTHON` is OFF by default; `python/pyproject.toml` (scikit-build-core, source dir = repo root, Release, Conan toolchain) turns it on. In that build only, `mflib` gets position-independent code and hidden visibility so the module can embed it. The module is built with pybind11's `NO_EXTRAS` (no LTO, no strip). The wheel ships one native file.

**Data flow.**
1. `load_tir` calls `_core.load_tir`, which runs the normal loader (`loaded_model`, unchecked) and moves it into a `Model`; a failure is returned as an `Error`.
2. `evaluate` broadcasts the five channels with NumPy rules and packs one Fortran-ordered `(N, 5)` float64 matrix, then calls `evaluate_batch`. `evaluate_batch` takes such a matrix without a copy (any other layout or dtype is converted once).
3. The adapter runs `checked_evaluate_batch` (or the unchecked `evaluate_batch`) with the GIL released, then moves the resulting matrix into a capsule-owned `(N, 10)` array. There is no output copy; the flag outputs stay floats (0.0 or 1.0).

**Error model.** The adapter returns errors as values (`Model | Error`) and never throws or catches; pybind11's dispatcher is its boundary. The Python layer raises `MagicFormulaError` with `code`, `message`, `file`, `line`, `key` and `row`. A wrong input shape is always `INVALID_ARGUMENT`, whatever `check` is; `check=False` skips input and output validation. Load warnings are emitted as `MagicFormulaWarning` and kept on `TireModel.warnings`.

**Performance.** One virtual dispatch per batch, the same lane-templated kernels as the C++ API, no input copy for F-ordered float64 input, no output copy, GIL released during evaluation. Models are immutable, so Python threads can evaluate chunks in parallel.

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
   - the batch-vs-point agreement test at the 1e-12 scaled tolerance, and the pipeline tests (flags, Fz <= 0, clamping, finite checks) for the new model;
   - layout tests (enum positions match the channel tables), routing tests (FITTYP 61 routes to the new model, 70 still gives `UNSUPPORTED_MODEL`), loader tests for the binding table and the pressure range;
   - a module test and a CLI test with a 6.x `.tir` file and a P column that is used; update the existing test that expects 6.x to be rejected.
10. **Python bindings.** They are layout-driven (channels, limits and output names come from the model layout), so they need no change; `evaluate(p=...)` becomes valid once the layout has a P input.
11. **Docs.** Update this file, `CLAUDE.md` and the README "Models" section in the same PR.
