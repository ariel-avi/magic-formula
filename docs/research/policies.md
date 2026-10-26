# Project policies

These are the project rules: behaviour that the equations alone do not fix (errors, warnings, defaults, numerical guards, choices between alternative forms). Each rule is implemented as stated here; where another note and this file differ, this file is what the code implements. Items that may still change are listed in open-questions.md. The "Tested by" text names the vectors in test-data.md or the test area that pins the rule.

## 1. Evaluation pipeline

- **Unloaded rows give zeros.** Fz zero, NaN or on the opposite side of FNOMIN (for FNOMIN > 0: Fz <= 0, incl. -0.0): all five outputs exactly 0. Checked on raw Fz before clamping. The same zeros are returned when a validated Fz interval leaves Fz at zero or on the side opposite FNOMIN (Fz*sgn(FNOMIN) <= 0) (FZMIN/FZMAX may end at or beyond zero): the `fz_clamped` flag is set and the equations run branch-free on every lane, with the five forces and moments of that point masked to exactly +0.0 by the contact mask (`detail::mf5x_pipeline` / `mf5x_condition`, `include/magic_formula/detail/mf5x_kernels.hpp`, the contact mask). Tested by: V22, pipeline.
- **Finite inputs and Vx.** Every input must be finite (INVALID_INPUT names the field; checked in the order FZ, SA, SR, IA, VX). Vx < 0 is rejected for both models. PAC2002 accepts Vx = 0. MF 5.2 requires Vx > 0 (-0.0 rejected). The Vx domain is data (`mf5x::PAC2002_INPUTS` / `MF52_INPUTS`), checked by `mf::validate_input(s)`. Tested by: M8, pipeline.
- **VXLOW.** VXLOW is not applied; default 1. Rows with Vx < VXLOW are flagged and the CLI warns once (open question, open-questions.md). Tested by: pipeline, cli.
- **Clamping.** Clamp when both limits exist. A single limit gives a warning and is not applied. lower > upper gives INVALID_VALUE. Echoed inputs stay unclamped; flags are per row. If the Fz clamp leaves Fz at zero or on the side opposite FNOMIN (Fz*sgn(FNOMIN) <= 0; a validated Fz interval that ends at or beyond zero), the five outputs are exactly 0 with `fz_clamped` set (plus any other flag already set); the equations run branch-free on every lane and the contact mask sets the five forces and moments of that point to exactly +0.0, as in the unloaded rule (`detail::mf5x_pipeline` / `mf5x_condition`, `include/magic_formula/detail/mf5x_kernels.hpp`). Tested by: pipeline (`tests/unit/mf5x_pipeline_test.cpp`), V19.
- **Slip modes and USE_MODE.** Combined mode by default; uncombined via `model_options::mode`, chosen at model construction (CLI: `--uncombined`). USE_MODE is echoed only, with a warning if < 0 or >= 10. Tested by: loader, P15.
- **Outputs.** The output vector carries Fx, Fy, Mz, Mx and My (no t, Mzr, Kx, Ky) plus the five 0/1 flag outputs FZ_CLAMPED..BELOW_VXLOW, which the CLI prints as columns. The characteristics are a separate 8-value vector per point (DFY_DSA, DFX_DSR, FY_OVER_SA, FX_OVER_SR, DY, DX, SA_AT_PEAK_FY, SR_AT_PEAK_FX; section 9), printed as 8 trailing CLI columns after the 15 existing ones. Settled (open-questions.md section 2). Tested by: cli.
- **Non-finite results.** Outputs: a non-finite result gives NUMERICAL (output field and row named); NaN or inf is never printed. Non-finite results go through `mf::check_outputs` (called by `mf::checked_evaluate_batch` in the CLI). Characteristics: NaN is allowed only in SA_AT_PEAK_FY and SR_AT_PEAK_FX and is printed `nan`; an infinite or any other non-finite characteristic gives NUMERICAL naming the channel (section 9). Tested by: pipeline, Fz 1e308, model_checks.

## 2. Numerics

