# Architecture

This note describes the code as it is. The equations it implements are in [`docs/research/`](research/README.md); the project rules where the sources are silent are in [`research/policies.md`](research/policies.md). The routing rules are in [`research/model-routing.md`](research/model-routing.md).

## Layers

Dependencies point downwards only. The core model knows nothing about files or the command line.

| Layer | Headers (`include/magic_formula/`) | Sources (`src/`) | Namespace | Role |
|-------|------|------|-----------|------|
| core model | `tire_model.hpp`, `mf5x_model.hpp`, `pac2002_model.hpp`, `mf52_model.hpp`, `mf5x_coefficients.hpp`, `evaluation_types.hpp`, `error.hpp`, `detail/mf5x_kernels.hpp` | `model/`, `error.cpp` | `mf`, `mf::detail` | Abstract formulation, the two concrete models, the pure equation kernels. No I/O, no printing. |
| io | `io/*.hpp` | `io/`, `detail/text.cpp` | `mf::io` | `.tir` lexing (`tir_document`, `tir_reader`), unit check, model routing, key-to-coefficient bindings, the model loader (factory), the input-table parser, the CSV result writer. |
| cli | `cli/cli.hpp` | `cli/cli.cpp` | `mf::cli` | Command-line parsing and the `run` driver. Compiled into `mflib` so module tests can run it in-process. |
| app | none | `app/main.cpp` | none | One call into `mf::cli::run`. Builds the executable `mf`. |

The library target is `mflib` (alias `mf::mflib`, every `src/**/*.cpp` except `src/app/`). Errors are values (see "Error model"); only `mf::cli::run` catches exceptions.

## Class hierarchy

```
tire_model                  abstract; non-virtual Template Method evaluate()
  `-- mf5x_model            abstract; shared MF 5.x evaluation order (compute() is final)
        |-- pac2002_model   final; PAC2002
        `-- mf52_model      final; MF 5.2 (FITTYP 6)
```

- **`tire_model`** owns the `operating_limits` and `model_metadata` and exposes `evaluate`, `evaluate_batch`, `name`, `limits`, `metadata`. Derived classes supply `compute` (private pure virtual, called only with validated, clamped inputs and Fz > 0) and may refine `validate_input` (protected virtual). A model is immutable after construction, so concurrent `evaluate` calls are safe. Models are built through static `create` functions that return `std::expected`, not through public constructors.
- **`mf5x_model`** holds an `mf5x_coefficients` (117 doubles named like the `.tir` keys) and implements `compute` once, from the kernels in `mf::detail`, in the PAC2002 / MF 5.2 evaluation order. The differences between the two formulations are protected virtual hooks (see `research/pac2002-vs-mf52.md`):

  | Hook | Default (also MF 5.2 where noted) | `pac2002_model` | `mf52_model` |
  |------|-----------------------------------|-----------------|--------------|
  | `trail_cosine` (pure virtual) | none | cos(alpha) | cos'(alpha) = Vx/(Vc + eps) |
  | `combine` | cosine method (MF 5.2 uses it) | friction ellipse when the file has no R* keys | inherited |
  | `fy_prime` | F'y = Fy - SVyk | inherited | F'y = Fy - SVyk0 (mu_y at zero camber) |
  | `my` | the QSY1-QSY4 form, ISO sign (MF 5.2 uses it) | shift form -R0*(SVx + Kx*SHx) when FITTYP 5 and QSY1 = QSY2 = 0 | inherited |
  | `validate_input` | finite inputs, Vx >= 0 | inherited (accepts Vx = 0) | adds Vx > 0 |
  | `create` | | takes the `combined_method` chosen by the loader | forces LGAX = LGAY = LGAZ = 1 |

- **`mf5x_parameters`** = coefficients + `operating_limits` + `model_metadata`. `mf5x_model::validate_parameters` runs `validate_coefficients` and then `validate_limits` and returns the first error unchanged.

## Evaluation pipeline

`tire_model::evaluate` (`src/model/tire_model.cpp`) is the same for every model:

1. `validate_input`: every field finite (checked in the order FZ, SR, SA, IA, VX), Vx >= 0, plus the model's own rule. Failure: `INVALID_INPUT` naming the field.
2. `flags.below_vxlow = Vx < VXLOW`. The row is flagged, never modified.
3. Raw Fz <= 0 (including -0.0): return exactly zero outputs, `compute` is not called.
4. Clamp Fz, SR, SA and IA to their ranges when both bounds exist; each variable that actually moved sets its `*_clamped` flag.
5. Clamped Fz <= 0 (a validated Fz interval may end at or below zero): the same zero outputs are returned, with the flags set so far (including `fz_clamped`), and `compute` is not called (`src/model/tire_model.cpp`, "A validated Fz interval may still end at or below zero").
6. `compute(clamped_point, mode)`. For `mf5x_model`: pure-slip terms (`evaluate_slip_state`), then for combined mode `combine`, the equivalent slip angles alpha_eq, Mz, Mx, and `my` for My; for uncombined mode Fx0, Fy0 and Mz0.
7. Every output must be finite, otherwise `NUMERICAL` naming the first offending output. NaN or inf is never returned.

`evaluate_batch` loops over `evaluate` in input order and stops at the first failure, reporting its index (`batch_error`). It is the seam for a later vectorised (Eigen) path.

The outputs are Fx, Fy, Mz, Mx and My in the ISO W-axis system, plus the `input_flags`. The echoed inputs are never clamped; only the model's internal copy is.

## Loading a `.tir` file

`io::load_tire_model` (`include/magic_formula/io/model_loader.hpp`) is the factory. In order: `check_units` (SI only) -> `identify_model` (routing) -> bind the coefficient keys of `mf5x_common_bindings()` (plus the camber factors for PAC2002) with their `binding_role` (REQUIRED, SCALING, COEFFICIENT, COMBINED) -> R* rule -> LONGVL -> VXLOW and the four ranges -> `[MODEL]` metadata -> MF 5.2 dimension keys -> `pac2002_model::create` or `mf52_model::create` -> warnings. The result is a `loaded_model`: `std::unique_ptr<const tire_model>`, warnings as data, and the ignored keys.

Routing (`io::identify_model`) maps PROPERTY_FILE_FORMAT and FITTYP to `io::model_kind` (`PAC2002`, `MF52`); see `research/model-routing.md` for the table.

## CLI flow

`mf::cli::run` parses the command line, loads the `.tir` file, reads the input table (`io::read_input_table`, units converted to SI once), evaluates every row, and only then writes the CSV (`io::write_results`). Diagnostics go to `err`, results to `out`; the P column is read and ignored with one warning. Exit codes 0 / 1 / 2.

## Error model

- Recoverable problems are values: `std::expected<T, mf::error>`. `mf::error` = `error_code` + message + `location` (file, 1-based line, key). Codes: `IO`, `SYNTAX`, `INVALID_NUMBER`, `DUPLICATE_KEY`, `MISSING_KEY`, `INVALID_VALUE`, `UNSUPPORTED_UNIT`, `UNSUPPORTED_MODEL`, `NOT_IMPLEMENTED`, `INVALID_INPUT`, `NUMERICAL`.
- Warnings are data (`mf::warning`), returned by the loader; the library never prints. The CLI prints them as `mf: warning: ...`.
- Kernels and `compute` are total and `noexcept`; bad numerics surface through the finite check in `evaluate`.
- Exceptions are only for resource exhaustion and stream failures; `mf::cli::run` is the single place that catches them.

## Extending to MF 6.x

MF 6.0 / 6.1 / 6.2 (FITTYP 60 / 61 / 62) are currently rejected with `UNSUPPORTED_MODEL` (`model-routing.md`, routing table row "any / 60, 61, 62, 70"). Adding one of them touches these places and nothing in the pipeline of `tire_model::evaluate`:

1. **Research first (rule 4 in `CLAUDE.md`).** Have the researcher add a cited note (for example `docs/research/mf61.md`) with the equations, the coefficient list, and a row-by-row difference table against MF 5.2, in the style of `pac2002-vs-mf52.md`. Record new project rules (items where the sources are silent) in `policies.md`. Primary sources for 6.x are not in the repository yet, so the provenance of every equation must be stated.
2. **Coefficients.** MF 6.1 has many more coefficients than the 117 of `mf5x_coefficients`. Add a new struct (for example `mf61_coefficients` plus `mf61_parameters`) in its own header instead of growing `mf5x_coefficients`; add its `validate_coefficients`-style checks (the required-key and value-check pattern of `mf5x_coefficients`: values the equations divide by). `operating_limits` and `model_metadata` are reused as they are. 6.x adds the pressure range (`PRESMIN`/`PRESMAX`), so `operating_limits` gets a pressure interval and a clamp flag for it.
3. **Model class.** Add `mf61_model final : public tire_model` (or a new abstract `mf6x_model` if 6.0, 6.1 and 6.2 share an evaluation order). Implement the private `compute` hook (called with validated, clamped inputs and Fz > 0) and `name()`; add a static `create` returning `std::expected<mf61_model, error>`. Reuse the kernels in `mf::detail` where the equations are unchanged, and add new kernels next to them. New kernels that compute one Magic Formula quantity are named after its symbol (cpp-conventions). If the model derives from `mf5x_model` instead, it can only override the existing hooks; that is the case only if the 6.x equations are expressed as differences from MF 5.2. `mf5x_model::compute` is final.
4. **Pressure input.** Pressure is not an input today: `operating_point` has no pressure field and the CLI ignores the P column. Add `pressure_pa` to `operating_point`, extend `tire_model::validate_input` (finite check) and the field table, and give `io::input_row` and the CLI an effective P column (the parser already reads it into `input_row` and sets `input_table::has_pressure`). Models that have no pressure input must keep ignoring it; the CLI warning is then limited to those models. Revisit the rule that pressure is not an input.
5. **Binding table.** Add a binding table in `io/` next to `mf5x_bindings` (for example `mf61_bindings.hpp/.cpp`) with the key, the member pointer and the `binding_role` for every key. The loader's binding step is table-driven, so it should only need to select the table by `model_kind`. Give 6.x its own "foreign keys" list if needed (compare `mf52_foreign_keys()`).
6. **Routing.** Add `MF61` (and `MF60`, `MF62` as they are implemented) to `io::model_kind`. In `identify_model`, remove the FITTYP rows 60 / 61 / 62 from the rule that returns "not implemented" (rule 3 in `model_routing.hpp`) and route them to the new kinds. FITTYP 70 (6.2 plus temperature and velocity) stays unsupported until the T&V model exists. Keep the order of the rules: an unknown PFF first, a non-integer FITTYP second. Update `model-routing.md`, the routing-order rule in `research/policies.md` section 3 and the README "Models" section in the same PR. Add the new kind to the model-creation step of `model_loader.cpp`, which today chooses between `create_pac2002` and `create_mf52` from `kind`.
7. **Tests to add.** Following the rules of this repository (100% line and branch on core, nothing skipped):
   - unit tests for each new kernel and for the coefficient validation (every branch);
   - unit tests for the new model with reference vectors whose origin is noted in `tests/data/` and `docs/research/test-data.md` (no licensed vendor data, see `policies.md` section 6);
   - the pipeline tests that already exist for `tire_model` (flags, Fz <= 0, clamping, finite check) instantiated for the new model;
   - routing tests: FITTYP 61 (and 60, 62) now route to the new model, FITTYP 70 still gives `UNSUPPORTED_MODEL`, PROPERTY_FILE_FORMAT precedence is unchanged;
   - loader tests for the binding table (required, defaulted, foreign keys) and for the pressure range;
   - a module test and a CLI test with a 6.x `.tir` file and a P column that is used; update the existing test that expects 6.x to be rejected.
8. **Docs.** Update this file, `CLAUDE.md` and the README "Models" section in the same PR.
