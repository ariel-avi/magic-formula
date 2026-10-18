# Architect decisions (AD-xx)

The user delegated the [policy] items that have no primary MSC/TNO source to the architect. This file records the architect's sign-off. Each decision is implemented as stated here. A primary MSC or TNO/Siemens source for any of them would trigger a change, and the change is recorded in this file. D-xx IDs are the researcher's decisions in README.md; where an AD-xx supersedes a D-xx or a rev-2 item, this file wins. The "Pinned by" column names the vectors in test-data.md or the test area that pins the decision.

## Decision table

| AD | Decision | Basis / supersedes | Pinned by |
|---|---|---|---|
| AD-01 | Fz <= 0 (incl. -0.0): all five outputs exactly 0. Checked on raw Fz before clamping. The same zeros are returned when a validated Fz interval clamps a positive Fz to <= 0 (FZMIN/FZMAX may end at or below zero): the `fz_clamped` flag is set and `compute` is not called (`src/model/tire_model.cpp`, `tire_model::evaluate`, the check after the clamps). | D-04 | V22, stub |
| AD-02 | Every input must be finite (INVALID_INPUT names the field). Vx < 0 is rejected for both models. PAC2002 accepts Vx = 0. MF 5.2 requires Vx > 0 (-0.0 rejected). | D-27 | M8, stub |
| AD-03 | sgn+ = +1 for x >= 0 (incl. -0.0). eps = 1e-6, applied as d + eps*sgn+(d) to: Cx*Dx and Cy*Dy (sign of the product; MFeval uses sgn(Dx), :788-791, identical when Cx > 0); Ky in SHf and Kx/Ky; Kx and Ky in the ellipse kappa_c/alpha_c (new, since MSC prints unguarded divisions); Vc in MF 5.2. | D-21, Q-08, Q-09 | kernel tests |
| AD-04 | Moment arm s normalised by FNOMIN, raw gamma, combined Fy. | D-09, Q-02 | `moment_arm` tests |
| AD-05 | VXLOW is not applied; default 1. Rows with Vx < VXLOW are flagged and the CLI warns once. | D-12, Q-03 | stub, cli |
| AD-06 | TYRESIDE LEFT/RIGHT (case-insensitive), absent = LEFT, no mirroring. RIGHT gives a warning (new). Any other value gives NOT_IMPLEMENTED. | Q-05 | loader |
| AD-07 | PAC2002 R*: all 20 present gives cosine; none gives friction ellipse plus a warning (new); a partial set gives MISSING_KEY listing the missing keys. A zero value counts as present. FE_METHOD gives a warning and is ignored (both models). | D-07, Q-06 | loader, V23 |
| AD-08 | MF 5.2 missing R* keys default to 0 and appear in the defaulted warning. No TNO estimation. | Q-15 | M9 |
| AD-09 | Pressure has no library input. A CLI P column is parsed and ignored with one warning. | D-25 | cli |
| AD-10 | [UNITS] are SI only, with no aliases: LENGTH 'meter', FORCE 'newton', ANGLE 'radians', MASS 'kg' ([UNITS] only), TIME 'second', PRESSURE 'pascal', compared case-insensitively. Absent = SI. Any other value gives UNSUPPORTED_UNIT. Other [UNITS] keys are ignored. Rev-2 'RAD' acceptance and 'missing [UNITS] is an error' are superseded. | D-13, Q-14 | U1-U3 |
| AD-11 | Routing order: an unknown PFF errors first; a non-integer FITTYP gives INVALID_VALUE; FITTYP 60/61/62/70 gives UNSUPPORTED_MODEL even with PFF 'PAC2002' (resolves the table overlap); then PAC2002; FITTYP 5 without PFF PAC2002, 21, 51 and 52 give UNSUPPORTED_MODEL (no opt-in). Amended: PFF 'USER' and 'MF-TYRE' (the TNO MF-Tyre ADAMS spellings, observed in the MFEVAL, gab-s98 and simscape files) are accepted and route by FITTYP exactly like an absent PFF; the known PFF set is PAC2002, MF_05, USER, MF-TYRE (model-routing.md, Q-13). | D-02, Q-13, Q-17 | routing tests |
| AD-12 | Required keys: PAC2002 {FNOMIN, UNLOADED_RADIUS}; MF 5.2 adds {FITTYP, WIDTH, RIM_RADIUS}. Critical keys: {PCX1, PDX1, PKX1, PCY1, PDY1, PKY1, PKY2}. Value checks: FNOMIN > 0, R0 > 0, LFZO > 0, plus (new) PKY2 != 0, LMUY != 0, LONGVL > 0. | tir-format section 3 | loader, coefficients |
| AD-13 | LONGVL is required iff QSY3 != 0 or QSY4 != 0; when absent and unused, an internal placeholder of 1 m/s. | Q-16 | loader |
| AD-14 | Defaults: L* = 1 silently; other P/Q/R/S = 0 with one warning; VXLOW = 1. | tir-format section 3 | loader |
| AD-15 | Clamp when both limits exist. A single limit gives a warning and is not applied (new). lower > upper gives INVALID_VALUE (new). Echoed inputs stay unclamped; flags are per row. If the Fz clamp leaves Fz <= 0 (a validated Fz interval that ends at or below zero), the five outputs are exactly 0 with `fz_clamped` set (plus any other flag already set) and `compute` is not called, as in AD-01 (`src/model/tire_model.cpp`, `tire_model::evaluate`). | D-03 | stub, V19 |
| AD-16 | Combined mode by default; uncombined via `slip_mode` or `--uncombined`. USE_MODE is echoed only, with a warning if < 0 or >= 10. Rev-2 USE_MODE errors are superseded. | D-26 | loader, P15 |
| AD-17 | My_out = -(MSC (87)) and -(88); (88) as printed, without LMY. The MF 5.2 form already carries the minus. | D-11, Q-10 | V5, V10, V21, M4 |
| AD-18 | MF 5.2 trail uses the shared R0/FNOMIN form, which equals MFeval's R0/F'z0*LFZO [derived]. | D-23 | M6 |
| AD-19 | `mf52_model::create` forces LGA* = 1. Present foreign keys (mf52.md section 3 list) give one warning and count as consumed. | D-14 | M3 |
| AD-20 | Lexical refinements to tir-format section 2, as listed below. | D-13 | document tests |
| AD-21 | Outputs are Fx, Fy, Mz, Mx and My only (no t, Mzr, Kx, Ky). | Q-20 | cli |
| AD-22 | A non-finite result gives NUMERICAL (output field and row named); NaN or inf is never printed. | new | stub, Fz 1e308 |
| AD-23 | Gxa and Gyk denominators are unguarded, as printed; pathological sets surface via AD-22. | MSC (66), (77) | - |
| AD-24 | Friction ellipse is computed in the Fz-free form (algebraically identical); a 0/0 ratio gives 1. | D-06 | P1, P2, P16 |
| AD-25 | MF 5.2: F'y = Fy - SVyk0, with mu_y at gamma = 0 and the other terms at the actual gamma. PAC2002: reading A. | D-16, MFeval :1587-1611 | M10 |
| AD-26 | The [impl] evidence tier for MF 5.2 is accepted (user decision). | Q-01 closed | - |
| AD-27 | CLI: table and CSV formats as documented in the README "CLI" section and the `mf::cli` header; exit codes 0/1/2; stdout is all-or-nothing. | new | cli |
| AD-28 | Fixture range values are synthetic (KPU +/-1.0, ALP +/-1.5, CAM +/-0.2, FZ 200..10000), not the MSC example values. | licence constraint | V19 |