- **sgn+ and epsilon.** sgn+ = +1 for x >= 0 (incl. -0.0). eps = 1e-6, applied as d + eps*sgn+(d) to: Cx*Dx and Cy*Dy (sign of the product); Ky in SHf and Kx/Ky; Kx and Ky in the ellipse kappa_c/alpha_c; Vc in MF 5.2. Tested by: kernel tests.
- **Moment arm s.** Moment arm s normalised by FNOMIN, raw gamma, combined Fy (pac2002.md section 5.1). Tested by: the `s` kernel tests (`S_*` in `tests/unit/mf5x_kernels_test.cpp`).
- **My output sign and the SHIFTS form.** My_out = -(QSY form) and -sgn(FNOMIN)*(SHIFTS form) (pac2002.md section 3); the SHIFTS form has no LMY factor. For FNOMIN > 0 this equals the earlier rule. The MF 5.2 form already carries the minus (mf52.md section 4, step 9; conventions.md section 4). Tested by: V5, V10, V21, M4.
- **MF 5.2 trail normaliser.** MF 5.2 trail uses the shared R0/FNOMIN form, which equals the MF 5.2 form R0/F'z0 with t multiplied by LFZO (pac2002.md section 5.6). Tested by: M6.
- **Gxa and Gyk denominators.** Gxa and Gyk denominators are unguarded: each is the cosine weighting at the shift value (SHxa, SHyk), which vanishes only for pathological coefficient sets; such sets surface as a non-finite result (section 1). Tested by: -.
- **Friction ellipse.** The friction ellipse is computed in the Fz-free form (algebraically identical for either sign of Fz); a 0/0 ratio gives 1 (pac2002.md section 5.4); the derivative there is the limit of characteristics.md section 2.5 (section 9). Tested by: P1, P2, P16.
- **MF 5.2 F'y.** MF 5.2: F'y = Fy - SVyk0, with mu_y at gamma = 0 and the other terms at the actual gamma (pac2002.md section 5.3). PAC2002 uses the actual-camber reading. Tested by: M10.

## 3. Loading a .tir file

- **TYRESIDE.** TYRESIDE LEFT/RIGHT (case-insensitive), absent = LEFT, no mirroring. RIGHT gives a warning. Any other value gives NOT_IMPLEMENTED (open question, open-questions.md). Tested by: loader.
- **PAC2002 R* keys and FE_METHOD.** PAC2002 R*: all 20 present gives cosine; none gives friction ellipse plus a warning; a partial set gives MISSING_KEY listing the missing keys. A zero value counts as present. FE_METHOD gives a warning and is ignored (both models) (pac2002.md section 5.5). Tested by: loader, V23.
- **MF 5.2 missing R* keys.** MF 5.2 missing R* keys default to 0 and appear in the defaulted warning. Missing coefficients are not estimated (open question, open-questions.md). Tested by: M9.
- **[UNITS].** [UNITS] are SI only, with no aliases: LENGTH 'meter', FORCE 'newton', ANGLE 'radians', MASS 'kg' ([UNITS] only), TIME 'second', PRESSURE 'pascal', compared case-insensitively. Absent = SI. Any other value gives UNSUPPORTED_UNIT. Other [UNITS] keys are ignored. 'RAD' is not accepted, and a missing [UNITS] is not an error. Tested by: U1-U3.
- **Routing order.** An unknown PROPERTY_FILE_FORMAT errors first; a non-integer FITTYP gives INVALID_VALUE; FITTYP 60/61/62/70 gives UNSUPPORTED_MODEL even with PFF 'PAC2002' (resolves the table overlap); then PAC2002; FITTYP 5 without PFF PAC2002, 21, 51 and 52 give UNSUPPORTED_MODEL (no opt-in). PFF 'USER' and 'MF-TYRE' (generic values that do not name a formulation) are accepted and route by FITTYP exactly like an absent PFF; the known PFF set is PAC2002, MF_05, USER, MF-TYRE (model-routing.md, open question in open-questions.md). Tested by: routing tests.
- **Required keys and value checks.** Required keys: PAC2002 {FNOMIN, UNLOADED_RADIUS}; MF 5.2 adds {FITTYP, WIDTH, RIM_RADIUS}. Critical keys: {PCX1, PDX1, PKX1, PCY1, PDY1, PKY1, PKY2}. Value checks: FNOMIN non-zero (its sign is the loaded side), R0 > 0, LFZO > 0, plus PKY2 != 0, LMUY != 0, LONGVL > 0 (tir-format.md section 3). Tested by: loader, coefficients.
- **LONGVL.** LONGVL is required iff QSY3 != 0 or QSY4 != 0; when absent and unused, an internal placeholder of 1 m/s (open question, open-questions.md). Tested by: loader.
- **Defaults.** L* = 1 silently; other P/Q/R/S = 0 with one warning; VXLOW = 1 (tir-format.md section 3). Tested by: loader.
- **MF 5.2 camber factors and foreign keys.** `mf52_model::create` forces LGA* = 1. Present foreign keys (mf52.md section 3 list) give one warning and count as consumed. Tested by: M3.

## 4. Lexical refinements

Refinements to tir-format.md section 2. Tested by: document tests.

- **Encoding and lines:** the UTF-8 BOM is skipped; a NUL byte gives SYNTAX (this also covers UTF-16).
- **Sections:** content before the first section gives SYNTAX; a header with junk or an empty name gives SYNTAX.
- **Keys:** must match `[A-Za-z0-9_]+`.
- **Values:** unquoted values are allowed for string keys; a quoted value for a numeric key gives INVALID_VALUE.
- **Numbers:** `from_chars` general format plus one optional leading '+', finite only.
- **Integers:** FITTYP and USE_MODE accept integral values (6.0).
- **Tables:** a table section is one whose first content line starts with '{'; '=' inside a table gives SYNTAX; rows are not validated; a duplicate table section gives a warning and the first wins.
- **Unused keys:** shown only with `--verbose`.

## 5. CLI

- **Pressure column.** Pressure has no library input. A CLI P column is parsed and ignored with one warning (README.md, Findings, Pressure row). Tested by: cli.
- **Formats, exit codes, stdout.** CLI: table and CSV formats as documented in the repository README.md, section "CLI" and the `mf::cli` header; exit codes 0/1/2; stdout is all-or-nothing. Tested by: cli.

## 6. Test data

- **Synthetic fixture ranges.** Fixture range values are synthetic (KPU +/-1.0, ALP +/-1.5, CAM +/-0.2, FZ 200..10000). Tested by: V19.
- **Reference outputs.** `tests/data/reference/mf52_mfeval.csv` (190 rows, computed on fixture S52) is kept in the repository; its origin and licence notice are in `tests/data/reference/README.md` and stay with the data. Tested by: `tests/module/mfeval_reference_test.cpp`.

## 7. Items that may change

These items are implemented per the rules above and may change; see open-questions.md for each: moment-arm normaliser; low-speed treatment (VXLOW, low-speed My taper); TYRESIDE mirroring; FE_METHOD semantics; PAC2002 My output sign; positive inclination direction; camber in mu_x; PROPERTY_FILE_FORMAT values; estimation of missing MF 5.2 coefficients; LONGVL when absent.

## 8. Coordinate systems

Rules where the sources are silent. The definitions, the flip tables and the identification rationale are in coordinate-systems.md. Tested by: coordinate conversion (property), identification, tir conversion, cli, Python and web tests.