## AD-20 lexical refinements

- **Encoding and lines:** the UTF-8 BOM is skipped; a NUL byte gives SYNTAX (this also covers UTF-16).
- **Sections:** content before the first section gives SYNTAX; a header with junk or an empty name gives SYNTAX.
- **Keys:** must match `[A-Za-z0-9_]+`.
- **Values:** unquoted values are allowed for string keys; a quoted value for a numeric key gives INVALID_VALUE.
- **Numbers:** `from_chars` general format plus one optional leading '+', finite only.
- **Integers:** FITTYP and USE_MODE accept integral values (6.0).
- **Tables:** a table section is one whose first content line starts with '{'; '=' inside a table gives SYNTAX; rows are not validated; a duplicate table section gives a warning and the first wins.
- **Unused keys:** shown only with `--verbose`.

## Open for primary evidence

These items are implemented per the AD-xx above. A primary MSC/TNO source would trigger a change.

- Q-02 (AD-04), Q-03 (AD-05), Q-05 (AD-06), Q-06 (AD-07), Q-10 (AD-17), Q-11, Q-12, Q-13 (AD-11), Q-15 (AD-08), Q-16 (AD-13): see open-questions.md.
- Q-07 (licences) is a user decision and gates the later validation against reference implementations and vendor data.
- The architect's direct MFeval reads (Q-18: `readTIR.m:200`; Q-19: `Solver.m:648-682` and `:1142-1146`) should be confirmed by a later researcher pass.