- **User decision.** Evaluation never converts: the equations take raw inputs and are the same in every system. A model is converted between ISO and SAE only by flipping coefficient signs. The UI ISO/SAE toggle flips the model's coefficients. The convention sketch is a 3D view.
- **Canonical tables.** `mf5x::COEFFICIENT_SIGNS`: 15 VERTICAL coefficients (FNOMIN, PDX1, PDX2, PKX1, PKX2, PVX1, PVX2, QSX1, QSX2, QSX3, QSY2, SSZ1..SSZ4) and 15 LATERAL coefficients (PKY1, PHY1..PHY3, PEY3, PEY4, QHZ1..QHZ4, QEZ4, QEZ5, QBZ10, RBY3, RHX1). QBZ10, not QBZ9; PKY1, not PKY2; no L* ever flips. FZMIN/FZMAX (VERTICAL) and ALPMIN/ALPMAX (LATERAL) negate and swap.
- **Loaded side.** FNOMIN is non-zero and its sign is the loaded side. The unloaded rule (section 1) is relative to sgn(FNOMIN). `tire_model::loaded_side()` is SAE when the reference FZ is negative, else ISO.
- **Identification.** `tire_model::find_coordinate_system` is data-driven (`IDENTIFICATION_INDICATORS`: three VERTICAL, six LATERAL and two CHECK rows; weights and thresholds in coordinate-systems.md section 7.2). Load scales 0.5, 1 and 1.5 times F'z0 for swept rows; minimum valid weight 0.25 per axis; minimum confidence 0.6. Raw coefficient signs are never evidence. Clamped probes are skipped.
- **Fallback and MIXED.** An undetermined axis takes the other decided axis's sign; ISO when both are undetermined. (+,+) is ISO, (-,-) is SAE, anything else is MIXED with a warning and a recommended conversion. A MIXED model is not converted implicitly.
- **Warnings.** Undetermined axis (insufficient or conflicting evidence), dissent (naming the indicators), mixed, and CHECK failure (a negative dFx/dSR or Fx secant, which no conversion changes).
- **CLI defaults.** No conversion by default. `--system=iso|sae` converts the model by coefficient flips (only when its identified system differs). `--table-system=iso|sae` sets the system of the input table and of the printed rows; the default is the evaluated model's loaded side, so no data conversion. The warnings of the evaluated model's identification are always printed; the source model's identification summary and every reading only with `--verbose`. FZ rows on the unloaded side of the table system get one load-side warning naming the first row.
- **Data conversion.** FZ, FY, MZ and MY flip; SA, SR, IA, VX, FX, MX and the flags do not. The conversion is a utility (`convert_inputs`, `convert_outputs`) used at the boundary by the CLI, the UI and Python, never inside the evaluation.
- **Text conversion (`io::convert_tir`).** Values that are zero stay as written (no `-0`), so a converted text loads to coefficients equal to `transformed()` value for value, but a zero may be +0.0 where `transformed()` gives -0.0; a lone limit bound (the model ignores it) stays as written; a leading '+' is dropped on negation; a text that does not change is not recorded as a change. Limit pairs become (negated upper, negated lower).
- **UI.** On load the panel shows the identified ISO, SAE or Mixed label, the indicator table and the warnings. The toggle converts the model's coefficients (the parameter panel marks the changed entries and Save exports the converted text). Only FZ sweep values flip, and only when the VERTICAL transform was applied; SA never flips. A MIXED model has no toggle item pressed and the 3D scene shows the vertical side's frame.
- **Python.** `find_coordinate_system`, `convert_coordinate_system`, `convert_inputs` and `convert_outputs`; `evaluate_batch(inputs, *, check=True)` warns on rows on the unloaded side. The v1 `input_system` / `output_system` keywords are removed.

## 9. Characteristics

Rules for the eight characteristics (DFY_DSA, DFX_DSR, FY_OVER_SA, FX_OVER_SR, DY, DX, SA_AT_PEAK_FY, SR_AT_PEAK_FX) where the sources are silent. The definitions, closed forms and citations are in characteristics.md; the vectors are in test-data.md section 6. They are project policies, not cited results. Tested by: mf5x_characteristics, dual_lane, model_checks, tire_model, model_contract, coordinate_conversion, result_writer, cli (suites named per bullet).

- **Exposure and scope.** The characteristics are available for PAC2002 and MF 5.2 in the library (a separate 8-value vector per point, point and batch forms), in the CLI (8 trailing columns) and in the Python bindings. They apply in combined and in uncombined mode. Tested by: tire_model, model_contract, cli.
- **Analytic method.** The derivatives are obtained by exact forward-mode differentiation (dual numbers) of the closed-form kernels: every operation applies its exact derivative rule, there is no step size and no truncation error, and no finite difference is used anywhere. This counts as analytic and is the adopted method; the closed forms of characteristics.md section 2 are the test oracles. Tested by: dual_lane, mf5x_characteristics.
- **True derivative.** The derivatives are the true derivative of the evaluated force wherever it is differentiable, including at the friction-ellipse 0/0 points (the correct limit, never a convenience value). The force values themselves stay bit-identical to the evaluation path. One-sided values occur only at genuine kinks (the ellipse |own shifted slip| kink and the clamp bounds). Tested by: dual_lane, mf5x_characteristics.
- **Secants.** FY_OVER_SA and FX_OVER_SR are the evaluated force (clamped, contact-masked) divided by the raw slip of the row. At a raw slip of +-0 the secant is the row's tangent channel (DFY_DSA or DFX_DSR, which is 0 where the swept slip was clamped), so no secant is NaN. Rows without contact give exactly +0 in both secants, including at zero slip. Tested by: mf5x_characteristics, model_contract, cli.
- **Clamped inputs.** Where the swept input was clamped to ALPMIN/ALPMAX or KPUMIN/KPUMAX, the derivative is 0 (the chain rule through the clamp; the model output is constant there); at equality with the limit the interior derivative is used. The secant uses the clamped force and the raw slip. Tested by: mf5x_characteristics.
- **Peak slips.** The side is sgn+ of the row's raw slip (+-0 searches the positive side). The result is the first stationary point of the evaluated force with respect to its own slip, found moving from 0 outward on the segment [0, side*min(extent, max(0, side*bound))], with extent pi/2 rad for SA and 1 for SR and bound = ALPMAX/KPUMAX on the positive side and ALPMIN/KPUMIN on the negative side (infinite when there is no range). The search is a uniform scan of 32 cells (33 slope evaluations) for the first sign change of the analytic slope, then 52 bisection steps. In combined mode the other inputs are held at the clamped row and the swept slip is not clamped inside the segment. The first, not the global, stationary point is returned; two stationary points inside one cell (width |segment end|/32) are not resolved. Tested by: mf5x_characteristics.
- **NaN peaks.** SA_AT_PEAK_FY and SR_AT_PEAK_FX are NaN when the segment holds no stationary point (C <= 1, a monotone combined curve, a stationary point beyond the range, an empty segment) and for rows without contact. Tested by: mf5x_characteristics, model_checks.
- **Ellipse kinks and 0/0 points.** At a genuine kink of the friction ellipse (ratio denominator non-zero, own shifted slip +-0) the derivative is the right derivative, the sgn+ branch. At a 0/0 ratio the ratio value is the defined 1 (the force is the pure force) and the force derivative is F0'*r_lim, with r_lim = |P'|/sqrt(P'^2 + Q'^2) along the swept slip (P and Q are the own shifted slip times the other axis' peak and the own actual force times the other shifted slip), or 1 when P' = Q' = 0. For the own seeds rx_lim = mym/sqrt(mym^2 + (Fx0'*a*)^2) and ry_lim = mxm/sqrt(mxm^2 + (kc*Fy0')^2); where both shifted slips are 0 the ratio is identically 1 along the seed and the derivative is F0'. Derivation: characteristics.md section 2.5. Tested by: mf5x_characteristics, dual_lane.
- **DY and DX.** DY = mu_y*Fz and DX = mu_x*Fz at the clamped Fz and the scaled camber (gamma*LGAY and gamma*LGAX; LGA* = 1 for MF 5.2), contact-masked (0 without contact). They are the pure-slip D parameters in combined mode too. Tested by: mf5x_characteristics, tire_model.
- **Unloaded rows.** Rows without contact (section 1) give derivatives 0, DY = DX = 0, both secants +0 and both peaks NaN. Tested by: mf5x_characteristics, model_contract, cli.
- **Uncombined mode.** The characteristics are those of the pure-slip curves Fx0 and Fy0. Tested by: mf5x_characteristics, cli.
- **Vx.** Fx and Fy do not depend on Vx in MF 5.x, but the Vx domain is still validated as for the outputs (section 1). Tested by: tire_model, model_contract.
- **NaN, inf and printing.** NaN is a documented value only in SA_AT_PEAK_FY and SR_AT_PEAK_FX. Infinity is always a numerical failure (NUMERICAL naming the channel), including a secant at a subnormal slip with a non-zero force. The CLI prints NaN as `nan` (never `-nan`, also after a LATERAL conversion flips the sign bit of a NaN). Tested by: model_checks, result_writer, cli.
- **Coordinate conversion.** The channels convert by the sign parity of characteristics.md section 6 (`--table-system`, `convert_characteristics`). Tested by: coordinate_conversion, cli.
